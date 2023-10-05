#include <memory>
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

std::vector<std::unique_ptr<Zone>> zones;
Valve * local_valve = nullptr;

PicoUtils::RestfulServer<ESP8266WebServer> server(80);

const char CONFIG_FILE[] PROGMEM = "/config.json";

Zone * find_zone_by_name(const String & name) {
    for (auto & zone_ptr : zones) {
        if (name == zone_ptr->name) {
            return zone_ptr.get();
        }
    }
    return nullptr;
}

DynamicJsonDocument get_config() {
    DynamicJsonDocument json(1024);

    auto zone_config = json["zones"].to<JsonObject>();
    for (const auto & zone : zones) {
        zone_config[zone->name] = zone->get_config();
    }

    json["valve"] = local_valve->get_config();

    return json;
}

void setup_server() {

    server.on("/zones", HTTP_GET, [] {
        StaticJsonDocument<1024> json;

        for (const auto & zone : zones) {
            json[zone->name] = zone->get_status();
        }

        server.sendJson(json);
    });

    server.on("/config", HTTP_GET, [] {
        server.sendJson(get_config());
    });

    server.on(UriRegex("/zones/([^/]+)"), HTTP_GET, [] {
        const String name = server.decodedPathArg(0).c_str();

        Zone * zone = find_zone_by_name(name);

        if (!zone) {
            server.send(404);
        } else {
            server.sendJson(zone->get_status());
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
            zones.push_back(std::unique_ptr<Zone>(new Zone(kv.key().c_str(), kv.value())));
        }

        local_valve = new Valve(valve_relay, config["valve"]);
    }

    setup_server();

    get_mqtt().begin();
}

PicoUtils::PeriodicRun local_valve_proc(1, 0, [] {
    Zone * zone = find_zone_by_name(local_valve->name);
    if (!zone) {
        local_valve->demand_open = false;
        local_valve->tick();
        return;
    }

    local_valve->demand_open = zone->valve_desired_state();
    local_valve->tick();
    zone->valve_state = local_valve->get_state();
});

PicoUtils::PeriodicRun heating_proc(5, 10, [] {
    bool boiler_on = false;
    printf("Checking %i zones...\n", zones.size());

    for (auto & zone : zones) {
        boiler_on = boiler_on || zone->boiler_desired_state();
        printf("  %16s:\t%5s\treading %5.2f ºC\t(updated %4lu s ago)\tdesired %5.2f ºC ± %3.2f ºC\n",
               zone->name.c_str(), to_c_str(zone->get_state()),
               (double) zone->reading, zone->get_seconds_since_last_reading_update(),
               zone->desired, 0.5 * zone->hysteresis
              );
    };
    printf("Zone processing complete, heating: %s\n", boiler_on ? "ON" : "OFF");

    heating_relay.set(boiler_on);
    heating_demand.set(boiler_on);
});

void loop() {
    wifi_control.tick();
    server.handleClient();
    {
        for (auto & zone : zones) { zone->tick(); }
        heating_proc.tick();
        local_valve->tick();
        local_valve_proc.tick();
    }
    get_mqtt().loop();
}
