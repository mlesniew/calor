#ifndef ZONESTATE_H
#define ZONESTATE_H

enum class ZoneState {
    init = 0,
    off = 1,
    on = 2,
    open_valve = 3,
    close_valve = 4,
    error = -1,
};

const char * to_c_str(const ZoneState & s);

#endif
