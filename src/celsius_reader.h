#ifndef CELSIUS_READER
#define CELSIUS_READER

#include <functional>
#include <string>
#include <vector>

#include <utils/periodic.h>

class CelsiusReader: public Periodic {
public:
    CelsiusReader(std::function<void(std::string, double)> callback, std::vector<std::string> addresses = {});

    void periodic_proc() override;

    std::function<void(std::string, double)> callback;
    std::vector<std::string> addresses;
};

#endif
