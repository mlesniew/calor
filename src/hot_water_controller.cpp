#include "clock.h"
#include "hot_water_controller.h"
#include "presence.h"

HotWaterController::HotWaterController(
    const Clock & clock,
    const TemperatureSensorControllerInterface & sensors,
    const PresenceController & presence_controller,
    const std::function<void(bool)> & set_output,
    double histeresis):
    Periodic(10), set_output(set_output), clock(clock),
    sensors(sensors), presence_controller(presence_controller),
    state(false), histeresis(histeresis), reason("init") {}

void HotWaterController::init() {
    set_output(state);
}

double HotWaterController::get_desired() const {
    if (!clock.ready())
        return 0;
    return get(clock.get_time());
}

double HotWaterController::get_reading() const {
    return sensors.get_reading(0);
}

void HotWaterController::periodic_proc() {
    const double reading = get_reading();
    double desired = 0;

    bool new_state = state;

    if ((reading <= 0) || (reading >= 80)) {
        // invalid reading, stop heating for safety
        new_state = false;
        reason = "reading error";
    } else if (reading <= 5) {
        // start heating to avoid freezing
        new_state = true;
        reason = "anti-freeze";
    } else if (!clock.ready()) {
        // we don't know the time
        new_state = false;
        reason = "clock error";
    } else if (presence_controller.inactivity_time_hours() >= 1) {
        // nobody home
        new_state = false;
        reason = "nobody home";
    } else {
        const auto time = clock.get_time();
        desired = get(time);

        if (state) {
            if (reading >= desired + histeresis)
                new_state = true;
        } else {
            if (reading <= desired - histeresis)
                new_state = false;
        }
        reason = "program";
    }

    Serial.printf("water temperature %.2f, desired %.2f, heating = %s, reason = %s\n",
            reading, desired, new_state ? "on" : "off", reason.c_str());

    if (new_state != state) {
        state = new_state;
        set_output(state);
    }
}
