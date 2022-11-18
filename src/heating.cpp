#include <cmath>
#include <limits>

#include "heating.h"

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
               (double) zone.reading, zone.desired, zone.hysteresis
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
