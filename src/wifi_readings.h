#ifndef WIFI_READINGS_H
#define WIFI_READINGS_H

#include <map>
#include <string>

#include <utils/stopwatch.h>
#include <utils/periodic.h>

class Stream;

class WiFiReadings : public Periodic {
public:
    struct Readings {
        Readings(): presence(false) {}

        bool presence;
        std::map<std::string, double> temperature;
    };

    WiFiReadings(const std::string & url, float max_age_seconds = 5 * 60)
        : Periodic(15), url(url), max_age_seconds(max_age_seconds) {
    }

    void periodic_proc() override {
        update();
    }

    bool update();

    const Readings & get_readings() const { return readings; }
    bool readings_valid() const { return last_update.elapsed() <= max_age_seconds; }

    std::string url;
    float max_age_seconds;

protected:
    bool update(Stream & stream);

    Stopwatch last_update;
    Readings readings;
};

#endif
