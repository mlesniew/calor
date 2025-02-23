#pragma once

#include <ArduinoJson.h>
#include <PicoUtils.h>

class AbstractSchalter;
class AbstractSensor;

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

        JsonDocument get_config() const;
        JsonDocument get_status() const;
        double get_reading() const;
        State get_state() const;

        String unique_id() const;
        bool healthcheck() const;

        void boost(double timeout_seconds = 60 * 60);
        bool boost_active() const;

        const String name;
        bool enabled;
        double desired;
        const double hysteresis;

    private:
        State state;
        AbstractSensor * sensor;
        AbstractSchalter * valve;

        double boost_timeout;
        PicoUtils::Stopwatch boost_stopwatch;
};
