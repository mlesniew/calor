#include <PicoMQTT.h>
#include <PicoSyslog.h>

#include "sensor.h"

extern PicoSyslog::Logger syslog;
extern PicoMQTT::Server mqtt;

const char * to_c_str(const Sensor::State & s) {
    switch (s) {
        case Sensor::State::init:
            return "init";
        case Sensor::State::ok:
            return "ok";
        default:
            return "error";
    }
}

void Sensor::set_state(State new_state) {
    if (state == new_state) {
        return;
    }
    syslog.printf("Sensor %s changing state from %s to %s.\n", str().c_str(), to_c_str(state), to_c_str(new_state));
    state = new_state;
}

DummySensor::DummySensor() {
    set_state(State::error);
}

void DummySensor::tick() {}

DynamicJsonDocument DummySensor::get_config() const {
    DynamicJsonDocument json(0);
    return json;
}

KelvinSensor::KelvinSensor(const String & address)
    : address(address), reading(std::numeric_limits<double>::quiet_NaN()) {

    String addr = address;
    addr.toLowerCase();
    mqtt.subscribe("celsius/+/" + addr, [this](const char *, Stream & stream) {
        StaticJsonDocument<512> json;
        if (deserializeJson(json, stream) || !json.containsKey("temperature")) {
            return;
        }
        reading = json["temperature"].as<double>();
        Serial.printf("Temperature update for sensor %s: %.2f ºC\n", this->address.c_str(), (double) reading);
        set_state(State::ok);
    });
}

void KelvinSensor::tick() {
    if (reading.elapsed_millis() >= 2 * 60 * 1000) {
        set_state(State::error);
        reading = std::numeric_limits<double>::quiet_NaN();
    }
}

double KelvinSensor::get_reading() const { return reading; }

DynamicJsonDocument KelvinSensor::get_config() const {
    DynamicJsonDocument json(64);
    json["type"] = "kelvin";
    json["address"] = address;
    return json;
}

Sensor * create_sensor(const JsonVariantConst & json) {
    if (json.is<const char *>()) {
        return new KelvinSensor(json.as<const char *>());
    } else if (json["type"] == "kelvin") {
        return new KelvinSensor(json["address"] | "");
    } else {
        return new DummySensor();
    }
}
