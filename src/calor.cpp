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

#include "celsius.h"
#include "zone.h"

PicoMQTT::Server & get_mqtt() {
    static PicoMQTT::Server mqtt;
    return mqtt;
}

PicoMQTT::Publisher & get_mqtt_publisher() {
    return get_mqtt();
}

Prometheus & get_prometheus() {
    static Prometheus prometheus;
    return prometheus;
}

PrometheusGauge heating_demand(get_prometheus(), "heating_demand", "Burner heat demand state");

PicoUtils::PinInput<D1, false> button;
PicoUtils::ResetButton reset_button(button);

PicoUtils::PinOutput<D5, true> heating_relay;
PicoUtils::PinOutput<D6, true> valve_relay;

PicoUtils::PinOutput<D4, true> wifi_led;
PicoUtils::WiFiControl<WiFiManager> wifi_control(wifi_led);

std::vector<Zone> zones;
std::set<std::string> celsius_addresses;
Valve local_valve(valve_relay, "built-in valve");

PicoUtils::RestfulServer<ESP8266WebServer> server(80);

const char CONFIG_FILE[] PROGMEM = "/config.json";

std::vector<Zone>::iterator find_zone_by_name(const std::string & name) {
    std::vector<Zone>::iterator it = zones.begin();
    while (it != zones.end() && it->get_name() != name) {
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

    auto celsius_config = json["celsius"].to<JsonArray>();
    for (const auto & address : celsius_addresses) {
        celsius_config.add(address);
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

    server.on("/config/save", HTTP_POST, [] {
        const auto json = get_config();
        File f = LittleFS.open(FPSTR(CONFIG_FILE), "w");
        if (!f) {
            server.send(500);
            return;
        }
        serializeJson(json, f);
        f.close();
        server.send(200);
    });

    server.on(UriRegex("/config/celsius/([^/]+)"), [] {
        const std::string name = server.decodedPathArg(1).c_str();

        switch (server.method()) {
            case HTTP_POST:
            case HTTP_PUT:
                celsius_addresses.insert(name);
                server.send(200);
                return;
            case HTTP_DELETE:
                celsius_addresses.erase(name);
                server.send(200);
                return;
            default:
                server.send(405);
                return;
        }
    });

    server.on("/config/valve", [] {
        switch (server.method()) {
            case HTTP_PUT:
            case HTTP_POST:
            case HTTP_PATCH: {
                StaticJsonDocument<128> json;

                const auto error = deserializeJson(json, server.arg("plain"));
                if (error) {
                    server.send(400);
                    return;
                }

                if (!local_valve.set_config(json.as<JsonVariant>())) {
                    server.send(400);
                    return;
                }
            }

            // fall through
            case HTTP_GET: {
                server.sendJson(local_valve.get_config());
                return;
            }

            default:
                server.send(405);
                return;
        }
    });

    server.on(UriRegex("/zones/([^/]+)"), [] {
        const std::string name = server.decodedPathArg(0).c_str();

        auto it = find_zone_by_name(name);

        // check for conflicts
        const bool zone_exists = (it != zones.end());
        const bool zone_should_exist = (
            (server.method() == HTTP_GET) ||
            (server.method() == HTTP_PUT) ||
            (server.method() == HTTP_DELETE));

        if (zone_exists && !zone_should_exist) {
            server.send(409);
            return;
        } else if (!zone_exists && zone_should_exist) {
            server.send(404);
            return;
        }

        // parse data if needed
        const bool parse_upload = ((server.method() == HTTP_POST) || (server.method() == HTTP_PUT));
        Zone parsed_zone(name.c_str());
        if (parse_upload) {
            StaticJsonDocument<64> json;
            const auto error = deserializeJson(json, server.arg("plain"));

            if (error) {
                server.send(400);
                return;
            }

            if (!parsed_zone.set_config(json.as<JsonVariant>())) {
                server.send(400);
                return;
            }
        }

        // perform action
        switch (server.method()) {
            case HTTP_POST:
                zones.push_back(parsed_zone);
                it = zones.end() - 1;
                break;
            case HTTP_PUT:
                it->copy_config_from(parsed_zone);
                break;
            case HTTP_DELETE:
                zones.erase(it);
                server.send(200);
                return;
            case HTTP_GET:
                break;
            default:
                server.send(405);
                return;
        }

        it->tick();
        server.sendJson(it->get_status());
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

        for (JsonVariantConst v : config["celsius"].as<JsonArrayConst>()) {
            const auto addr = v.as<std::string>();
            if (!addr.empty()) {
                celsius_addresses.insert(addr);
            }
        }

        for (JsonPairConst kv : config["zones"].as<JsonObjectConst>()) {
            Zone zone(kv.key().c_str());
            zone.set_config(kv.value());
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

        auto it = find_zone_by_name(zone_name.c_str());
        if (it != zones.end()) {
            Serial.printf("Valve state update for zone %s: %s\n", zone_name.c_str(), to_c_str(valve_state));
            it->valve_state = valve_state;
        }
    });

    get_mqtt().begin();
}

PicoUtils::PeriodicRun celsius_proc(60, 5, [] {
    for (const auto & address : celsius_addresses) {
        const auto readings = get_celsius_readings(address);
        for (const auto & kv : readings) {
            const auto & name = kv.first;
            const double reading = kv.second;
            printf("Temperature in %s = %.2f ºC\n", name.c_str(), reading);
            auto it = find_zone_by_name(name);
            if (it != zones.end()) {
                it->reading = reading;
            }
        }
    }
});

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

PicoUtils::PeriodicRun heating_proc(1, 10, [] {
    bool boiler_on = false;
    printf("Checking %i zones...\n", zones.size());

    for (auto & zone : zones) {
        zone.tick();
        boiler_on = boiler_on || zone.boiler_desired_state();
        printf("  %s:\t%s\treading %.2f ºC\tdesired %.2f ºC ± %.2f ºC\n",
               zone.get_name(), to_c_str(zone.get_state()),
               (double) zone.reading, zone.desired, 0.5 * zone.hysteresis
              );
    };

    printf("Zone processing complete, heating: %s\n", boiler_on ? "ON" : "OFF");
    heating_relay.set(boiler_on);
    heating_demand.set(boiler_on);
});

void loop() {
    wifi_control.tick();
    server.handleClient();
    celsius_proc.tick();
    heating_proc.tick();
    local_valve.tick();
    local_valve_proc.tick();
    get_mqtt().loop();
    update_mqtt_proc.tick();
}
