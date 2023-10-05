#ifndef ZONE_H
#define ZONE_H

#include <map>
#include <string>

#include <ArduinoJson.h>
#include <PicoUtils.h>

#include <namedfsm.h>
#include <valve.h>

namespace PicoMQTT {
class Publisher;
};

enum class ZoneState {
    init = 0,
    off = 1,
    on = 2,
    open_valve = 3,
    close_valve = 4,
    error = -1,
};

class Zone: public PicoUtils::Tickable, public NamedFSM<ZoneState> {
    public:
        Zone(const char * name, const JsonVariantConst & json);
        virtual ~Zone() { delete_metric(); }

        void tick();

        bool valve_desired_state() const;
        bool boiler_desired_state() const;

        DynamicJsonDocument get_config() const;
        DynamicJsonDocument get_status() const;

        void set_reading(double value);
        double get_reading() const { return reading; }

        void update(const String & source, double temperature, double rssi = std::numeric_limits<double>::quiet_NaN());

        unsigned long get_seconds_since_last_reading_update() const { return reading.elapsed_millis() / 1000; }

        PicoUtils::TimedValue<ValveState> valve_state;

        const std::string sensor;
        const bool read_only;
        const double hysteresis;

        double desired;

        void update_mqtt() const override;
        void update_metric() const override;

    protected:
        std::map<String, unsigned long> reading_update_time_by_source;
        PicoUtils::TimedValue<double> reading;

        virtual const char * get_class_name() const override { return "Zone"; }

        void delete_metric() const override;

};

const char * to_c_str(const ZoneState & s);

#endif
