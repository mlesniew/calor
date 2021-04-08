#ifndef TEMPERATURE_SENSOR_CONTROLLER_H
#define TEMPERATURE_SENSOR_CONTROLLER_H

#include <DallasTemperature.h>
#include <utils/periodic.h>
#include "temperature_sensor_controller_interface.h"

class OneWire;

template<uint8_t max_sensors = 4, uint8_t resolution = 12>
class TemperatureSensorController: public TemperatureSensorControllerInterface, public Periodic {
    public:
        TemperatureSensorController(OneWire & one_wire, unsigned long interval_seconds = 15)
            : Periodic(interval_seconds), sensors(&one_wire), sensor_count(0) {}

        static void print_address(const DeviceAddress & address) {
            for (uint8_t j = 0; j < 8; ++j) {
                Serial.print(address[j] >> 4, HEX);
                Serial.print(address[j] & 0xf, HEX);
            }
        }

        void init() override {
            sensors.begin();
            scan();
        }

        uint8_t scan() override {
            const uint8_t device_count = sensors.getDeviceCount();

            for (uint8_t idx = 0; idx < device_count && sensor_count < max_sensors; ++idx) {
                DeviceAddress & address = addresses[sensor_count];

                if (!sensors.getAddress(address, idx)) {
                    // error reading address
                    continue;
                }

                Serial.print(F("Found device: "));
                print_address(address);

                if (!sensors.validFamily(address)) {
                    // unsupported sensor
                    Serial.println(F(" - unsupported"));
                    continue;
                }

                if (sensors.getResolution(address) != resolution) {
                    // resolution needs to be set
                    if (!sensors.setResolution(address, resolution)) {
                        // error setting resolution
                        Serial.println(F(" - error setting resolution"));
                        continue;
                    }
                }

                // all good
                Serial.println(F(" - ready"));

                // ...but no reading yet
                readings[sensor_count] = DEVICE_DISCONNECTED_C;

                ++sensor_count;
            }

            // no more sensors found
            return sensor_count;
        }

        void periodic_proc() override {
            if (!sensor_count)
                return;
            Serial.println(F("Updating DS18B20 temperatures..."));
            sensors.requestTemperatures();
            for (uint8_t i = 0; i < sensor_count; ++i) {
                readings[i] = sensors.getTempC(addresses[i]);
                Serial.print(F(" * ["));
                Serial.print(i);
                Serial.print(F("] "));
                print_address(addresses[i]);
                Serial.print(F(": "));
                Serial.println(readings[i]);
            }
        }

        float get_reading(uint8_t idx) const override {
            if (idx < sensor_count)
                return readings[idx];
            else
                return DEVICE_DISCONNECTED_C;
        }

        float get_reading(const DeviceAddress & address) const override {
            for (uint8_t idx = 0; idx < sensor_count; ++idx) {
                if (memcmp(address, addresses[idx], 8) == 0)
                    return readings[idx];
            }
            return DEVICE_DISCONNECTED_C;
        }

        const DeviceAddress & get_address(uint8_t idx) const override {
            return addresses[idx];
        }

        const uint8_t get_sensor_count() const override {
            return sensor_count;
        }

    protected:
        DallasTemperature sensors;
        DeviceAddress addresses[max_sensors];
        float readings[max_sensors];
        uint8_t sensor_count;
};

#endif
