#include <map>
#include <set>
#include <string>

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <uri/UriRegex.h>

#include <ArduinoJson.h>

#include <utils/io.h>
#include <utils/json_config.h>
#include <utils/periodic_run.h>
#include <utils/reset_button.h>
#include <utils/stopwatch.h>
#include <utils/wifi_control.h>

#include "celsius.h"
#include "utils.h"
#include "valve.h"
#include "valvola.h"
#include "zone.h"

PinInput<D1, false> button;
ResetButton reset_button(button);

PinOutput<D5, true> heating_relay;
PinOutput<D6, true> valve_relay;

PinOutput<D4, true> wifi_led;
WiFiControl wifi_control(wifi_led);

std::map<std::string, Zone> zones;
std::set<std::string> celsius_addresses;
std::set<std::string> valvola_addresses;
Valve local_valve(valve_relay, "built-in valve");

ESP8266WebServer server(80);

const char CONFIG_FILE[] PROGMEM = "/config.json";

void return_json(const JsonDocument & json, unsigned int code = 200) {
    String output;
    serializeJson(json, output);
    server.send(code, F("application/json"), output);
}

DynamicJsonDocument get_config() {
    DynamicJsonDocument json(1024);

    auto zone_config = json["zones"].to<JsonObject>();
    for (auto & kv : zones) {
        zone_config[kv.first] = kv.second.get_config();
    }

    auto valvola_config = json["valvola"].to<JsonArray>();
    for (const auto & address : valvola_addresses) {
        valvola_config.add(address);
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

        for (auto & kv : zones)
            json[kv.first] = kv.second.get_status();

        return_json(json);
    });

    server.on("/config", HTTP_GET, [] {
        return_json(get_config());
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

    server.on(UriRegex("/config/(celsius|valvola)/([^/]+)"), [] {
        std::set<std::string> & addresses = server.pathArg(0).c_str()[0] == 'c' ? celsius_addresses : valvola_addresses;
        const std::string name = uri_unquote(server.pathArg(1).c_str());

        switch (server.method()) {
            case HTTP_POST:
            case HTTP_PUT:
                addresses.insert(name);
                server.send(200);
                return;
            case HTTP_DELETE:
                addresses.erase(name);
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
                return_json(local_valve.get_config());
                return;
            }

            default:
                server.send(405);
                return;
        }
    });

    server.on(UriRegex("/zones/([^/]+)"), [] {
        const std::string name = uri_unquote(server.pathArg(0).c_str());

        auto it = zones.find(name);

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
        Zone parsed_zone;
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
                it = zones.insert({name, parsed_zone}).first;
                break;
            case HTTP_PUT:
                it->second = parsed_zone;
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

        return_json(it->second.get_status());
    });

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

    wifi_control.init(button ? WiFiInitMode::setup : WiFiInitMode::saved, "calor");

    LittleFS.begin();

    {
        const auto config = JsonConfigFile(LittleFS, FPSTR(CONFIG_FILE), 1024);

        for (JsonVariantConst v : config["celsius"].as<JsonArrayConst>()) {
            const auto addr = v.as<std::string>();
            if (!addr.empty()) {
                celsius_addresses.insert(addr);
            }
        }

        for (JsonVariantConst v : config["valvola"].as<JsonArrayConst>()) {
            const auto addr = v.as<std::string>();
            if (!addr.empty()) {
                valvola_addresses.insert(addr);
            }
        }

        for (JsonPairConst kv : config["zones"].as<JsonObjectConst>()) {
            zones[kv.key().c_str()].set_config(kv.value());
        }

        local_valve.set_config(config["valve"]);
    }

    setup_server();
}

PeriodicRun celsius_proc(60, 5, [] {
    for (const auto & address : celsius_addresses) {
        const auto readings = get_celsius_readings(address);
        for (const auto & kv : readings) {
            const auto & name = kv.first;
            const double reading = kv.second;
            printf("Temperature in %s = %.2f ºC\n", name.c_str(), reading);
            auto it = zones.find(name);
            if (it != zones.end()) {
                it->second.reading = reading;
            }
        }
    }
});

PeriodicRun local_valve_proc(5, 0, [] {
    auto it = zones.find(local_valve.name);
    if (it == zones.end()) {
        local_valve.demand_open = false;
        local_valve.tick();
        return;
    }

    auto & zone = it->second;

    local_valve.demand_open = zone.valve_desired_state();
    local_valve.tick();

    zone.valve_state = local_valve.get_state();
});

PeriodicRun valvola_proc(60, 5, [] {
    for (const auto & address : valvola_addresses) {
        std::map<std::string, bool> desired_valve_states;
        for (auto & kv : zones) {
            desired_valve_states[kv.first] = kv.second.valve_desired_state();
        }

        const auto valve_states = update_valvola(address, desired_valve_states);
        for (const auto & kv : valve_states) {
            const auto & name = kv.first;
            const auto state = kv.second;
            printf("Valve state %s = %s\n", name.c_str(), to_c_str(state));
            auto it = zones.find(name);
            if (it != zones.end()) {
                it->second.valve_state = state;
            }
        }
    }

});

PeriodicRun heating_proc(10, 10, [] {
    bool boiler_on = false;
    printf("Checking %i zones...\n", zones.size());

    for (auto & kv : zones) {
        const auto & name = kv.first;
        auto & zone = kv.second;
        zone.tick();
        boiler_on = boiler_on || zone.boiler_desired_state();
        printf("  %s:\t%s\treading %.2f ºC; desired %.2f ºC ± %.2f ºC\n",
               name.c_str(), to_c_str(zone.get_state()),
               (double) zone.reading, zone.desired, 0.5 * zone.hysteresis
              );
    };

    printf("Zone processing complete, heating: %s\n", boiler_on ? "ON" : "OFF");
    heating_relay.set(boiler_on);
});

void loop() {
    server.handleClient();
    wifi_control.tick();
    celsius_proc.tick();
    heating_proc.tick();
    valvola_proc.tick();
    local_valve.tick();
    local_valve_proc.tick();
}
