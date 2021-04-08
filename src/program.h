#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdint.h>
#include "time.h"

template <typename ValueType, uint8_t max_size>
class TimeProgram {
        struct Element {
            Element() : time(0), value() {}
            Element(const Time & time, const ValueType & value)
                : time(time), value(value) {}

            Time time;
            ValueType value;
        };

    public:
        TimeProgram(): size(0) {}

        bool add(const Time & time, const ValueType & value) {
            return add(Element{time, value});
        }

        ValueType get(const Time & time) const {
            ValueType ret = 0;
            for (uint8_t i = 0; i < size; ++i) {
                const auto & p = program[i];
                if (p.time > time)
                    break;
                ret = p.value;
            }
            return ret;
        }

    protected:
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
