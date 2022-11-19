#include <limits>

#include "heating.h"

Heating::Heating(const std::initializer_list<std::string> & zone_names)
    : Periodic(5, 4) {
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
