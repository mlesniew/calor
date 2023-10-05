#ifndef VALVOLA_H
#define VALVOLA_H

#include <map>
#include <string>

#include <valve.h>

std::map<std::string, Valve::State> update_valvola(
    const std::string & address,
    const std::map<std::string, bool> desired);

#endif
