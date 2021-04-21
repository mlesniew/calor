#include <Arduino.h>
#include <LittleFS.h>
#include <OneWire.h>
#include <LiquidCrystal_I2C.h>

#include <utils/led.h>
#include <utils/shift_register.h>
#include <utils/wifi_control.h>

#include "buttons.h"
#include "menu.h"
#include "clock.h"
#include "wifi_readings.h"
#include "temperature_sensor_controller.h"
#include "hot_water_controller.h"

BlinkingLed wifi_led(D4, 0, 91, true);
WiFiControl wifi_control(wifi_led);
WiFiReadings wifi_readings;
ShiftRegister<1> shift_register(
    D6,  // data pin
    D5,  // clock pin
    D0,  // latch pin
(uint8_t[1]) {
    0b00001111,
}  // inverted outputs
);

OneWire one_wire(D7);
TemperatureSensorController<4, 12> temperature_sensor_controller(one_wire);

Clock ntp_clock;

HotWaterController hot_water_program(
    ntp_clock,
    temperature_sensor_controller,
    [](bool value) {
        shift_register.write(3, value);
    },
    2.5);

LiquidCrystal_I2C lcd(0x27, 20, 4);
Buttons buttons;
Menu menu;

void setup() {
    Serial.begin(9600);
    Serial.println(F("Calor " __DATE__ " " __TIME__));

    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print(F("Calor"));
    lcd.setCursor(0, 1);
    lcd.print(F("    " __DATE__));
    lcd.setCursor(0, 2);
    lcd.print(F("    " __TIME__));

    shift_register.init();
    temperature_sensor_controller.init();
    LittleFS.begin();

    wifi_control.init();
    ntp_clock.init();

    hot_water_program.init();
    hot_water_program.add(Time{7, 0}, 40);
    hot_water_program.add(Time{8, 30}, 30);
    hot_water_program.add(Time{17, 30}, 45);
    hot_water_program.add(Time{20, 30}, 0);

    lcd.clear();

    menu.add_item("Time", []() {
            if (!ntp_clock.ready()) {
                return std::string("??:??");
            }
            const auto time = ntp_clock.get_time();
            char text[6];
            snprintf(text, 6, "%02i:%02i", time.get_hours(), time.get_minutes());
            return std::string(text);
        });
    menu.add_item("Current", []() { return hot_water_program.get_reading(); });
    menu.add_item("Desired", []() { return hot_water_program.get_desired(); });
    menu.add_item_on_off("Heating", []() { return hot_water_program.get_state() == HotWaterController::State::on; });

    menu.add_item("IP", []() { return std::string(WiFi.localIP().toString().c_str()); });
}

void loop() {
    wifi_control.tick();
    ntp_clock.tick();

    // Serial.println(WiFi.localIP());
    // wifi_readings.load("http://192.168.1.200/measurements.json");

    temperature_sensor_controller.tick();
    hot_water_program.tick();

#if 0
    static Stopwatch stopwatch;
    if (stopwatch.elapsed() > 1) {
        stopwatch.reset();

        lcd.setCursor(0, 0);
        lcd.print("Water  ");

        lcd.setCursor(0, 1);
        lcd.printf("Temperature %6.1f\337C", hot_water_program.get_reading());

        lcd.setCursor(0, 2);
        lcd.printf("Desired     %6.1f\337C", hot_water_program.get_desired());

        lcd.setCursor(15, 0);
        if (!ntp_clock.ready()) {
            lcd.print("??:??");
        } else {
            const auto time = ntp_clock.get_time();
            lcd.printf("%02i:%02i", time.get_hours(), time.get_minutes());
        }

        lcd.setCursor(0, 3);
        switch(hot_water_program.get_state()) {
            case HotWaterController::State::on:
                lcd.print(F("Heating on "));
                break;

            case HotWaterController::State::off:
                lcd.print(F("Heating off"));
                break;

            case HotWaterController::State::invalid:
                lcd.print(F("Error      "));
                break;
        }
    }
#endif
    buttons.tick();
    menu.tick(lcd, buttons);
}
