#include <map>
#include <set>
#include <string>

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <uri/UriRegex.h>

#include <ArduinoJson.h>

#include <utils/io.h>
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

PinOutput<D4, true> wifi_led;
PinOutput<D5, true> status_led;

PinOutput<D6, false> heating_relay;
PinOutput<D7, false> valve_relay;
DummyOutput other_relay;

WiFiControl wifi_control(wifi_led);

std::map<std::string, Zone> zones;
std::set<std::string> celsius_addresses = {"192.168.1.200"};
std::set<std::string> valvola_addresses = {"192.168.1.230"};

ESP8266WebServer server(80);

Valve local_valve(valve_relay, "built-in valve");

void setup_server() {

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
            server.send(409, "text/plain", "Zone exists");
            return;
        } else if (!zone_exists && zone_should_exist) {
            server.send(404, "text/plain", "No such zone");
            return;
        }

        // parse data if needed
        const bool parse_upload = ((server.method() == HTTP_POST) || (server.method() == HTTP_PUT));
        Zone parsed_zone;
        if (parse_upload) {
            StaticJsonDocument<64> json;
            const auto error = deserializeJson(json, server.arg("plain"));

            if (error) {
                server.send(400, "text/plain", error.f_str());
                return;
            }

            if (!parsed_zone.load(json.as<JsonVariant>())) {
                server.send(400, "text/plain", "data invalid");
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
                server.send(200, "text/plain", "zone deleted");
                return;
            case HTTP_GET:
                break;
            default:
                server.send(405, "text/plain", "invalid request");
                return;
        }

        // return zone
        String output;
        serializeJson(it->second.to_json(), output);
        server.send(200, "application/json", output);
    });

    server.on(UriRegex("/zones/([^/]+)/(desired|hysteresis|reading)"), HTTP_GET, [] {
        const std::string name = uri_unquote(server.pathArg(0).c_str());
        const auto it = zones.find(name);

        if (it == zones.end()) {
            server.send(404, "text/plain", "No such zone");
            return;
        }

        const Zone & zone = it->second;

        double value;
        switch (server.pathArg(1).c_str()[0]) {
            case 'd':
                value = zone.desired;
                break;
            case 'h':
                value = zone.hysteresis;
                break;
            case 'r':
                value = zone.reading;
                break;
        }

        server.send(200, "text/plain", String(value));
    });

    server.on(UriRegex("/zones/([^/]+)/(desired|hysteresis|reading)/([0-9]+([.][0-9]+)?)"), HTTP_POST, [] {
        const std::string name = uri_unquote(server.pathArg(0).c_str());

        auto it = zones.find(name);
        if (it == zones.end()) {
            server.send(404, "text/plain", "No such zone");
            return;
        }

        Zone & zone = it->second;

        const auto value = server.pathArg(2).toDouble();

        switch (server.pathArg(1).c_str()[0]) {
            case 'd':
                if ((value < 0) || (value > 30.0)) {
                    server.send(400, "text/plain", "Value out of bounds");
                } else {
                    zone.desired = value;
                    server.send(200, "text/plain", "OK");
                }
                return;

            case 'h':
                if ((value < 0) || (value > 5)) {
                    server.send(400, "text/plain", "Value out of bounds");
                } else {
                    zone.hysteresis = value;
                    server.send(200, "text/plain", "OK");
                }
                return;
        }

        server.send(400, "text/plain", "Bad request");
    });

    server.begin();
}

void setup() {
    Serial.begin(9600);

    wifi_led.init();
    status_led.init();
    reset_button.init();

    const auto mode = button ? WiFiInitMode::setup : WiFiInitMode::saved;
    wifi_control.init(mode, "calor");

    setup_server();

    zones["Living room"];
    zones["Bedroom"];
}

PeriodicRun celsius_proc(60, 3, [] {
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

PeriodicRun local_valve_proc(60, 5, [] {
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
    String output;
    DynamicJsonDocument json(1024);
    for (auto & kv : zones) {
        const auto & name = kv.first;
        auto & zone = kv.second;
        json[name] = zone.valve_desired_state();
    }
    serializeJson(json, output);

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

PeriodicRun heating_proc(10, 4, [] {
    bool boiler_on = false;
    printf("Checking %i zones...\n", zones.size());

    for (auto & kv : zones) {
        const auto & name = kv.first;
        auto & zone = kv.second;
        zone.tick();
        boiler_on = boiler_on || zone.boiler_desired_state();
        printf("  %s: %s  reading %.2f ºC; desired %.2f ºC ± %.2f ºC\n",
               name.c_str(), zone.boiler_desired_state() ? "ON" : "OFF",
               (double) zone.reading, zone.desired, zone.hysteresis
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
