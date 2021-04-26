#ifndef HOT_WATER_CONTROLLER_H
#define HOT_WATER_CONTROLLER_H

#include <string>

#include <Arduino.h>
#include <utils/periodic.h>
#include "temperature_sensor_controller_interface.h"
#include "program.h"

class Clock;
class PresenceController;

class HotWaterController: public TimeProgram<uint8_t, 10>, public Periodic {
    public:
        HotWaterController(
            const Clock & clock,
            const TemperatureSensorControllerInterface & sensors,
            const PresenceController & presence_controller,
            const std::function<void(bool)> & set_output,
            double histeresis);
        void init();
        virtual double get_reading() const;
        virtual double get_desired() const;
        void periodic_proc() override;
        bool get_state() const { return state; }
        const std::string & get_reason() const { return reason; }

    protected:
        const std::function<void(bool)> set_output;
        const Clock & clock;
        const TemperatureSensorControllerInterface & sensors;
        const PresenceController & presence_controller;
        bool state;
        double histeresis;
        std::string reason;
};

#endif
