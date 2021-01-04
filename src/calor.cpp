#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define DEGREE "\xdf"
LiquidCrystal_I2C lcd(0x3F, 16, 2); // set the LCD address to 0x3F for a 16 chars and 2 line display

void show_measurement(const char * title, const char * value) {
    const auto len = strlen(value);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(title);

    if (len >= 16)
        lcd.setCursor(0, 1);
    else
        lcd.setCursor(16 - len, 1);

    lcd.print(value);
}

void show_measurement(const char * title, const float value, const char * unit = "", unsigned char decimal_places = 1) {
    char value_text[17];
    snprintf(value_text, 17, "%.*f%s", decimal_places, value, unit);
    show_measurement(title, value_text);
}

void show_measurement(const char * title, const int value, const char * unit) {
    show_measurement(title, float(value), unit, 0);
}

void show_measurement(const char * title, const bool value) {
    show_measurement(title, value ? "on" : "off");
}

void setup() {
    lcd.init();
    lcd.backlight();
}

void loop() {
    show_measurement("Water tank", 42.654, DEGREE "C", 2);
    delay(2000);
    show_measurement("Circulation pump", true);
    delay(2000);
    show_measurement("Boiler state", "off");
    delay(2000);
    show_measurement("Boiler state", "heating");
    delay(2000);
    show_measurement("Boiler state", "heating water");
    delay(2000);
    show_measurement("Room temperature", 20.15, DEGREE "C", 1);
    delay(2000);
}
