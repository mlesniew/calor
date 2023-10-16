#pragma once

#include <ArduinoJson.h>
#include <PicoUtils.h>

class Sensor: public PicoUtils::Tickable {
    public:
        enum class State {
            init = 0,
            ok = 1,
            error = -1,
        };

        Sensor(): state(State::init) {}

        virtual String str() const = 0;
        virtual double get_reading() const { return std::numeric_limits<double>::quiet_NaN(); }
        State get_state() const { return state; }
        virtual DynamicJsonDocument get_config() const = 0;

    protected:
        void set_state(State new_state);

    private:
        State state;
};

class DummySensor: public Sensor {
    public:
        DummySensor();
        void tick() override;
        virtual String str() const override { return "dummy"; }
        DynamicJsonDocument get_config() const override;
};

class KelvinSensor: public Sensor {
    public:
        KelvinSensor(const String & address);

        void tick() override;
        virtual String str() const override { return "kelvin/" + address; }
        double get_reading() const override;
        DynamicJsonDocument get_config() const override;

        const String address;

    protected:
        PicoUtils::TimedValue<double> reading;
};

const char * to_c_str(const Sensor::State & s);
Sensor * create_sensor(const JsonVariantConst & json);
