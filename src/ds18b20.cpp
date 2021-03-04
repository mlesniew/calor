#include "ds18b20.h"

#define DS18B20_RESOLUTION 12

void TemperatureSensors::init() {
    sensors.begin();
}

uint8_t TemperatureSensors::scan(uint8_t * address, uint8_t idx) {
    uint8_t device_count = sensors.getDeviceCount();

    for (; idx < device_count; ++idx) {
        if (!sensors.getAddress(address, idx)) {
            // error reading address
            return 0;
        }

        if (!sensors.validFamily(address)) {
            // unsupported sensor
            continue;
        }

        if (sensors.getResolution(address) != DS18B20_RESOLUTION) {
            // resolution needs to be set
            if (!sensors.setResolution(address, DS18B20_RESOLUTION)) {
                // error setting resolution
                return 0;
            }
        }

        // all good!
        return idx + 1;
    }

    // no more sensors found
    return 0;
}

void TemperatureSensors::update() {
    sensors.requestTemperatures();
}

float TemperatureSensors::read(const uint8_t * address) {
    return sensors.getTempC(address);
}
