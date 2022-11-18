#include <initializer_list>
#include <string>
#include <map>

#include <utils/periodic.h>
#include <utils/stopwatch.h>
#include <utils/tickable.h>

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
        return stopwatch.elapsed();
    }

private:
    T value;
    Stopwatch stopwatch;
};

class Zone: public Tickable {
public:
    Zone();

    void tick();

    bool get_boiler_state() const;

    ValueWithStopwatch<double> reading;
    double desired, hysteresis;

protected:
    ZoneState state;
};

struct Heating: public Periodic {
public:
    Heating(const std::initializer_list<std::string> & zone_names);
    void periodic_proc();

    Zone * get(const std::string & name);

private:
    bool burner;
    std::map<std::string, Zone> zones;
};
