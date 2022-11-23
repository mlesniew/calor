#ifndef ZONE_H
#define ZONE_H

#include <ArduinoJson.h>

#include <utils/tickable.h>
#include <utils/stopwatch.h>

enum class ZoneState { init, off, on, open_valve, close_valve, error };

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

    bool valve_desired_state() const;
    bool boiler_desired_state() const;

    DynamicJsonDocument to_json() const;
    bool load(const JsonVariant & json);

    ValueWithStopwatch<double> reading;
    ValueWithStopwatch<bool> valve_open;

    double desired, hysteresis;

protected:
    ZoneState state;
};

#endif
