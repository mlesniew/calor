#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <uri/UriRegex.h>

#include <ArduinoJson.h>

#include <utils/io.h>
#include <utils/stopwatch.h>
#include <utils/wifi_control.h>

#include "celsius_reader.h"
#include "heating.h"
#include "utils.h"

PinInput<D1, true> button;

PinOutput<D4, true> wifi_led;
PinOutput<D5, true> status_led;

PinOutput<D6, false> heating_relay;
PinOutput<D7, false> valve_relay;
DummyOutput other_relay;

WiFiControl wifi_control(wifi_led);

Heating heating = { "Salon", "Piętro", "Strych" };

CelsiusReader celsius_reader(
[](const std::string & name, double reading) {
    Zone * zone = heating.get(name);
    if (zone) {
        zone->reading = reading;
    }
},
{"192.168.1.200"});

ESP8266WebServer server(80);

void setup_server() {

    server.on(UriRegex("/zones/([^/]+)"), [] {
        const std::string zone_name = uri_unquote(server.pathArg(0).c_str());

        auto it = heating.zones.find(zone_name);

        // check for conflicts
        const bool zone_exists = (it != heating.zones.end());
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
                it = heating.zones.insert({zone_name, parsed_zone}).first;
                break;
            case HTTP_PUT:
                it->second = parsed_zone;
                break;
            case HTTP_DELETE:
                heating.zones.erase(it);
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

        Zone * zone = heating.get(uri_unquote(server.pathArg(0).c_str()));
        if (!zone) {
            server.send(404, "text/plain", "No such zone");
            return;
        }

        double value;
        switch (server.pathArg(1).c_str()[0]) {
        case 'd':
            value = zone->desired;
            break;
        case 'h':
            value = zone->hysteresis;
            break;
        case 'r':
            value = zone->reading;
            break;
        }

        server.send(200, "text/plain", String(value));
    });

    server.on(UriRegex("/zones/([^/]+)/(desired|hysteresis|reading)/([0-9]+([.][0-9]+)?)"), HTTP_POST, [] {

        Zone * zone = heating.get(uri_unquote(server.pathArg(0).c_str()));
        if (!zone) {
            server.send(404, "text/plain", "No such zone");
            return;
        }

        const auto value = server.pathArg(2).toDouble();

        switch (server.pathArg(1).c_str()[0]) {
        case 'd':
            if ((value < 0) || (value > 30.0)) {
                server.send(400, "text/plain", "Value out of bounds");
            } else {
                zone->desired = value;
                server.send(200, "text/plain", "OK");
            }
            return;

        case 'h':
            if ((value < 0) || (value > 5)) {
                server.send(400, "text/plain", "Value out of bounds");
            } else {
                zone->hysteresis = value;
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

    const auto mode = WiFiInitMode::saved; // button ? WiFiInitMode::setup : WiFiInitMode::saved;
    wifi_control.init(mode, "calor");

    setup_server();
}

void loop() {
    server.handleClient();
    wifi_control.tick();
    celsius_reader.tick();
    heating.tick();
}
