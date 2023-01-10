#ifndef VALVE_H
#define VALVE_H

#include <string>

#include <utils/io.h>
#include <utils/tickable.h>
#include <utils/timedvalue.h>

#include <ArduinoJson.h>

#include "valvestate.h"


class Valve: public Tickable {
    public:
        Valve(BinaryOutput & output, const std::string & name = "", const unsigned long switch_time_millis = 0);
        ~Valve();

        void tick() override;
        ValveState get_state() const {
            return state;
        }

        DynamicJsonDocument get_config() const;
        DynamicJsonDocument get_status() const;
        bool set_config(const JsonVariantConst & json);

        std::string name;
        unsigned long switch_time_millis;
        bool demand_open;

    protected:
        BinaryOutput & output;
        TimedValue<ValveState> state;
};

#endif
