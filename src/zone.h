#ifndef ZONE_H
#define ZONE_H

#include <map>

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

        void tick();

        bool valve_desired_state() const;
        bool boiler_desired_state() const;

        DynamicJsonDocument get_config() const;
        DynamicJsonDocument get_status() const;

        unsigned long get_seconds_since_last_reading_update() const { return reading.elapsed_millis() / 1000; }

        const String sensor;
        const bool read_only;
        const double hysteresis;

        PicoUtils::TimedValue<double> reading;
        double desired;

        PicoUtils::TimedValue<ValveState> valve_state;

        void update_mqtt() const override;
        String unique_id() const;

    protected:
        virtual const char * get_class_name() const override { return "Zone"; }
};

const char * to_c_str(const ZoneState & s);

#endif
