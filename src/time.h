#ifndef TIME_H
#define TIME_H

#include <stdint.h>

struct Time {
    Time(uint16_t time) : time(time) {}
    Time(uint8_t hours, uint8_t minutes) : time(uint16_t(hours) << 8 | uint16_t(minutes)) {}
    uint8_t get_hours() const {
        return time >> 8;
    }
    uint8_t get_minutes() const {
        return time & 0xff;
    }
    uint16_t time;

    bool operator==(const Time & other) const {
        return time == other.time;
    }

    bool operator!=(const Time & other) const {
        return time != other.time;
    }

    bool operator<(const Time & other) const {
        return time < other.time;
    }

    bool operator>(const Time & other) const {
        return time > other.time;
    }

    bool operator<=(const Time & other) const {
        return time <= other.time;
    }

    bool operator>=(const Time & other) const {
        return time >= other.time;
    }
};

#endif
