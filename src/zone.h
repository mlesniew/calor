#ifndef ZONE_H
#define ZONE_H

#include <ArduinoJson.h>

#include <utils/tickable.h>
#include <utils/stopwatch.h>

enum class ZoneState { init, off, on, error };

template <typename T>
class ValueWithStopwatch {
public:
    ValueWithStopwatch(const T & value)
        : value(value) {
    }

    T & operator=(const T & value) {
        stopwatch.reset();
        return this->value = value;
    }

    operator T() const {
        return value;
    }

    unsigned long elapsed_millis() const {
        return stopwatch.elapsed_millis();
    }

private:
    T value;
    Stopwatch stopwatch;
};

class Zone: public Tickable {
public:
    Zone();
    Zone(const Zone & zone);
    const Zone & operator=(const Zone & zone);

    void tick();

    bool get_boiler_state() const;

    DynamicJsonDocument to_json() const;
    bool load(const JsonVariant & json);

    ValueWithStopwatch<double> reading;
    double desired, hysteresis;

protected:
    ZoneState state;
};

#endif