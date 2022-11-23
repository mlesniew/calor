#ifndef VALVOLA_H
#define VALVOLA_H

#include <map>
#include <string>

std::map<std::string, bool> update_valvola(
    const std::string & address,
    const std::map<std::string, bool> desired);

#endif
