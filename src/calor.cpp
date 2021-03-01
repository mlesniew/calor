#include <Arduino.h>

#include <DallasTemperature.h>
#include <NTPClient.h>
#include <OneWire.h>
#include <WiFiUdp.h>

#include "led.h"
#include "shift_register.h"
#include "wifi_control.h"
#include "wifi_readings.h"

#define DS18B20_RESOLUTION 12
#define DS18B20_CONVERSION_DELAY_MS (750 / (1 << (12 - DS18B20_RESOLUTION)))

OneWire one_wire(D7);
DallasTemperature sensors(&one_wire);

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

bool setup_sensors() {
    DeviceAddress address;
    sensors.begin();

    Serial.println(F("Searching for DS18B20 temperature sensors..."));
    if (!sensors.getDeviceCount() || !sensors.getAddress(address, 0)) {
        Serial.println(F("No device found."));
        return false;
    }

    Serial.print(F("Found sensor: "));
    for (uint8_t j = 0; j < 8; ++j) {
        Serial.print(address[j] >> 4, HEX);
        Serial.print(address[j] & 0xf, HEX);
    }

    Serial.print(F(": "));

    if (!sensors.validFamily(address)) {
        Serial.println(F("unsupported"));
        return false;
    }

    Serial.println(F("ok"));

    if (sensors.getResolution(address) != DS18B20_RESOLUTION) {
        Serial.print(F("Setting resolution..."));
        if (!sensors.setResolution(address, DS18B20_RESOLUTION)) {
            Serial.println(F("failed"));
            return false;
        }
        Serial.println(F("ok"));
    } else {
        Serial.println(F("Resolution already set."));
    }

    // use asynchronous requests
    // sensors.setWaitForConversion(false);

    return true;
}

void setup() {
    Serial.begin(9600);
    Serial.println(F("Calor " __DATE__ " " __TIME__));

    shift_register.init();
    wifi_control.init();
    ntp_client.begin();

#if 0
    if (!setup_sensors()) {
        delay(1000);
        ESP.restart();
    }
#endif
}

void loop() {
    Serial.println(WiFi.localIP());
    wifi_readings.load("http://192.168.1.200/measurements.json");

    static bool have_time = false;
    have_time = have_time || ntp_client.update();

    if (have_time) {
        Serial.println(ntp_client.getFormattedTime());
    }

#if 0
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

    if(tempC != DEVICE_DISCONNECTED_C) 
    {
        Serial.print("Temperature for the device 1 (index 0) is: ");
        Serial.println(tempC);
    } else {
        Serial.println("Error: Could not read temperature data");
    }
#endif

    delay(5000);
}
