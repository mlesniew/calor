#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdint.h>

template <typename ValueType, uint8_t max_size>
class TimeProgram {
    struct Element {
        Element() : time(0), value() {}
        Element(uint8_t hour, uint8_t minute, ValueType value)
            : time(construct_time(hour, minute)), value(value) {}

        uint16_t time;  // 8 bit hour, 8 bit minute
        ValueType value;
    };

    public:
        TimeProgram(): size(0) {}

        ValueType get(uint8_t hour, uint8_t minute) const {
            return get(construct_time(hour, minute));
        }

        bool add(uint8_t hour, uint8_t minute, ValueType value) {
            if ((hour >= 24) || (minute >= 60))
                return false;
            return add(Element{hour, minute, value});
        }

    protected:
        static constexpr uint16_t construct_time(uint8_t hour, uint8_t minute) {
            return uint16_t(hour) << 8 | uint16_t(minute);
        }

        ValueType get(uint16_t time) const {
            ValueType ret = 0;
            for (uint8_t i = 0; i < size; ++i) {
                const auto & p = program[i];
                if (p.time > time)
                    break;
                ret = p.value;
            }
            return ret;
        }

        bool add(const Element & e) {
            // find index to insert e
            uint8_t i = 0;
            while ((i < size) && (e.time > program[i].time))
                ++i;

            if ((size > 0) && (e.time == program[i].time)) {
                // special case: overwrite existing entry
                program[i] = e;
                return true;
            }

            if (size >= max_size)
                return false;

            for (uint8_t j = size; j > i; --j)
                program[j] = program[j - 1];

            program[i] = e;
            ++size;

            return true;
        }

        uint8_t size;
        Element program[max_size];
};

#endif
