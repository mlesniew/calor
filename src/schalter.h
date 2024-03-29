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
        void set_state(State new_state);
        bool has_activation_requests() const { return !requesters.empty(); }
        unsigned long get_state_time_millis() const { return state.elapsed_millis(); }

    private:
        std::set<const void *> requesters;
        PicoUtils::TimedValue<State> state;
};

class SchalterGroup: public AbstractSchalter {
    public:
        SchalterGroup(const std::list<AbstractSchalter *> schalters)
            : schalters(schalters) {}

        String str() const override;
        JsonDocument get_config() const override;

    protected:
        virtual String get_group_type() const = 0;
        const std::list<AbstractSchalter *> schalters;
};

class SchalterSequence: public SchalterGroup {
    public:
        using SchalterGroup::SchalterGroup;
        void tick() override;

    protected:
        String get_group_type() const override { return "sequence"; }
};

class SchalterSet: public SchalterGroup {
    public:
        using SchalterGroup::SchalterGroup;
        void tick() override;

    protected:
        String get_group_type() const override { return "set"; }
};

class Schalter: public AbstractSchalter {
    public:
        Schalter(const String address, const unsigned int index, const unsigned long switch_time_millis);
        String str() const override { return address + "/" + String(index); }

        const String address;
        const unsigned int index;
        const unsigned long switch_time_millis;

        void tick() override;
        JsonDocument get_config() const override;

        unsigned long get_last_request_elapsed_millis() const { return last_request.elapsed_millis(); }
        void publish_request();

    protected:
        PicoUtils::TimedValue<bool> is_active;
        PicoUtils::TimedValue<bool> last_request;
};

const char * to_c_str(const Schalter::State & s);
AbstractSchalter * get_schalter(const JsonVariantConst & json);
void publish_schalter_requests();
