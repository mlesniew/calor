#include "zone.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Hash.h>
#include <PicoSyslog.h>

#include <cstdint>

#include "schalter.h"
#include "sensor.h"

extern PicoSyslog::Logger syslog;

const char * to_c_str(const Zone::State & s) {
    switch (s) {
        case Zone::State::init:
            return "init";
        case Zone::State::heat:
            return "heat";
        case Zone::State::wait:
            return "wait";
        default:
            return "error";
    }
}

Zone::Zone(const String & name, const JsonVariantConst & json)
    : name(name),
      enabled(json["enabled"] | true),
      desired(json["desired"] | 21.0),
      hysteresis(json["hysteresis"] | 0.5),
      state(State::init),
      sensor(get_sensor(json["sensor"])),
      valve(get_schalter(json["valve"])),
      boost_timeout(0) {}

void Zone::tick() {
    sensor->tick();
    if (valve) {
        valve->set_request(this, (enabled && (state == State::heat)));
        valve->tick();
    }

    auto set_state = [this](State new_state) {
        if (new_state == state) {
            return;
        }
        syslog.printf("Zone '%s' changing state from %s to %s.\n", name.c_str(),
                      to_c_str(state), to_c_str(new_state));
        state = new_state;
    };

    if (sensor->get_state() == AbstractSensor::State::error ||
        (valve && valve->get_state() == Schalter::State::error)) {
        set_state(State::error);
        return;
    }

    if (sensor->get_state() == AbstractSensor::State::init ||
        (valve && valve->get_state() == Schalter::State::init)) {
        if (state != State::init) {
            set_state(State::init);
        }
        return;
    }

    if (boost_active()) {
        set_state(State::heat);
        return;
    }

    // FSM inputs
    const bool warm = sensor->get_reading() >= desired + 0.5 * hysteresis;
    const bool cold = sensor->get_reading() <= desired - 0.5 * hysteresis;

    switch (state) {
        case State::heat:
            set_state(warm ? State::wait : State::heat);
            break;
        default:
            set_state(cold ? State::heat : State::wait);
    }
}

bool Zone::heat() const {
    return enabled && (state == State::heat) &&
           (!valve || (valve->get_state() == Schalter::State::active));
}

JsonDocument Zone::get_config() const {
    JsonDocument json;

    json["desired"] = desired;
    json["hysteresis"] = hysteresis;
    json["sensor"] = sensor->get_config();
    if (valve) {
        json["valve"] = valve->get_config();
    }
    json["enabled"] = enabled;

    return json;
}

JsonDocument Zone::get_status() const {
    JsonDocument json;

    json["desired"] = desired;
    json["hysteresis"] = hysteresis;
    json["enabled"] = enabled;
    json["reading"] = get_reading();
    json["state"] = to_c_str(state);
    json["sensor"] = to_c_str(sensor->get_state());
    json["boost"] = boost_active();
    if (valve) {
        json["valve"] = to_c_str(valve->get_state());
    }

    return json;
}

String Zone::unique_id() const {
    // TODO: Cache this?
    return sha1(String(name)).substring(0, 7);
}

bool Zone::healthcheck() const { return state != State::error; }

void Zone::boost(double timeout_seconds) {
    boost_stopwatch.reset();
    boost_timeout = timeout_seconds;
}

bool Zone::boost_active() const {
    return boost_stopwatch.elapsed() < boost_timeout;
}

double Zone::get_reading() const { return sensor->get_reading(); }

Zone::State Zone::get_state() const { return state; }
