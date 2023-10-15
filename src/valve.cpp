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

DynamicJsonDocument DummyValve::get_config() const {
    DynamicJsonDocument json(128);
    json["type"] = "null";
    return json;
}

void PhysicalValve::tick() {
    const bool output_active = is_output_active();
    const bool timeout = (get_state_elapsed_millis() >= switch_time_millis);

    switch (get_state()) {
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
        default:
            set_state(output_active ? State::opening : State::closing);
    }
}

DynamicJsonDocument PhysicalValve::get_config() const {
    DynamicJsonDocument json(256);
    json["switch_time"] = switch_time_millis / 1000;
    return json;
}

void LocalValve::tick() {
    output = request_open;
    PhysicalValve::tick();
}

DynamicJsonDocument LocalValve::get_config() const {
    DynamicJsonDocument json = PhysicalValve::get_config();
    json["type"] = "local";
    return json;
}

SchalterValve::SchalterValve(const JsonVariantConst & json)
    : PhysicalValve(json), address(json["address"] | ""), index(json["index"] | 0),
      is_active(false), last_request(false) {

    if (!address.length()) {
        set_state(State::error);
        return;
    }

    mqtt.subscribe("schalter/" + address + "/" + String(index), [this](const String & payload) {
        if (payload == "ON") {
            is_active = true;
            PhysicalValve::tick();
        } else if (payload == "OFF") {
            is_active = false;
            PhysicalValve::tick();
        }
    });

}

void SchalterValve::tick() {
    if (address.length() && ((last_request.elapsed_millis() >= 15 * 1000) || (last_request != request_open))) {
        mqtt.publish("schalter/" + address + "/" + String(index) + "/set", request_open ? "ON" : "OFF");
        last_request = request_open;
    }

    if (is_active.elapsed_millis() >= 2 * 60 * 1000) {
        set_state(State::error);
    }

    switch (get_state()) {
        case State::error:
        case State::init:
            // noop
            break;
        default:
            PhysicalValve::tick();
    }
}

DynamicJsonDocument SchalterValve::get_config() const {
    DynamicJsonDocument json = PhysicalValve::get_config();
    json["type"] = "schalter";
    json["address"] = address;
    json["index"] = index;
    return json;
}

Valve * create_valve(const JsonVariantConst & json) {
    const String type = json["type"] | "null";
    if (type == "local") {
        extern PicoUtils::PinOutput valve_relay;
        static Valve * local_valve = new LocalValve(json, valve_relay);
        return local_valve;
    } else if (type == "schalter") {
        return new SchalterValve(json);
    } else {
        return new DummyValve();
    }
}
