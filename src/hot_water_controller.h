#ifndef HOT_WATER_CONTROLLER_H
#define HOT_WATER_CONTROLLER_H

#include <Arduino.h>
#include <utils/periodic.h>
#include "temperature_sensor_controller_interface.h"
#include "clock.h"
#include "program.h"

class HotWaterController: public TimeProgram<uint8_t, 10>, public Periodic {
    public:
        enum class State : uint8_t { off, on, invalid };

        HotWaterController(
            const Clock & clock,
            const TemperatureSensorControllerInterface & sensors,
            const std::function<void(bool)> & set_output,
            double histeresis);
        void init();
        virtual double get_reading() const;
        void periodic_proc() override;

    protected:
        const std::function<void(bool)> set_output;
        const Clock & clock;
        const TemperatureSensorControllerInterface & sensors;
        State state;
        double histeresis;
};

#endif
