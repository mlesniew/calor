#include <Arduino.h>

namespace {

constexpr auto SR_LTCH = D0;
constexpr auto SR_CLCK = D5;
constexpr auto SR_DATA = D6;

// specifies which outputs are inverted
constexpr uint8_t inverted = 0b00001111;

uint8_t values = 0;

void write() {
    digitalWrite(SR_LTCH, LOW);
    shiftOut(SR_DATA, SR_CLCK, LSBFIRST, values ^ inverted);
    digitalWrite(SR_LTCH, HIGH);
}

}

void shift_register_reset() {
    values = 0;
    write();
}

void shift_register_init() {
    pinMode(SR_LTCH, OUTPUT);
    pinMode(SR_CLCK, OUTPUT);
    pinMode(SR_DATA, OUTPUT);
    shift_register_reset();
}

void shift_register_write(uint8_t idx, bool value) {
    if (value)
        values |= 1 << idx;
    else
        values &= ~(1 << idx);
    write();
}
