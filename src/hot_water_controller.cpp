#include "hot_water_controller.h"
HotWaterController::HotWaterController(
    const Clock & clock,
    const TemperatureSensorControllerInterface & sensors,
    const std::function<void(bool)> & set_output,
    double histeresis):
    Periodic(10), set_output(set_output), clock(clock), sensors(sensors), state(State::invalid), histeresis(histeresis) {}

void HotWaterController::init() {
    set_output(state == State::on);
}

double HotWaterController::get_reading() const {
    return sensors.get_reading(0);
}

void HotWaterController::periodic_proc() {
    const double reading = get_reading();
    uint8_t h = 0;
    uint8_t m = 0;
    double desired = 0;

    State new_state = state;

    if ((reading <= 0) || (reading >= 80)) {
        // invalid reading, stop heating for safety
        new_state = State::invalid;
    } else if (reading <= 5) {
        // start heating to avoid freezing
        new_state = State::on;
    } else if (!clock.ready()) {
        // we don't know the time
        new_state = State::invalid;
    } else {
        const auto time = clock.get_time();
        h = time.get_hours();
        m = time.get_minutes();
        desired = get(time);

        switch (state) {
            case State::on:
                if (reading >= desired + histeresis)
                    new_state = State::off;
                break;
            case State::invalid:
                new_state = State::off;
            case State::off:
                if (reading <= desired - histeresis)
                    new_state = State::on;
                break;
        }
    }

    Serial.printf("%02i:%02i water temperature %.2f, desired %.2f, ", h, m, reading, desired);
    if (new_state != state) {
        Serial.println(new_state == State::on ? F("starting heating") : F("stopping heating"));
        state = new_state;
        set_output(state == State::on);
    } else {
        Serial.println(new_state == State::on ? F("heating stays on") : F("heating stays off"));
    }
}
