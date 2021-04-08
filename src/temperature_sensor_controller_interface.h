#ifndef TEMPERATURE_SENSOR_CONTROLLER_INTERFACE_H
#define TEMPERATURE_SENSOR_CONTROLLER_INTERFACE_H

#include <stdint.h>
#include <DallasTemperature.h>

class TemperatureSensorControllerInterface {
    public:
        virtual void init() = 0;
        virtual uint8_t scan() = 0;
        virtual float get_reading(const uint8_t index) const = 0;
        virtual float get_reading(const DeviceAddress & address) const = 0;
        virtual const DeviceAddress & get_address(uint8_t idx) const = 0;
        virtual const uint8_t get_sensor_count() const = 0;
};

#endif
