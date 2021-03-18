#include <Arduino.h>
#include <LittleFS.h>

#include <utils/led.h>
#include <utils/periodic.h>
#include <utils/shift_register.h>
#include <utils/wifi_control.h>

#include "program.h"
#include "clock.h"
#include "wifi_readings.h"
#include "ds18b20.h"

BlinkingLed wifi_led(D4, 0, 91, true);
WiFiControl wifi_control(wifi_led);
WiFiReadings wifi_readings;
ShiftRegister<1> shift_register(
    D6,  // data pin
    D5,  // clock pin
    D0,  // latch pin
(uint8_t[1]) {
    0b00001111,
}  // inverted outputs
);

TemperatureSensors temperature_sensors(D7);
DeviceAddress temperature_sensor_address[4];
uint8_t sensor_count;

Clock ntp_clock;

class HotWaterController: public TimeProgram<uint8_t, 10>, public Periodic {

    public:
        enum class State : uint8_t { off, on, invalid };

        HotWaterController(const Clock & clock, double histeresis):
            Periodic(10), clock(clock), state(State::invalid), histeresis(histeresis) {}

        void init() {
            set_output(state);
        }

        virtual double get_reading() const {
            return temperature_sensors.read(temperature_sensor_address[0]);
        }

        void periodic_proc() override {
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
                set_output(state);
            } else {
                Serial.println(new_state == State::on ? F("heating stays on") : F("heating stays off"));
            }
        }

    protected:
        virtual void set_output(const State state) const {
            shift_register.write(3, state == State::on);
        }

        const Clock & clock;
        State state;
        double histeresis;
};


HotWaterController hot_water_program(ntp_clock, 2.5);


void setup() {
    Serial.begin(9600);
    Serial.println(F("Calor " __DATE__ " " __TIME__));

    shift_register.init();
    temperature_sensors.init();

    LittleFS.begin();

    wifi_control.init();

    uint8_t idx = 0;
    sensor_count = 0;
    while ((idx = temperature_sensors.scan(temperature_sensor_address[sensor_count], idx)) && (sensor_count < 4)) {
        Serial.print(F("Found sensor: "));
        for (uint8_t j = 0; j < 8; ++j) {
            Serial.print(temperature_sensor_address[sensor_count][j] >> 4, HEX);
            Serial.print(temperature_sensor_address[sensor_count][j] & 0xf, HEX);
        }
        Serial.print(F("\n"));
        ++sensor_count;
    }

    ntp_clock.init();

    hot_water_program.init();
    hot_water_program.add(Time{7, 0}, 40);
    hot_water_program.add(Time{8, 30}, 30);
    hot_water_program.add(Time{17, 30}, 45);
    hot_water_program.add(Time{20, 30}, 0);
}


void loop() {
    wifi_control.tick();
    ntp_clock.tick();

    // Serial.println(WiFi.localIP());
    // wifi_readings.load("http://192.168.1.200/measurements.json");

    temperature_sensors.update();

    hot_water_program.tick();
}
