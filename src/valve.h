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

        Valve(const JsonVariantConst & json);
        String str() const { return address + "/" + String(index); }

        bool request_open;

        const String address;
        const unsigned int index;
        const unsigned long switch_time_millis;

        void tick() override;
        State get_state() const { return state; }
        DynamicJsonDocument get_config() const;

    protected:
        PicoUtils::TimedValue<bool> is_active;
        PicoUtils::TimedValue<bool> last_request;

        void set_state(State new_state);
        unsigned long get_state_elapsed_millis() { return state.elapsed_millis(); }

    private:
        PicoUtils::TimedValue<State> state;
};

const char * to_c_str(const Valve::State & s);
Valve * get_valve(const JsonVariantConst & json);
