#include <initializer_list>
#include <string>
#include <map>

#include <utils/stopwatch.h>

enum class ZoneState { init, off, on, error };

class Zone {
public:
    Zone();

    void tick();

    void set_reading(double new_reading);
    void set_desired(double new_desired);
    void set_hysteresis(double new_hysteresis);

    double get_reading() const {
        return reading;
    }
    double get_desired() const {
        return desired;
    }
    double get_hysteresis() const {
        return hysteresis;
    }

    bool get_boiler_state() const;

protected:
    void process();

    ZoneState state;
    double desired, reading, hysteresis;
    Stopwatch last_update;
};


struct Heating {
public:
    Heating(const std::initializer_list<std::string> & zone_names);
    void tick();
    void set_reading(const std::string & name, double temperature);

private:
    bool burner;
    std::map<std::string, Zone> zones;
};
