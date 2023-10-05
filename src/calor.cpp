#include <vector>
#include <set>
#include <string>

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WebServer.h>
#include <uri/UriRegex.h>

#include <ArduinoJson.h>
#include <PicoMQTT.h>
#include <PicoUtils.h>
#include <WiFiManager.h>

#include <valve.h>

#include "zone.h"

PicoMQTT::Server & get_mqtt() {
    static PicoMQTT::Server mqtt;
    return mqtt;
}

PicoMQTT::Publisher & get_mqtt_publisher() {
    return get_mqtt();
}

PicoPrometheus::Registry & get_prometheus() {
    static PicoPrometheus::Registry prometheus;
    return prometheus;
}

PicoPrometheus::Gauge heating_demand(get_prometheus(), "heating_demand", "Burner heat demand state");

PicoUtils::PinInput<D1, false> button;
PicoUtils::ResetButton reset_button(button);

PicoUtils::PinOutput<D5, true> heating_relay;
PicoUtils::PinOutput<D6, true> valve_relay;

PicoUtils::PinOutput<D4, true> wifi_led;
PicoUtils::WiFiControl<WiFiManager> wifi_control(wifi_led);

std::vector<Zone> zones;
Valve local_valve(valve_relay, "built-in valve");

PicoUtils::RestfulServer<ESP8266WebServer> server(80);

const char CONFIG_FILE[] PROGMEM = "/config.json";

std::vector<Zone>::iterator find_zone_by_name(const String & name) {
    std::vector<Zone>::iterator it = zones.begin();
    while (it != zones.end() && name != it->get_name()) {
        ++it;
    }
    return it;
}

std::vector<Zone>::iterator find_zone_by_sensor(const String & sensor) {
    std::vector<Zone>::iterator it = zones.begin();
    while (it != zones.end() && it->sensor != sensor) {
        ++it;
    }
    return it;
}

DynamicJsonDocument get_config() {
    DynamicJsonDocument json(1024);

    auto zone_config = json["zones"].to<JsonObject>();
    for (const auto & zone : zones) {
        zone_config[zone.get_name()] = zone.get_config();
    }

    json["valve"] = local_valve.get_config();

    return json;
}

void setup_server() {

    server.on("/zones", HTTP_GET, [] {
        StaticJsonDocument<1024> json;

        for (const auto & zone : zones) {
            json[zone.get_name()] = zone.get_status();
        }

        server.sendJson(json);
    });

    server.on("/config", HTTP_GET, [] {
        server.sendJson(get_config());
    });

    server.on(UriRegex("/zones/([^/]+)"), HTTP_GET, [] {
        const String name = server.decodedPathArg(0).c_str();

        auto it = find_zone_by_name(name);

        if (it == zones.end())
            server.send(404);
        else {
            server.sendJson(it->get_status());
        }
    });

    get_prometheus().labels["module"] = "calor";

    get_prometheus().register_metrics_endpoint(server);

    server.begin();
}

