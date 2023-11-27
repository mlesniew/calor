#include <PicoMQTT.h>
#include <PicoSyslog.h>

#include "valve.h"

extern PicoSyslog::Logger syslog;
extern PicoMQTT::Server mqtt;

const char * to_c_str(const Valve::State & s) {
    switch (s) {
        case Valve::State::init:
            return "init";
        case Valve::State::open:
            return "open";
        case Valve::State::closed:
            return "closed";
        case Valve::State::opening:
            return "opening";
        case Valve::State::closing:
            return "closing";
        default:
            return "error";
    }
}

void Valve::set_state(State new_state) {
    if (state == new_state) {
        return;
    }
    syslog.printf("Valve %s changing state from %s to %s.\n", str().c_str(), to_c_str(state), to_c_str(new_state));
    state = new_state;
}

Valve::Valve(const JsonVariantConst & json)
    : request_open(false), state(State::init), address(json["address"] | ""), index(json["index"] | 0),
      switch_time_millis((json["switch_time"] | 120) * 1000), is_active(false), last_request(false) {

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

void Valve::tick() {
    if (address.length() && ((last_request.elapsed_millis() >= 15 * 1000) || (last_request != request_open))) {
        mqtt.publish("schalter/" + address + "/" + String(index) + "/set", request_open ? "ON" : "OFF");
        last_request = request_open;
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
        case State::closed:
            if (output_active) {
                set_state(State::opening);
            }
            break;
        case State::open:
            if (!output_active) {
                set_state(State::closing);
            }
            break;
        case State::closing:
            if (output_active) {
                set_state(State::opening);
            } else if (timeout) {
                set_state(State::closed);
            }
            break;
        case State::opening:
            if (!output_active) {
                set_state(State::closing);
            } else if (timeout) {
                set_state(State::open);
            }
            break;
    }
}

DynamicJsonDocument Valve::get_config() const {
    DynamicJsonDocument json(256);
    json["address"] = address;
    json["index"] = index;
    json["switch_time"] = switch_time_millis / 1000;
    return json;
}

Valve * get_valve(const JsonVariantConst & json) {
    const String type = json["type"] | "null";
    if (json.isNull()) {
        return nullptr;
    } else {
        return new Valve(json);
    }
}
