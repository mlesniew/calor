#include "zonestate.h"

const char * to_c_str(const ZoneState & s) {
    switch (s) {
        case ZoneState::init:
            return "init";
        case ZoneState::on:
            return "heat";
        case ZoneState::off:
            return "wait";
        case ZoneState::open_valve:
            return "open valve";
        case ZoneState::close_valve:
            return "close valve";
        default:
            return "error";
    }
}
