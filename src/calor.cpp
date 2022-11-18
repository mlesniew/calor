#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <uri/UriRegex.h>

#include <utils/io.h>
#include <utils/stopwatch.h>
#include <utils/wifi_control.h>

#include "celsius_reader.h"
#include "heating.h"

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
    heating.set_reading(name, reading);
},
{"192.168.1.200"});

ESP8266WebServer server(80);

void setup_server() {
    server.on(UriRegex("/zones/([^/]+)/desired/([0-9]+([.][0-9]+)?)"), HTTP_POST, []{
            printf("Set zone %s desired temperature to %s\n", server.pathArg(0).c_str(), server.pathArg(1).c_str());
            const auto zone = server.pathArg(0);
            const auto value = server.pathArg(1).toDouble();
            if ((value > 30.0) || (value < 10.0)) {
                server.send(400, "text/plain", "Value out of bounds");
            } else if (heating.set_desired(zone.c_str(), value)) {
                server.send(200, "text/plain", "OK");
            } else {
                server.send(404, "text/plain", "Zone not found");
            }
        });

    server.on(UriRegex("/zones/([^/]+)/hysteresis/([0-9]+([.][0-9]+)?)"), HTTP_POST, []{
            printf("Set zone %s hysteresis to %s\n", server.pathArg(0).c_str(), server.pathArg(1).c_str());
            const auto zone = server.pathArg(0);
            const auto value = server.pathArg(1).toDouble();
            if ((value > 5.0) || (value < 0.0)) {
                server.send(400, "text/plain", "Value out of bounds");
            } else if (heating.set_hysteresis(zone.c_str(), value)) {
                server.send(200, "text/plain", "OK");
            } else {
                server.send(404, "text/plain", "Zone not found");
            }
        });

    server.begin();
}

void setup() {
    Serial.begin(9600);

    wifi_led.init();
    status_led.init();

    const auto mode = button ? WiFiInitMode::setup : WiFiInitMode::saved;
    wifi_control.init(mode, "calor");

    setup_server();
}

void loop() {
    server.handleClient();
    wifi_control.tick();
    celsius_reader.tick();
    heating.tick();
}
