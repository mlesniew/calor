#ifndef HEATING_H
#define HEATING_H

#include <initializer_list>
#include <string>
#include <map>

#include <utils/periodic.h>
#include <utils/stopwatch.h>

#include "zone.h"

struct Heating: public Periodic {
public:
    Heating(const std::initializer_list<std::string> & zone_names);
    void periodic_proc();

    Zone * get(const std::string & name);

private:
    bool burner;
    std::map<std::string, Zone> zones;
};

#endif
