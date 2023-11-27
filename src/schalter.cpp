#include <list>

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
      state(State::init), is_active(false), last_request(false) {

    if (!address.length()) {
        set_state(State::error);
        return;
    }

    mqtt.subscribe("schalter/" + address + "/" + String(index), [this](const String & payload) {
        if (payload == "ON") {
            is_active = true;
            tick();
        } else if (payload == "OFF") {
            is_active = false;
            tick();
        }
    });

}

void Schalter::set_state(State new_state) {
    if (state == new_state) {
        return;
    }
    syslog.printf("Schalter %s changing state from %s to %s.\n", str().c_str(), to_c_str(state), to_c_str(new_state));
    state = new_state;
}

void Schalter::set_request(const void * requester, bool requesting) {
    if (requesting) { requesters.insert(requester); } else { requesters.erase(requester); }
}

void Schalter::tick() {
    const bool activate = !requesters.empty();

    if (address.length() && ((last_request.elapsed_millis() >= 15 * 1000) || (last_request != activate))) {
        mqtt.publish("schalter/" + address + "/" + String(index) + "/set", activate ? "ON" : "OFF");
        last_request = activate;
    }

    if (is_active.elapsed_millis() >= 2 * 60 * 1000) {
        set_state(State::error);
    }

    const bool output_active = bool(is_active);
    const bool timeout = (state.elapsed_millis() >= switch_time_millis);

    switch (state) {
        case State::error:
        case State::init:
            // noop
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

Schalter * get_schalter(const JsonVariantConst & json) {
    const String address = json["address"] | "";
    const unsigned int index = json["index"] | 0;
    const unsigned long switch_time_millis = (json["switch_time"] | 120) * 1000;

    if (address.length() == 0 || index == 0) {
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
}
