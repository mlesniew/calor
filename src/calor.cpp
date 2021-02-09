#include <Arduino.h>

#include <Ticker.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "shift_register.h"

#define DS18B20_RESOLUTION 12
#define DS18B20_CONVERSION_DELAY_MS (750 / (1 << (12 - DS18B20_RESOLUTION)))

OneWire one_wire(D7);
DallasTemperature sensors(&one_wire);

class Led {
    public:
        Led(unsigned int pin, bool inverted) : pin(pin), inverted(inverted), step(0) {}
        void begin() {
            pinMode(pin, OUTPUT);
            ticker.attach(3, [this](){ tick(); });
        }

    private:
        void tick() {
            step = 1 - step;
            digitalWrite(pin, step);
        }

        const unsigned int pin;
        const bool inverted;
        unsigned int step;
        Ticker ticker;
};

Led led(D4, true);

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

    shift_register_init();

    led.begin();

#if 1
    if (!setup_sensors()) {
        delay(1000);
        ESP.restart();
    }
#endif
}

void loop() {
    delay(1000);
#if 1
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

    {
        static uint8_t idx = 0;
        shift_register_write(idx, false);
        idx = (idx + 1) % 8;
        shift_register_write(idx, true);
        Serial.println(idx);
    }
}
