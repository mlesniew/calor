#pragma once

#include <list>
#include <set>

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
        virtual JsonDocument get_config() const = 0;

        void set_request(const void * requester, bool requesting);

        State get_state() const { return state; }
        bool is_ok() const { return state != State::error && state != State::init; }

    protected:
        virtual void set_state(State new_state);
        bool has_activation_requests() const { return !requesters.empty(); }

    private:
        std::set<const void *> requesters;
        PicoUtils::TimedValue<State> state;
};

class SchalterSet: public AbstractSchalter {
    public:
        SchalterSet(const std::list<AbstractSchalter *> schalters)
            : schalters(schalters) {}

        String str() const override;
        JsonDocument get_config() const override;

        void tick() override;

    protected:
        const std::list<AbstractSchalter *> schalters;
};

class Schalter: public AbstractSchalter {
    public:
        Schalter(const String & name);
        String str() const override { return name; }

        const String name;

        void tick() override;
        JsonDocument get_config() const override;

        void publish_request();

    protected:
        virtual void set_state(State new_state) override;

        PicoUtils::Stopwatch last_update;
        PicoUtils::TimedValue<bool> last_request;
};

const char * to_c_str(const Schalter::State & s);
AbstractSchalter * get_schalter(const JsonVariantConst & json);
