#pragma once

#include <ArduinoJson.h>
#include <PicoUtils.h>

class Valve;
class Sensor;

class Zone: public PicoUtils::Tickable {
    public:
        enum class State {
            init = 0,
            wait = 1,
            heat = 2,
            error = -1,
        };

        Zone(const String & name, const JsonVariantConst & json);
        Zone(const Zone &) = delete;
        Zone & operator=(const Zone &) = delete;

        void tick();
        bool heat() const;

        DynamicJsonDocument get_config() const;
        DynamicJsonDocument get_status() const;
        double get_reading() const;
        State get_state() const;

        String unique_id() const;
        bool healthcheck() const;

        const String name;
        bool enabled;
        double desired;
        const double hysteresis;

    private:
        State state;
        Sensor * sensor;
        Valve * valve;
};
