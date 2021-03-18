#ifndef CLOCK_H
#define CLOCK_H

#include <NTPClient.h>
#include <WiFiUdp.h>

#include "time.h"

class Clock {
    public:
        Clock() : ntp_udp(), ntp_client(ntp_udp), have_time(false) {}

        void init() {
            ntp_client.begin();
        }

        void tick() {
            have_time = have_time || ntp_client.update();
        }

        bool ready() const {
            return have_time;
        }

        Time get_time() const {
            auto hour_minute = (ntp_client.getEpochTime() / 60) % (24 * 60);
            return Time(hour_minute / 60, hour_minute % 60);
        }

    protected:
        WiFiUDP ntp_udp;
        NTPClient ntp_client;
        bool have_time;
};

#endif
