#include <Arduino.h>

#include <utils/io.h>
#include <utils/stopwatch.h>
#include <utils/wifi_control.h>

#include "celsius.h"
#include "heating.h"

constexpr auto celsius_address = "192.168.1.200";

PinInput<D1, true> button;

PinOutput<D4, true> wifi_led;
PinOutput<D5, true> status_led;

PinOutput<D6, false> heating_relay;
PinOutput<D7, false> valve_relay;
DummyOutput other_relay;

WiFiControl wifi_control(wifi_led);

Stopwatch stopwatch;

Heating heating = { "Salon", "Piętro", "Strych" };


void setup() {
    Serial.begin(9600);

    wifi_led.init();
    status_led.init();

    const auto mode = button ? WiFiInitMode::setup : WiFiInitMode::saved;
    wifi_control.init(mode, "calor");
}

void loop() {
    wifi_control.tick();
    heating.tick();

    if (stopwatch.elapsed() >= 10) {
        const auto readings = get_celsius_readings(celsius_address);
        for (const auto & kv : readings) {
            printf("Temperature in %s = %f\n", kv.first.c_str(), kv.second);
            heating.set_reading(kv.first, kv.second);
        }

        stopwatch.reset();
    }
}
