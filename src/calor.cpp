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


Menu water_menu = {
    MenuItem::value("Current", []() { return hot_water_program.get_reading(); }),
    MenuItem::value("Desired", []() { return hot_water_program.get_desired(); }),
    MenuItem::on_off("Heating", []() { return hot_water_program.get_state() == HotWaterController::State::on; }),
    MenuItem::exit()
};

Menu menu = {
    MenuItem::submenu("Hot water", water_menu),
    MenuItem::value("Time", []() {
            if (!ntp_clock.ready()) {
                return std::string("??:??");
            }
            const auto time = ntp_clock.get_time();
            char text[6];
            snprintf(text, 6, "%02i:%02i", time.get_hours(), time.get_minutes());
            return std::string(text);
        }),
    MenuItem::value("IP", []() { return std::string(WiFi.localIP().toString().c_str()); })
};

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
}


void loop() {
    wifi_control.tick();
    ntp_clock.tick();

    // Serial.println(WiFi.localIP());
    // wifi_readings.load("http://192.168.1.200/measurements.json");

    temperature_sensor_controller.tick();
    hot_water_program.tick();

    buttons.tick();
    menu.tick(lcd, buttons);
}
