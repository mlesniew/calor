#ifndef ZONE_H
#define ZONE_H

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
        Zone(const char * name);
        virtual ~Zone() { delete_metric(); }

        void copy_config_from(const Zone & zone);

        void tick();

        bool valve_desired_state() const;
        bool boiler_desired_state() const;

        DynamicJsonDocument get_config() const;
        DynamicJsonDocument get_status() const;
        bool set_config(const JsonVariantConst & json);

        PicoUtils::TimedValue<double> reading;
        PicoUtils::TimedValue<ValveState> valve_state;

        std::string sensor;
        bool read_only;
        double desired, hysteresis;

        void update_mqtt() const override;
        void update_metric() const override;

    protected:
        virtual const char * get_class_name() const override { return "Zone"; }

        void delete_metric() const override;

};

const char * to_c_str(const ZoneState & s);

#endif
