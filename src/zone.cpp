#include <cmath>

#include <ArduinoJson.h>

#include "zone.h"
#include "metrics.h"

Zone::Zone(const std::string & name)
    : name(name), reading(std::numeric_limits<double>::quiet_NaN()), valve_state(ValveState::error),
      desired(21.0), hysteresis(0.5), state(ZoneState::init) {
}

Zone::~Zone() {
    metrics::zone_state.remove({{"zone", name}});
    metrics::zone_desired_temperature.remove({{"zone", name}});
    metrics::zone_desired_temperature_hysteresis.remove({{"zone", name}});
    metrics::zone_actual_temperature.remove({{"zone", name}});
    metrics::zone_valve_state.remove({{"zone", name}});
}

void Zone::copy_config_from(const Zone & zone) {
    desired = zone.desired;
    hysteresis = zone.hysteresis;
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
    const bool warm = reading >= desired + 0.5 * hysteresis;
    const bool cold = reading <= desired - 0.5 * hysteresis;

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

    metrics::zone_state[ {{"zone", name}}].set(static_cast<typename std::underlying_type<ZoneState>::type>(state));
    metrics::zone_desired_temperature[ {{"zone", name}}].set(desired);
    metrics::zone_desired_temperature_hysteresis[ {{"zone", name}}].set(hysteresis);
    metrics::zone_actual_temperature[ {{"zone", name}}].set(reading);
    metrics::zone_valve_state[ {{"zone", name}}].set(static_cast<typename std::underlying_type<ValveState>::type>
            (ValveState(valve_state)));
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
