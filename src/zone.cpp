#include <cmath>

#include <ArduinoJson.h>

#include "zone.h"

const char * to_c_str(const ZoneState & s) {
    switch (s) {
        case ZoneState::init: return "init";
        case ZoneState::on: return "heat";
        case ZoneState::off: return "wait";
        default: return "error";
    }
}

Zone::Zone()
    : reading(std::numeric_limits<double>::quiet_NaN()), desired(21.0), hysteresis(0.5), state(ZoneState::init) {
}

void Zone::tick() {
    const bool reading_timeout = reading.elapsed_millis() >= 2 * 60 * 1000;

    /* handle errors */
    switch (state) {
    case ZoneState::off:
    case ZoneState::on:
        if (std::isnan(reading)) {
            state = ZoneState::error;
        }
    case ZoneState::init:
        if (reading_timeout) {
            state = ZoneState::error;
        }
    case ZoneState::error:
        if (!std::isnan(reading) && !reading_timeout) {
            state = ZoneState::off;
        }
    default:
        ;
    }

    /* handle temperature control */
    switch (state) {
    case ZoneState::off:
        if (reading <= desired - hysteresis) {
            state = ZoneState::on;
        }
        break;
    case ZoneState::on:
        if (reading >= desired + hysteresis) {
            state = ZoneState::off;
        }
        break;
    default:
        ;
    };
}

bool Zone::get_boiler_state() const {
    return state == ZoneState::on;
}

DynamicJsonDocument Zone::get_json() const {
    DynamicJsonDocument json(64);

    json["desired"] = desired;
    json["hysteresis"] = hysteresis;
    json["reading"] = double(reading);
    json["state"] = to_c_str(state);

    return json;
}
