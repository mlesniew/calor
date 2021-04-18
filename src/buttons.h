#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <utils/stopwatch.h>

enum class Button : uint8_t { none, up, down, left, right, ok };

class Buttons {
    enum class State { no_event, event_pending, event_fired };
    public:
        Buttons(): last_button(Button::none), state(State::no_event) {}
        void tick();
        Button get_button();

    protected:
        Button get_current_button();
        Button last_button;
        Stopwatch button_hold_time;
        State state;
};

#endif
