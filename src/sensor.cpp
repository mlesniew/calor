#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <PicoMQ.h>
#include <PicoMQTT.h>
#include <PicoSyslog.h>

#include "sensor.h"

extern PicoSyslog::Logger syslog;
extern PicoMQ picomq;
extern PicoMQTT::Server mqtt;

namespace {
std::list<Sensor *> sensors;
}

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

JsonDocument DummySensor::get_config() const {
    JsonDocument json;
    return json;
}

Sensor::Sensor(const String & address)
    : address(address), reading(std::numeric_limits<double>::quiet_NaN()) {

    const String topic = "celsius/+/" + address + "/temperature";
    const auto handler = [this](const char *, String payload) {
        reading = payload.toDouble();
        Serial.printf("Temperature update for sensor %s: %.2f ºC\n", this->address.c_str(), (double) reading);
        set_state(State::ok);
    };

    picomq.subscribe(topic, handler);
    mqtt.subscribe(topic, handler);
}

void Sensor::tick() {
    if (reading.elapsed_millis() >= 5 * 60 * 1000) {
        set_state(State::error);
        reading = std::numeric_limits<double>::quiet_NaN();
    }
}

double Sensor::get_reading() const { return reading; }

JsonDocument Sensor::get_config() const {
    JsonDocument json;
    json.set(address);
    return json;
}

void SensorChain::tick() {
    State new_state = State::error;
    for (AbstractSensor * sensor : sensors) {
        sensor->tick();
        if (new_state == State::error) {
            new_state = sensor->get_state();
        }
    }
    set_state(new_state);
}

double SensorChain::get_reading() const {
    for (AbstractSensor * sensor : sensors) {
        if (sensor->get_state() == State::ok) {
            return sensor->get_reading();
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

String SensorChain::str() const {
    String ret;
    bool first = true;
    for (AbstractSensor * sensor : sensors) {
        if (first) {
            ret = sensor->str();
            first = false;
        } else {
            ret = ret + ", " + sensor->str();
        }
    };
    return "[" + ret + "]";
}

JsonDocument SensorChain::get_config() const {
    JsonDocument json;
    unsigned int idx = 0;
    for (AbstractSensor * sensor : sensors) {
        json[idx++] = sensor->get_config();
    }
    return json;
}

AbstractSensor * get_sensor(const JsonVariantConst & json) {
    if (json.is<const char *>()) {
        const String address = json.as<const char *>();

        for (auto sensor : sensors) {
            if ((sensor->address == address)) {
                return sensor;
            }
        }
        auto sensor = new Sensor(address);
        sensors.push_back(sensor);

        return sensor;
    } else if (json.is<JsonArrayConst>()) {
        std::list<AbstractSensor *> elements;
        for (const JsonVariantConst & value : json.as<JsonArrayConst>()) {
            AbstractSensor * element = get_sensor(value);
            if (element) {
                elements.push_back(element);
            }
        }
        return new SensorChain(elements);
    } else {
        return new DummySensor();
    }
}
