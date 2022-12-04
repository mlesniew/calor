#ifndef VALVOLA_H
#define VALVOLA_H

#include <map>
#include <string>

#include "valvestate.h"

std::map<std::string, ValveState> update_valvola(
    const std::string & address,
    const std::map<std::string, bool> desired);

#endif
