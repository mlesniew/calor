#include <Arduino.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <utils/led.h>
#include <utils/shift_register.h>
#include <utils/wifi_control.h>
#include "wifi_readings.h"

#include "ds18b20.h"

BlinkingLed wifi_led(D4, 0, 91, true);
WiFiControl wifi_control(wifi_led);
WiFiReadings wifi_readings;
ShiftRegister<1> shift_register(
        D6,  // data pin
        D5,  // clock pin
        D0,  // latch pin
        (uint8_t[1]){ 0b00001111, }  // inverted outputs
        );

WiFiUDP ntp_udp;
NTPClient ntp_client(ntp_udp, 60 * 60);

TemperatureSensors temperature_sensors(D7);
DeviceAddress temperature_sensor_address[4];
uint8_t sensor_count;

void setup() {
    Serial.begin(9600);
    Serial.println(F("Calor " __DATE__ " " __TIME__));

    shift_register.init();
    temperature_sensors.init();
    wifi_control.init();

    uint8_t idx = 0;
    sensor_count = 0;
    while ((idx = temperature_sensors.scan(temperature_sensor_address[sensor_count], idx)) && (sensor_count < 4)) {
        Serial.print(F("Found sensor: "));
        for (uint8_t j = 0; j < 8; ++j) {
            Serial.print(temperature_sensor_address[sensor_count][j] >> 4, HEX);
            Serial.print(temperature_sensor_address[sensor_count][j] & 0xf, HEX);
        }
        Serial.print(F("\n"));
        ++sensor_count;
    }

    ntp_client.begin();
}

void loop() {
    wifi_control.tick();

    Serial.println(WiFi.localIP());
    wifi_readings.load("http://192.168.1.200/measurements.json");

    static bool have_time = false;
    have_time = have_time || ntp_client.update();

    if (have_time) {
        Serial.println(ntp_client.getFormattedTime());
    }

    temperature_sensors.update();

    for (uint8_t i = 0; i < sensor_count; ++i) {
        Serial.print("Temperature: ");
        Serial.println(temperature_sensors.read(temperature_sensor_address[i]));
    }
}
