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

    void update_reading(double reading);
    double get_reading() const {
        return reading;
    }

    bool get_boiler_state() const;

    double desired, hysteresis;

protected:
    ZoneState state;
    double reading;
    Stopwatch last_reading_time;
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
