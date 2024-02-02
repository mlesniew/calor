#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <PicoMQ.h>
#include <PicoSyslog.h>

#include "sensor.h"

extern PicoSyslog::Logger syslog;
extern PicoMQ picomq;

const char * to_c_str(const AbstractSensor::State & s) {
    switch (s) {
        case AbstractSensor::State::init:
            return "init";
        case AbstractSensor::State::ok:
            return "ok";
        default:
            return "error";
    }
}

void AbstractSensor::set_state(State new_state) {
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

Sensor::Sensor(const String & address)
    : address(address), reading(std::numeric_limits<double>::quiet_NaN()) {

    picomq.subscribe("celsius/+/" + address + "/temperature", [this](const char *, String payload) {
        reading = payload.toDouble();
        Serial.printf("Temperature update for sensor %s: %.2f ºC\n", this->address.c_str(), (double) reading);
        set_state(State::ok);
    });
}

void Sensor::tick() {
    if (reading.elapsed_millis() >= 2 * 60 * 1000) {
        set_state(State::error);
        reading = std::numeric_limits<double>::quiet_NaN();
    }
}

double Sensor::get_reading() const { return reading; }

DynamicJsonDocument Sensor::get_config() const {
    DynamicJsonDocument json(64);
    json.set(address);
    return json;
}

AbstractSensor * create_sensor(const JsonVariantConst & json) {
    if (json.is<const char *>()) {
        return new Sensor(json.as<const char *>());
    } else if (json["type"] == "kelvin") {
        return new Sensor(json["address"] | "");
    } else {
        return new DummySensor();
    }
}
