#include <cstdint>
#include <Arduino.h>
#include <Hash.h>

#include <PicoPrometheus.h>
#include <PicoSyslog.h>
#include <ArduinoJson.h>

#include "sensor.h"
#include "schalter.h"
#include "zone.h"

extern PicoPrometheus::Registry prometheus;
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
      desired(json["desired"] | 21),
      hysteresis(json["hysteresis"] | 0.5),
      state(State::init),
      sensor(create_sensor(json["sensor"])),
      valve(get_schalter(json["valve"])) {

    // setup metrics
    static PicoPrometheus::Gauge zone_state(prometheus, "zone_state", "Zone state enum");
    static PicoPrometheus::Gauge zone_temperature_desired(prometheus, "zone_temperature_desired",
            "Zone's desired temperature");
    static PicoPrometheus::Gauge zone_temperature_hysteresis(prometheus, "zone_temperature_desired_hysteresis",
            "Zone's desired temperature hysteresis");
    static PicoPrometheus::Gauge zone_temperature_reading(prometheus, "zone_temperature_reading",
            "Zone's actual temperature");
    static PicoPrometheus::Gauge zone_valve_state(prometheus, "zone_valve_state", "Zone's valve state enum");
    static PicoPrometheus::Gauge zone_enabled(prometheus, "zone_enabled", "Zone enabled flag");

    const PicoPrometheus::Labels labels = {{"zone", name.c_str()}};
    zone_state[labels].bind([this] {
        return static_cast<typename std::underlying_type<State>::type>(state);
    });
    zone_temperature_desired[labels].bind(desired);
    zone_temperature_hysteresis[labels].bind(hysteresis);
    zone_temperature_reading[labels].bind([this] {
        return sensor->get_reading();
    });
    if (valve) {
        zone_valve_state[labels].bind([this] {
            return static_cast<typename std::underlying_type<Schalter::State>::type>(Schalter::State(valve->get_state()));
        });
    }
    zone_enabled[labels].bind([this] {
        return enabled ? 1 : 0;
    });
}

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
        syslog.printf("Zone '%s' changing state from %s to %s.\n", name.c_str(), to_c_str(state), to_c_str(new_state));
        state = new_state;
    };

    if (sensor->get_state() == AbstractSensor::State::error || (valve && valve->get_state() == Schalter::State::error)) {
        set_state(State::error);
        return;
    }

    if (sensor->get_state() == AbstractSensor::State::init || (valve && valve->get_state() == Schalter::State::init)) {
        if (state != State::init) {
            set_state(State::init);
        }
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
    return enabled && (state == State::heat) && (!valve || (valve->get_state() == Schalter::State::active));
}

DynamicJsonDocument Zone::get_config() const {
    DynamicJsonDocument json(512);

    json["desired"] = desired;
    json["hysteresis"] = hysteresis;
    json["sensor"] = sensor->get_config();
    if (valve) {
        json["valve"] = valve->get_config();
    }
    json["enabled"] = enabled;

    return json;
}

DynamicJsonDocument Zone::get_status() const {
    DynamicJsonDocument json(256);

    json["desired"] = desired;
    json["hysteresis"] = hysteresis;
    json["enabled"] = enabled;
    json["reading"] = get_reading();
    json["state"] = to_c_str(state);
    json["sensor"] = to_c_str(sensor->get_state());
    if (valve) {
        json["valve"] = to_c_str(valve->get_state());
    }

    return json;
}

String Zone::unique_id() const {
    // TODO: Cache this?
    return sha1(String(name)).substring(0, 7);
}

bool Zone::healthcheck() const {
    return state != State::error;
}

double Zone::get_reading() const {
    return sensor->get_reading();
}

Zone::State Zone::get_state() const {
    return state;
}