void setup() {
    heating_relay.init();
    heating_relay.set(false);

    valve_relay.init();
    valve_relay.set(false);

    wifi_led.init();
    wifi_led.set(true);

    Serial.begin(115200);

    Serial.println(F("\n\n"
                     "  ___      _\n"
                     " / __|__ _| |___ _ _\n"
                     "| (__/ _` | / _ \\ '_|\n"
                     " \\___\\__,_|_\\___/_|\n"
                     "\n"
                     "Calor " __DATE__ " " __TIME__ "\n"
                     "\n\n"
                     "Press and hold button now to enter WiFi setup.\n"
                    ));

    delay(3000);
    reset_button.init();

    wifi_control.init(button, "calor");

    LittleFS.begin();

    {
        const auto config = PicoUtils::JsonConfigFile<StaticJsonDocument<1024>>(LittleFS, FPSTR(CONFIG_FILE));

        for (JsonPairConst kv : config["zones"].as<JsonObjectConst>()) {
            Zone zone(kv.key().c_str(), kv.value());
            zones.push_back(zone);
        }

        local_valve.set_config(config["valve"]);
    }

    setup_server();

    get_mqtt().subscribe("valvola/valve/+", [](const char * topic, const char * payload) {
        const auto zone_name = PicoMQTT::Subscriber::get_topic_element(topic, 2);

        if (!zone_name.length()) {
            return;
        }

        const auto valve_state = parse_valve_state(payload);

        auto it = find_zone_by_name(zone_name);
        if (it != zones.end()) {
            Serial.printf("Valve state update for zone %s: %s\n", zone_name.c_str(), to_c_str(valve_state));
            it->valve_state = valve_state;
        }
    });

    get_mqtt().subscribe("celsius/+/+", [](const char * topic, Stream & stream) {
        const auto sensor = PicoMQTT::Subscriber::get_topic_element(topic, 2);

        auto it = find_zone_by_sensor(sensor);
        if (it == zones.end()) {
            Serial.printf("Unrecognized sensor: %s\n", sensor.c_str());
            return;
        }

        StaticJsonDocument<512> json;
        if (deserializeJson(json, stream)) {
            // error
            return;
        }

        if (!json.containsKey("temperature")) {
            return;
        }

        const auto source = PicoMQTT::Subscriber::get_topic_element(topic, 1);
        const auto temperature = json["temperature"].as<double>();

        Serial.printf("Temperature update for zone %s: %.2f ºC\n", it->get_name(), temperature);
        it->update(source, temperature, json["rssi"].as<double>());
    });

    get_mqtt().subscribe("+/+/BTtoMQTT/+", [](const char * topic, Stream & stream) {
        StaticJsonDocument<512> json;

        if (deserializeJson(json, stream)) {
            // error
            return;
        }

        if (!json.containsKey("tempc")) {
            return;
        }

        const auto source = PicoMQTT::Subscriber::get_topic_element(topic, 1);

        const auto temperature = json["tempc"].as<double>();
        const auto sensor = json["id"] | "";

        auto it = find_zone_by_sensor(sensor);
        if (it != zones.end()) {
            Serial.printf("Temperature update for zone %s: %.2f ºC\n", it->get_name(), temperature);
            it->update(source, temperature, json["rssi"].as<double>());
        }
    });

    get_mqtt().begin();
}

PicoUtils::PeriodicRun local_valve_proc(1, 0, [] {
    auto it = find_zone_by_name(local_valve.get_name());
    if (it == zones.end()) {
        local_valve.demand_open = false;
        local_valve.tick();
        return;
    }

    local_valve.demand_open = it->valve_desired_state();
    local_valve.tick();

    it->valve_state = local_valve.get_state();
});

PicoUtils::PeriodicRun update_mqtt_proc(30, 15, [] {
    for (const auto & zone : zones) {
        zone.update_mqtt();
    }
    local_valve.update_mqtt();
});

PicoUtils::PeriodicRun heating_proc(5, 10, [] {
    bool boiler_on = false;
    printf("Checking %i zones...\n", zones.size());

    for (auto & zone : zones) {
        zone.tick();
        boiler_on = boiler_on || zone.boiler_desired_state();
        printf("  %16s:\t%5s\treading %5.2f ºC\t(updated %4lu s ago)\tdesired %5.2f ºC ± %3.2f ºC\n",
               zone.get_name(), to_c_str(zone.get_state()),
               (double) zone.get_reading(), zone.get_seconds_since_last_reading_update(),
               zone.desired, 0.5 * zone.hysteresis
              );
    };

    printf("Zone processing complete, heating: %s\n", boiler_on ? "ON" : "OFF");
    heating_relay.set(boiler_on);
    heating_demand.set(boiler_on);
});

void loop() {
    wifi_control.tick();
    server.handleClient();
    heating_proc.tick();
    local_valve.tick();
    local_valve_proc.tick();
    get_mqtt().loop();
    update_mqtt_proc.tick();
}
