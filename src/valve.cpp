#include "valve.h"

Valve::Valve(BinaryOutput & output, const std::string & name, const unsigned long switch_time_millis)
    : name(name), switch_time_millis(switch_time_millis),
      demand_open(false), output(output), state(ValveState::closed) {
}

void Valve::tick() {
    const bool switch_time_elapsed = (state.elapsed_millis() >= switch_time_millis);

    switch (state) {
        case ValveState::closing:
            if (!demand_open && switch_time_elapsed) {
                state = ValveState::closed;
                printf("Valve '%s' switching state to '%s'.\n", name.c_str(), to_c_str(state));
            }
        // fall through

        case ValveState::closed:
            if (demand_open) {
                state = ValveState::opening;
                printf("Valve '%s' switching state to '%s'.\n", name.c_str(), to_c_str(state));
            }
            break;

        case ValveState::opening:
            if (demand_open && switch_time_elapsed) {
                state = ValveState::open;
                printf("Valve '%s' switching state to '%s'.\n", name.c_str(), to_c_str(state));
            }
        // fall through

        case ValveState::open:
            if (!demand_open) {
                state = ValveState::closing;
                printf("Valve '%s' switching state to '%s'.\n", name.c_str(), to_c_str(state));
            }
            break;

        default:
            break;
    }

    output.set(demand_open);
}

DynamicJsonDocument Valve::get_config() const {
    DynamicJsonDocument json(64);

    json["name"] = name;
    json["switch_time"] = double(switch_time_millis) * 0.001;

    return json;
}

DynamicJsonDocument Valve::get_status() const {
    DynamicJsonDocument json = get_config();

    json["state"] = to_c_str(state);

    return json;
}

bool Valve::set_config(const JsonVariantConst & json) {
    const auto object = json.as<JsonObjectConst>();

    if (object.containsKey("name")) {
        name = object["name"].as<std::string>();
    }

    if (object.containsKey("switch_time")) {
        switch_time_millis = object["switch_time"].as<double>() * 1000;
    }

    return true;
}

