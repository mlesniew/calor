#ifndef DS18B20_H
#define DS18B20_H

#include <OneWire.h>
#include <DallasTemperature.h>

class TemperatureSensors {
    public:
        TemperatureSensors(uint8_t pin) : one_wire(pin), sensors(&one_wire) {}
        void init();
        uint8_t scan(uint8_t * address, uint8_t start_idx = 0);
        void update();
        float read(const uint8_t * address);
    protected:
        OneWire one_wire;
        DallasTemperature sensors;
};

#endif
