#include <PicoMQTT.h>
#include <PicoSyslog.h>

#include "schalter.h"

extern PicoSyslog::Logger syslog;
extern PicoMQTT::Server mqtt;

namespace {

std::list<Schalter *> schalters;

}

const char * to_c_str(const Schalter::State & s) {
    switch (s) {
        case Schalter::State::init:
            return "init";
        case Schalter::State::active:
            return "active";
        case Schalter::State::inactive:
            return "inactive";
        case Schalter::State::activating:
            return "activating";
        case Schalter::State::deactivating:
            return "deactivating";
        default:
            return "error";
    }
}

Schalter::Schalter(const String & name)
    : name(name), last_request(false) {

    if (!name.length()) {
        set_state(State::error);
        return;
    }

    mqtt.subscribe("schalter/" + name, [this](const String & payload) {
        syslog.printf("Got update on valve %s: %s\n", this->name.c_str(), payload.c_str());
        if (payload == "ON") {
            set_state(State::active);
        } else if (payload == "OFF") {
            set_state(State::inactive);
        } else if (payload == "TON") {
            set_state(State::activating);
        } else if (payload == "TOFF") {
            set_state(State::deactivating);
        } else {
            syslog.printf("Invalid schalter state on valve %s: %s\n", this->name.c_str(), payload.c_str());
        }
    });

}

void AbstractSchalter::set_state(State new_state) {
    if (state == new_state) {
        return;
    }
    syslog.printf("Schalter %s changing state from %s to %s.\n", str().c_str(), to_c_str(state), to_c_str(new_state));
    state = new_state;
}

void AbstractSchalter::set_request(const void * requester, bool requesting) {
    if (requesting) { requesters.insert(requester); } else { requesters.erase(requester); }
}

void Schalter::publish_request() {
    if (name.length()) {
        const bool activate = has_activation_requests();
        mqtt.publish("schalter/" + name + "/set", activate ? "ON" : "OFF");
        last_request = activate;
    }
}

void Schalter::tick() {
    if ((last_request.elapsed_millis() >= 30 * 1000) || (last_request != has_activation_requests())) {
        publish_request();
    }

    if (last_update.elapsed_millis() >= 2 * 60 * 1000) {
        set_state(State::error);
    }
}

void Schalter::set_state(State new_state) {
    last_update.reset();
    AbstractSchalter::set_state(new_state);
}

JsonDocument Schalter::get_config() const {
    JsonDocument json;
    json = name;
    return json;
}

String SchalterSet::str() const {
    String ret;
    bool first = true;
    for (AbstractSchalter * schalter : schalters) {
        if (first) {
            ret = schalter->str();
            first = false;
        } else {
            ret = ret + ", " + schalter->str();
        }
    };
    return "[" + ret + "]";
}

JsonDocument SchalterSet::get_config() const {
    JsonDocument json;
    size_t idx = 0;
    for (AbstractSchalter * schalter : schalters) {
        json[idx++] = schalter->get_config();
    }
    return json;
}

void SchalterSet::tick() {
    std::map<State, size_t> states;
    const bool activate = has_activation_requests() && is_ok();

    for (AbstractSchalter * schalter : schalters) {
        schalter->tick();
        states[schalter->get_state()] += 1;
        schalter->set_request(this, activate);
    }

    if (states[State::error]) {
        // if any element is in error state, we're in error state too
        set_state(State::error);
    } else if (states[State::init]) {
        // if any element is in init state (but no errors), we're in init state too
        set_state(State::init);
    } else if (has_activation_requests() && states[State::active]) {
        // at least one active element
        set_state(State::active);
    } else if (!has_activation_requests() && (states[State::inactive] == schalters.size())) {
        // only inactive elements, means we're inactive too
        set_state(State::inactive);
    } else {
        // states are different, we're transitioning
        set_state(has_activation_requests() ? State::activating : State::deactivating);
    }
}

AbstractSchalter * get_schalter(const String & name) {
    if (name.length() == 0) {
        return nullptr;
    }

    for (auto schalter : schalters) {
        if (schalter->name == name) {
            return schalter;
        }
    }

    auto schalter = new Schalter(name);
    schalters.push_back(schalter);

    return schalter;
}

AbstractSchalter * get_schalter(const JsonVariantConst & json) {
    if (json.is<String>()) {
        return get_schalter(json.as<String>());
    } else if (json.is<JsonArrayConst>()) {
        std::list<AbstractSchalter *> elements;
        for (const JsonVariantConst & value : json.as<JsonArrayConst>()) {
            AbstractSchalter * element = get_schalter(value);
            if (element) {
                elements.push_back(element);
            }
        }
        return new SchalterSet(elements);
    } else {
        return nullptr;
    }
}
