#ifndef PRESENCE_H
#define PRESENCE_H

#include <utils/stopwatch.h>
#include <utils/periodic.h>

#include "wifi_readings.h"

class PresenceController: public Periodic {
public:
    PresenceController(const WiFiReadings & wifi_readings)
        : Periodic(3), wifi_readings(wifi_readings) {}

    void periodic_proc() override {
        // if we don't have valid readings, assume presence
        if (!wifi_readings.readings_valid() || wifi_readings.get_readings().presence) {
            notify_presence();
        }
        Serial.printf("Presence inactivity time: %lu s\n", inactivity_time.elapsed_millis() / 1000);
    }

    double inactivity_time_minutes() const {
        return double(inactivity_time.elapsed_millis()) / 1000.0 / 60.0;
    }

    double inactivity_time_hours() const {
        return inactivity_time_minutes() / 60.0;
    }

    void notify_presence() {
        inactivity_time.reset();
    }

protected:
    const WiFiReadings & wifi_readings;
    Stopwatch inactivity_time;
};

#endif
