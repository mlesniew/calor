#include <cmath>

#include <ArduinoJson.h>

#include "zone.h"

Zone::Zone()
    : reading(std::numeric_limits<double>::quiet_NaN()), valve_state(ValveState::open),
      desired(21.0), hysteresis(0.5), state(ZoneState::init) {
}

Zone::Zone(const Zone & other)
    : Zone() {
    *this = other;
}

const Zone & Zone::operator=(const Zone & other) {
    desired = other.desired;
    hysteresis = other.hysteresis;
    return *this;
}

void Zone::tick() {
    const bool reading_timeout = reading.elapsed_millis() >= 2 * 60 * 1000;
    const bool valve_timeout = valve_state.elapsed_millis() >= 2 * 60 * 1000;

    if (valve_timeout) {
        valve_state = ValveState::error;
    }

    if (reading_timeout) {
        reading = std::numeric_limits<double>::quiet_NaN();
    }

    // FSM inputs
    const bool comms_timeout = (reading_timeout || valve_timeout);
    const bool error = (valve_state == ValveState::error) || std::isnan(reading);
    const bool warm = reading >= desired + hysteresis;
    const bool cold = reading <= desired - hysteresis;

    switch (state) {
        case ZoneState::init:
            if (comms_timeout) {
                state = ZoneState::error;
            } else if (!error) {
                state = ZoneState::off;
            }
            break;

        case ZoneState::error:
            state = error ? ZoneState::error : ZoneState::off;
            break;

        case ZoneState::off:
        case ZoneState::close_valve:
            if (error) {
                state = ZoneState::error;
            } else if (cold) {
                state = ZoneState::open_valve;
            } else {
                state = (valve_state == ValveState::open) ? ZoneState::close_valve : ZoneState::off;
            }
            break;

        case ZoneState::on:
        case ZoneState::open_valve:
            if (error) {
                state = ZoneState::error;
            } else if (warm) {
                state = ZoneState::close_valve;
            } else {
                state = (valve_state == ValveState::open) ? ZoneState::on : ZoneState::open_valve;
            }
            break;

        default:
            state = ZoneState::error;
    }
}

bool Zone::boiler_desired_state() const {
    return (state == ZoneState::on);
}

bool Zone::valve_desired_state() const {
    return (state == ZoneState::on) || (state == ZoneState::open_valve);
}

DynamicJsonDocument Zone::get_config() const {
    DynamicJsonDocument json(64);

    json["desired"] = desired;
    json["hysteresis"] = hysteresis;

    return json;
}

DynamicJsonDocument Zone::get_status() const {
    DynamicJsonDocument json = get_config();

    json["reading"] = double(reading);
    json["state"] = to_c_str(state);

    return json;
}

bool Zone::set_config(const JsonVariantConst & json) {
    const auto obj = json.as<JsonObjectConst>();

    if (obj.isNull()) {
        return false;
    }

    // TODO: validate
    if (obj.containsKey("desired")) {
        desired = obj["desired"];
    }

    if (obj.containsKey("hysteresis")) {
        hysteresis = obj["hysteresis"];
    }

    return true;
}
