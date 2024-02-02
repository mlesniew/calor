#pragma once

#include <list>

#include <ArduinoJson.h>
#include <PicoUtils.h>

class AbstractSensor: public PicoUtils::Tickable {
    public:
        enum class State {
            init = 0,
            ok = 1,
            error = -1,
        };

        AbstractSensor(): state(State::init) {}

        virtual String str() const = 0;
        virtual double get_reading() const { return std::numeric_limits<double>::quiet_NaN(); }
        State get_state() const { return state; }
        virtual JsonDocument get_config() const = 0;

    protected:
        void set_state(State new_state);

    private:
        State state;
};

class DummySensor: public AbstractSensor {
    public:
        DummySensor();
        void tick() override;
        virtual String str() const override { return "dummy"; }
        JsonDocument get_config() const override;
};

class Sensor: public AbstractSensor {
    public:
        Sensor(const String & address);

        void tick() override;
        virtual String str() const override { return address; }
        double get_reading() const override;
        JsonDocument get_config() const override;

        const String address;

    protected:
        PicoUtils::TimedValue<double> reading;
};

class SensorChain: public AbstractSensor {
    public:
        SensorChain();
        SensorChain(const std::list<AbstractSensor *> sensors) : sensors(sensors) {}

        void tick() override;
        virtual String str() const override;
        double get_reading() const override;
        JsonDocument get_config() const override;

    protected:
        const std::list<AbstractSensor *> sensors;
};

const char * to_c_str(const AbstractSensor::State & s);
AbstractSensor * get_sensor(const JsonVariantConst & json);
