#include "valvestate.h"
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

const char * to_c_str(const ValveState & s) {
    switch (s) {
        case ValveState::open:
            return "open";
        case ValveState::closed:
            return "closed";
        case ValveState::opening:
            return "opening";
        case ValveState::closing:
            return "closing";
        default:
            return "error";
    }
}

ValveState parse_valve_state(const std::string & s) {
    if (s == "closed") { return ValveState::closed; }
    if (s == "closing") { return ValveState::closing; }
    if (s == "open") { return ValveState::open; }
    if (s == "opening") { return ValveState::opening; }
    return ValveState::error;
}

