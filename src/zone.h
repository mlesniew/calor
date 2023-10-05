#ifndef ZONE_H
#define ZONE_H

#include <string>

#include <ArduinoJson.h>

#include <utils/tickable.h>
#include <utils/timedvalue.h>

#include <valve.h>

class Zone: public Tickable {
    public:
        enum class State {
            init = 0,
            off = 1,
            on = 2,
            open_valve = 3,
            close_valve = 4,
            error = -1,
        };

        Zone(const std::string & name);
        ~Zone();

        void copy_config_from(const Zone & zone);

        void tick();

        bool valve_desired_state() const;
        bool boiler_desired_state() const;

        DynamicJsonDocument get_config() const;
        DynamicJsonDocument get_status() const;
        bool set_config(const JsonVariantConst & json);

        std::string name;

        TimedValue<double> reading;
        TimedValue<Valve::State> valve_state;

        double desired, hysteresis;

        State get_state() const { return state; }

    protected:
        State state;
};

const char * to_c_str(const Zone::State & s);

#endif
