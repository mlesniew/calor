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

Schalter::Schalter(const String address, const unsigned int index, const unsigned long switch_time_millis)
    : address(address), index(index), switch_time_millis(switch_time_millis),
      is_active(false), last_request(false) {

    if (!address.length()) {
        set_state(State::error);
        return;
    }

    mqtt.subscribe("schalter/" + address + "/" + String(index), [this](const String & payload) {
        syslog.printf("Got update on valve %s: %s\n", str().c_str(), payload.c_str());
        if (payload == "ON") {
            is_active = true;
            tick();
        } else if (payload == "OFF") {
            is_active = false;
            tick();
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

void Schalter::tick() {
    const bool activate = has_activation_requests();
    const bool is_error = is_active.elapsed_millis() >= 2 * 60 * 1000;

    if (address.length() && ((last_request.elapsed_millis() >= 15 * 1000) || (last_request != activate))) {
        mqtt.publish("schalter/" + address + "/" + String(index) + "/set", activate ? "ON" : "OFF");
        last_request = activate;
    }

    if (is_error) {
        set_state(State::error);
    }

    const bool output_active = bool(is_active);
    const bool timeout = (get_state_time_millis() >= switch_time_millis);

    switch (get_state()) {
        case State::error:
        case State::init:
            if (!is_error) {
                set_state(output_active ? State::activating : State::deactivating);
            }
            break;
        case State::inactive:
            if (output_active) {
                set_state(State::activating);
            }
            break;
        case State::active:
            if (!output_active) {
                set_state(State::deactivating);
            }
            break;
        case State::deactivating:
            if (output_active) {
                set_state(State::activating);
            } else if (timeout) {
                set_state(State::inactive);
            }
            break;
        case State::activating:
            if (!output_active) {
                set_state(State::deactivating);
            } else if (timeout) {
                set_state(State::active);
            }
            break;
    }
}

DynamicJsonDocument Schalter::get_config() const {
    DynamicJsonDocument json(256);
    json["address"] = address;
    json["index"] = index;
    json["switch_time"] = switch_time_millis / 1000;
    return json;
}

String SchalterGroup::str() const {
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
    return get_group_type() + ":[" + ret + "]";
}

DynamicJsonDocument SchalterGroup::get_config() const {
    DynamicJsonDocument json(1024);
    json["type"] = get_group_type();
    size_t idx = 0;
    for (AbstractSchalter * schalter : schalters) {
        json["elements"][idx++] = schalter->get_config();
    }
    return json;
}

void SchalterSequence::tick() {
    std::map<State, size_t> states;
    bool activate_next = has_activation_requests() && is_ok();

    for (AbstractSchalter * schalter : schalters) {
        schalter->tick();
        states[schalter->get_state()] += 1;
        schalter->set_request(this, activate_next);
        activate_next = activate_next && (schalter->get_state() == State::active);
    }

    if (states[State::error] > 0) {
        // if any element is in error state, we're in error state too
        set_state(State::error);
    } else if (states[State::init]) {
        // if any element is in init state (but no errors), we're in init state too
        set_state(State::init);
    } else if (has_activation_requests() && (states[State::active] == schalters.size())) {
        // only active elements, means we're active too
        set_state(State::active);
    } else if (!has_activation_requests() && (states[State::inactive] == schalters.size())) {
        // only inactive elements, means we're inactive too
        set_state(State::inactive);
    } else {
        // states are different we're transitioning
        set_state(has_activation_requests() ? State::activating : State::deactivating);
    }
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

AbstractSchalter * get_schalter(const JsonVariantConst & json) {

    std::list<AbstractSchalter *> elements;
    for (const JsonVariantConst & value : json["elements"].as<JsonArrayConst>()) {
        AbstractSchalter * element = get_schalter(value);
        if (element) {
            elements.push_back(element);
        }
    }

    const String type = json["type"] | "schalter";

    if (type == "schalter") {
        const String address = json["address"] | "";
        const unsigned int index = json["index"] | 0;
        const unsigned long switch_time_millis = (json["switch_time"] | 180) * 1000;

        if (address.length() == 0) {
            return nullptr;
        }

        for (auto schalter : schalters) {
            if ((schalter->address == address) && (schalter->index == index)) {
                return schalter;
            }
        }
        auto schalter = new Schalter(address, index, switch_time_millis);
        schalters.push_back(schalter);

        return schalter;
    } else if (type == "sequence") {
        return new SchalterSequence(elements);
    } else if (type == "set") {
        return new SchalterSet(elements);
    } else {
        return nullptr;
    }

}
