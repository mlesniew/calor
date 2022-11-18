#include <cmath>
#include <limits>

#include "heating.h"

Zone::Zone()
    : desired(21.0), hysteresis(0.5), state(ZoneState::init), reading(std::numeric_limits<double>::quiet_NaN()) {
}

void Zone::update_reading(double new_reading) {
    if (std::isnan(new_reading)) {
        // reading useless, ignore
        return;
    }

    reading = new_reading;
    last_reading_time.reset();

    switch (state) {
    case ZoneState::init:
    case ZoneState::error:
        state = ZoneState::off;
    default:
        /* noop */
        ;
    }
}

void Zone::tick() {
    if (last_reading_time.elapsed() >= 2 * 60 * 1000) {
        state = ZoneState::error;
        reading = std::numeric_limits<double>::quiet_NaN();
        return;
    }

    switch (state) {
    case ZoneState::init:
    case ZoneState::error:
        break;
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
    };
}

bool Zone::get_boiler_state() const {
    return state == ZoneState::on;
}

Heating::Heating(const std::initializer_list<std::string> & zone_names)
    : Periodic(5) {
    for (const auto & zone_name : zone_names) {
        zones[zone_name] = Zone();
    }
}

void Heating::periodic_proc() {
    burner = false;

    printf("Checking %i zones...\n", zones.size());
    for (auto & kv : zones) {
        const auto & name = kv.first;
        auto & zone = kv.second;
        zone.tick();
        burner = burner || zone.get_boiler_state();
        printf("  %s: %s  reading %.2f ºC; desired %.2f ºC ± %.2f ºC\n",
               name.c_str(), zone.get_boiler_state() ? "ON": "OFF",
               zone.get_reading(), zone.desired, zone.hysteresis
              );
    };
    printf("Zone processing complete, burner status: %s\n", burner ? "ON" : "OFF");
}

Zone * Heating::get(const std::string & name) {
    const auto it = zones.find(name);
    if (it == zones.end()) {
        return nullptr;
    }
    return &it->second;
}
