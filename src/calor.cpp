#include <Arduino.h>

#include <utils/io.h>
#include <utils/wifi_control.h>

PinInput<D1, false> button;

PinOutput<D4, true> wifi_led;
PinOutput<D5, true> status_led;

PinOutput<D6, false> heating_relay;
PinOutput<D7, false> valve_relay;

WiFiControl wifi_control(wifi_led);

void setup() {
    wifi_led.init();
    status_led.init();

    const auto mode = button ? WiFiInitMode::setup : WiFiInitMode::saved;
    wifi_control.init(mode, "calor");
}

void loop() {
    wifi_control.tick();
}
