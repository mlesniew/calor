#include "buttons.h"

void Buttons::tick() {
    const auto button = get_current_button();
    if (button != last_button) {
        button_hold_time.reset();
        last_button = button;
        state = State::no_event;
    } else if (button != Button::none && state == State::no_event && button_hold_time.elapsed_millis() >= 1) {
        state = State::event_pending;
    }
}

Button Buttons::get_button() {
    if (state != State::event_pending)
        return Button::none;

    state = State::event_fired;
    return last_button;
}

Button Buttons::get_current_button() {
    struct Mapping { Button button; int value; };
    static const Mapping mapping[5] = {
        { Button::left, 7 },
        { Button::up, 150 },
        { Button::down, 297 },
        { Button::right, 415 },
        { Button::ok, 546 },
    };
    static const int tolerance = 30;

    const auto reading = analogRead(A0);

    for (int i = 0; i < std::extent<decltype(mapping)>::value; ++i) {
        if ((reading >= mapping[i].value - tolerance) && (reading <= mapping[i].value + tolerance)) {
            return mapping[i].button;
        }
    }

    return Button::none;
}

