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
        Zone(const Zone &) = delete;
        Zone & operator=(const Zone &) = delete;

        void tick();

        bool valve_desired_state() const;
        bool boiler_desired_state() const;

        DynamicJsonDocument get_config() const;
        DynamicJsonDocument get_status() const;

        const String sensor;
        const double hysteresis;

        PicoUtils::TimedValue<double> reading;
        double desired;

        PicoUtils::TimedValue<ValveState> valve_state;

        String unique_id() const;

        bool healthcheck() const;

    protected:
        void update_mqtt() const override;
        virtual const char * get_class_name() const override { return "Zone"; }

        PicoUtils::PeriodicRun mqtt_updater;
};

const char * to_c_str(const ZoneState & s);

#endif
