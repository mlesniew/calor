#include "celsius.h"
#include "celsius_reader.h"

CelsiusReader::CelsiusReader(std::function<void(std::string, double)> callback, std::vector<std::string> addresses)
    : Periodic(60, 3), callback(callback), addresses(addresses) {
}

void CelsiusReader::periodic_proc() {
    if (!callback)
        return;

    for (const auto & address: addresses) {
        const auto readings = get_celsius_readings(address);
        for (const auto & kv : readings) {
            printf("Temperature in %s = %.2f ºC\n", kv.first.c_str(), kv.second);
            callback(kv.first, kv.second);
        }
    }
}
