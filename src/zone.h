#ifndef ZONE_H
#define ZONE_H

#include <ArduinoJson.h>

#include <utils/tickable.h>
#include <utils/timedvalue.h>

#include "valvestate.h"
#include "zonestate.h"


class Zone: public Tickable {
    public:
        Zone();
        Zone(const Zone & zone);
        const Zone & operator=(const Zone & zone);

        void tick();

        bool valve_desired_state() const;
        bool boiler_desired_state() const;

        DynamicJsonDocument get_config() const;
        DynamicJsonDocument get_status() const;
        bool set_config(const JsonVariantConst & json);

        TimedValue<double> reading;
        TimedValue<ValveState> valve_state;

        double desired, hysteresis;

    protected:
        ZoneState state;
};

#endif
