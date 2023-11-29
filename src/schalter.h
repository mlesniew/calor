#pragma once

#include <ArduinoJson.h>
#include <PicoUtils.h>

class AbstractSchalter: public PicoUtils::Tickable {
    public:
        enum class State {
            init = 0,
            inactive = 1,
            activating = 2,
            deactivating = 3,
            active = 4,
            error = -1,
        };

        AbstractSchalter(): state(State::init) {}

        virtual String str() const = 0;
        virtual DynamicJsonDocument get_config() const = 0;

        void set_request(const void * requester, bool requesting);

        State get_state() const { return state; }


    protected:
        void set_state(State new_state);
        bool has_activation_requests() const { return !requesters.empty(); }
        unsigned long get_state_time_millis() const { return state.elapsed_millis(); }

    private:
        std::set<const void *> requesters;
        PicoUtils::TimedValue<State> state;
};

class Schalter: public AbstractSchalter {
    public:

        Schalter(const String address, const unsigned int index, const unsigned long switch_time_millis);
        String str() const override { return address + "/" + String(index); }

        const String address;
        const unsigned int index;
        const unsigned long switch_time_millis;

        void tick() override;
        DynamicJsonDocument get_config() const override;

    protected:
        PicoUtils::TimedValue<bool> is_active;
        PicoUtils::TimedValue<bool> last_request;
};

const char * to_c_str(const Schalter::State & s);
Schalter * get_schalter(const JsonVariantConst & json);
