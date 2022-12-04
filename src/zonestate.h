#ifndef ZONESTATE_H
#define ZONESTATE_H

enum class ZoneState {
    init,
    off,
    on,
    open_valve,
    close_valve,
    error,
};

const char * to_c_str(const ZoneState & s);

#endif
