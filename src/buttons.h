#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <utils/stopwatch.h>

enum class Button : uint8_t { none, up, down, left, right, ok };

class Buttons {
    public:
        Buttons(): last_button(Button::none), event_fired(false) {}
        void tick();
        virtual void on_press(Button button);

    protected:
        Button get_current_button();
        Button last_button;
        bool event_fired;
        Stopwatch button_hold_time;
};

#endif
