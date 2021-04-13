#include "buttons.h"

void Buttons::tick() {
    const auto button = get_current_button();
    if (button != last_button) {
        button_hold_time.reset();
        last_button = button;
        event_fired = false;
    } else if (event_fired || button == Button::none) {
        return;
    } else if (button_hold_time.elapsed_millis() > 24) {
        on_press(button);
        event_fired = true;
    }
}

void Buttons::on_press(Button button) {
    Serial.print("Button pressed: ");
    switch (button) {
        case Button::left:
            Serial.println("Left");
            break;
        case Button::right:
            Serial.println("Right");
            break;
        case Button::up:
            Serial.println("Up");
            break;
        case Button::down:
            Serial.println("Down");
            break;
        case Button::ok:
            Serial.println("OK");
            break;
    }
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

