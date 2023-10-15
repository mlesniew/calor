#pragma once

#include <ArduinoJson.h>
#include <PicoUtils.h>

class Valve: public PicoUtils::Tickable {
    public:
        enum class State {
            init = 0,
            closed = 1,
            opening = 2,
            closing = 3,
            open = 4,
            error = -1,
        };

        Valve(): request_open(false), state(State::init) {}

        bool request_open;

        State get_state() const { return state; }
        virtual String str() const = 0;

        virtual DynamicJsonDocument get_config() const = 0;

    protected:
        void set_state(State new_state);
        unsigned long get_state_elapsed_millis() { return state.elapsed_millis(); }

    private:
        PicoUtils::TimedValue<State> state;
};

class DummyValve: public Valve {
    public:
        void tick() { set_state(request_open ? State::open : State::closed); }
        String str() const override { return "dummy"; }

        DynamicJsonDocument get_config() const override;
};

class PhysicalValve: public Valve {
    public:
        PhysicalValve(unsigned long switch_time_millis)
            : switch_time_millis(switch_time_millis) {}
        PhysicalValve(const JsonVariantConst & json)
            : PhysicalValve((json["switch_time"] | 120) * 1000) {}

        DynamicJsonDocument get_config() const override;
        void tick() override;

        const unsigned long switch_time_millis;

    protected:
        virtual bool is_output_active() const { return request_open; }
};

class LocalValve: public PhysicalValve {
    public:
        LocalValve(const JsonVariantConst & json, PicoUtils::BinaryOutput & output)
            : PhysicalValve(json), output(output) {}

        void tick() override;
        String str() const override { return "local"; }

        DynamicJsonDocument get_config() const override;

    protected:
        PicoUtils::BinaryOutput & output;
};

class SchalterValve: public PhysicalValve {
    public:
        SchalterValve(const JsonVariantConst & json);
        void tick() override;
        String str() const override { return "schalter:" + address + "/" + String(index); }

        const String address;
        const unsigned int index;

        DynamicJsonDocument get_config() const override;

    protected:
        PicoUtils::TimedValue<bool> is_active;
        PicoUtils::TimedValue<bool> last_request;

        virtual bool is_output_active() const { return is_active; }
};

const char * to_c_str(const Valve::State & s);
Valve * create_valve(const JsonVariantConst & json);
