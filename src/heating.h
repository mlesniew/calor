#include <initializer_list>
#include <string>
#include <map>

#include <utils/periodic.h>
#include <utils/stopwatch.h>
#include <utils/tickable.h>

enum class ZoneState { init, off, on, error };

class Zone: public Tickable {
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
    double reading, desired, hysteresis;
    Stopwatch last_update;
};


struct Heating: public Periodic {
public:
    Heating(const std::initializer_list<std::string> & zone_names);
    void periodic_proc();

    bool set_reading(const std::string & name, double value);
    bool set_desired(const std::string & name, double value);
    bool set_hysteresis(const std::string & name, double value);

private:
    bool burner;
    std::map<std::string, Zone> zones;

    bool zone_run(const std::string & name, std::function<void(Zone &)>);
};
