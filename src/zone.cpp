#include <cmath>

#include <ArduinoJson.h>
#include <PicoMQTT.h>

#include "zone.h"

PicoMQTT::Publisher & get_mqtt_publisher();
Prometheus & get_prometheus();

namespace {
PrometheusGauge zone_state(get_prometheus(), "zone_state", "Zone state enum");
PrometheusGauge zone_desired_temperature(get_prometheus(), "zone_temperature_desired", "Zone's desired temperature");
PrometheusGauge zone_desired_temperature_hysteresis(get_prometheus(), "zone_temperature_desired_hysteresis",
        "Zone's desired temperature hysteresis");
PrometheusGauge zone_actual_temperature(get_prometheus(), "zone_temperature_actual", "Zone's actual temperature");
PrometheusGauge zone_valve_state(get_prometheus(), "zone_valve_state", "Zone's valve state enum");
}

Zone::Zone(const char * name)
    : NamedFSM(name, ZoneState::init),
      reading(std::numeric_limits<double>::quiet_NaN()), valve_state(ValveState::error),
      desired(21.0), hysteresis(0.5) {
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

    switch (get_state()) {
        case ZoneState::init:
            if (comms_timeout) {
                set_state(ZoneState::error);
            } else if (!error) {
                set_state(ZoneState::off);
            }
            break;

        case ZoneState::error:
            set_state(error ? ZoneState::error : ZoneState::off);
            break;

        case ZoneState::off:
        case ZoneState::close_valve:
            if (error) {
                set_state(ZoneState::error);
            } else if (cold) {
                set_state(ZoneState::open_valve);
            } else {
                set_state((valve_state == ValveState::closed) ? ZoneState::off : ZoneState::close_valve);
            }
            break;

        case ZoneState::on:
        case ZoneState::open_valve:
            if (error) {
                set_state(ZoneState::error);
            } else if (warm) {
                set_state(ZoneState::close_valve);
            } else {
                set_state((valve_state == ValveState::open) ? ZoneState::on : ZoneState::open_valve);
            }
            break;

        default:
            set_state(ZoneState::error);
    }

    update_metric();
}

bool Zone::boiler_desired_state() const {
    return (get_state() == ZoneState::on);
}

bool Zone::valve_desired_state() const {
    return (get_state() == ZoneState::on) || (get_state() == ZoneState::open_valve);
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
    json["state"] = to_c_str(get_state());

    return json;
}

bool Zone::set_config(const JsonVariantConst & json) {
    const auto obj = json.as<JsonObjectConst>();

    if (obj.isNull()) {
        return false;
    }

    // TODO: validate
    sensor = obj["sensor"] | "";
    desired = obj["desired"] | 21;
    hysteresis = obj["hysteresis"] | 0.5;

    return true;
}

const char * to_c_str(const ZoneState & s) {
    switch (s) {
        case ZoneState::init:
            return "init";
        case ZoneState::on:
            return "heat";
        case ZoneState::off:
            return "wait";
        case ZoneState::open_valve:
            return "open valve";
        case ZoneState::close_valve:
            return "close valve";
        default:
            return "error";
    }
}

void Zone::update_mqtt() const {
    {
        const std::string topic = std::string("valvola/valve/") + get_name() + "/request";
        get_mqtt_publisher().publish(topic.c_str(), valve_desired_state() ? "open" : "closed");
    }
    {
        const std::string topic = std::string("valvola/valve/") + get_name() + "/state";
        get_mqtt_publisher().publish(topic.c_str(), to_c_str(get_state()));
    }
}

void Zone::delete_metric() const {
    const auto labels = get_prometheus_labels();
    zone_state.remove(labels);
    zone_desired_temperature.remove(labels);
    zone_desired_temperature_hysteresis.remove(labels);
    zone_actual_temperature.remove(labels);
    zone_valve_state.remove(labels);
}

void Zone::update_metric() const {
    const auto labels = get_prometheus_labels();
    zone_state[labels].set(static_cast<typename std::underlying_type<ZoneState>::type>(get_state()));
    zone_desired_temperature[labels].set(desired);
    zone_desired_temperature_hysteresis[labels].set(hysteresis);
    zone_actual_temperature[labels].set(reading);
    zone_valve_state[labels].set(static_cast<typename std::underlying_type<ValveState>::type>
                                 (ValveState(valve_state)));
}
