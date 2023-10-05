#include <cmath>

#include <ArduinoJson.h>
#include <Hash.h>
#include <PicoMQTT.h>

#include "zone.h"

PicoMQTT::Publisher & get_mqtt_publisher();
PicoPrometheus::Registry & get_prometheus();

namespace {
PicoPrometheus::Gauge zone_state(get_prometheus(), "zone_state", "Zone state enum");
PicoPrometheus::Gauge zone_temperature_desired(get_prometheus(), "zone_temperature_desired",
        "Zone's desired temperature");
PicoPrometheus::Gauge zone_temperature_hysteresis(get_prometheus(), "zone_temperature_desired_hysteresis",
        "Zone's desired temperature hysteresis");
PicoPrometheus::Gauge zone_temperature_reading(get_prometheus(), "zone_temperature_reading",
        "Zone's actual temperature");
PicoPrometheus::Gauge zone_valve_state(get_prometheus(), "zone_valve_state", "Zone's valve state enum");
}

Zone::Zone(const char * name, const JsonVariantConst & json)
    : NamedFSM(name, ZoneState::init),
      sensor(json["sensor"] | ""),
      read_only(json["read_only"] | false),
      hysteresis(read_only ? std::numeric_limits<double>::quiet_NaN() : json["hysteresis"] | 0.5),
      reading(std::numeric_limits<double>::quiet_NaN()),
      desired(read_only ? std::numeric_limits<double>::quiet_NaN() : json["desired"] | 21),
      valve_state(ValveState::error) {

    const auto labels = get_prometheus_labels();
    zone_state[labels].bind([this] {
        return static_cast<typename std::underlying_type<ZoneState>::type>(get_state());
    });
    zone_temperature_desired[labels].bind(desired);
    zone_temperature_hysteresis[labels].bind(hysteresis);
    zone_temperature_reading[labels].bind([this] {
        return (double) reading;
    });
    zone_valve_state[labels].bind([this] {
        return static_cast<typename std::underlying_type<ValveState>::type>(ValveState(valve_state));
    });
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

    if (read_only) {
        desired = std::numeric_limits<double>::quiet_NaN();

        if (get_state() != ZoneState::init || reading_timeout || !std::isnan(reading)) {
            set_state(std::isnan(reading) ? ZoneState::error : ZoneState::off);
        }

        return;
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
    json["read_only"] = read_only;
    json["sensor"] = sensor;

    return json;
}

DynamicJsonDocument Zone::get_status() const {
    DynamicJsonDocument json = get_config();

    json["reading"] = double(reading);
    json["state"] = to_c_str(get_state());

    return json;
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
            return "open";
        case ZoneState::close_valve:
            return "close";
        default:
            return "error";
    }
}

void Zone::update_mqtt() const {
    {
        const auto topic = "valvola/valve/" + name + "/request";
        get_mqtt_publisher().publish(topic, valve_desired_state() ? "open" : "closed");
    }
    {
        const auto topic = "valvola/valve/" + name + "/state";
        get_mqtt_publisher().publish(topic, to_c_str(get_state()));
    }
}

String Zone::unique_id() const {
    // TODO: Cache this?
    return sha1(String(name)).substring(0, 7);
}
