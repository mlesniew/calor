#include <Arduino.h>

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

void setup() {
    Serial.begin(9600);

    wifi_led.init();
    status_led.init();

    const auto mode = button ? WiFiInitMode::setup : WiFiInitMode::saved;
    wifi_control.init(mode, "calor");
}

void loop() {
    wifi_control.tick();
    celsius_reader.tick();
    heating.tick();
}
