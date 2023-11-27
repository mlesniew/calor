#pragma once

#include <ArduinoJson.h>
#include <PicoUtils.h>

class Schalter: public PicoUtils::Tickable {
    public:
        enum class State {
            init = 0,
            inactive = 1,
            activating = 2,
            deactivating = 3,
            active = 4,
            error = -1,
        };

        Schalter(const String address, const unsigned int index, const unsigned long switch_time_millis);
        String str() const { return address + "/" + String(index); }

        bool activate;

        const String address;
        const unsigned int index;
        const unsigned long switch_time_millis;

        void tick() override;
        State get_state() const { return state; }
        DynamicJsonDocument get_config() const;

    protected:
        PicoUtils::TimedValue<State> state;
        PicoUtils::TimedValue<bool> is_active;
        PicoUtils::TimedValue<bool> last_request;

        void set_state(State new_state);
        unsigned long get_state_elapsed_millis() { return state.elapsed_millis(); }

};

const char * to_c_str(const Schalter::State & s);
Schalter * get_schalter(const JsonVariantConst & json);
