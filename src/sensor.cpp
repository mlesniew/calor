#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

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

double CelsiusDevice::get_reading(const String & name) const {
    const auto it = readings.find(name);
    if (it != readings.end()) {
        return it->second;
    } else {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

void CelsiusDevice::periodic_proc() {
    const String uri = "http://" + address + "/temperature.json";

    syslog.printf("Updating Celsius %s (%s)...\n", address.c_str(), uri.c_str());

    readings.clear();
    if (WiFi.status() != WL_CONNECTED) {
        syslog.printf("Celsius %s update failed: no WiFi connection.\n", address.c_str());
        return;
    }

    WiFiClient client;
    HTTPClient http;

    // this disables chunked transfer encoding
    http.useHTTP10(true);

    // set timeout
    client.setTimeout(5000);

    if (!http.begin(client, uri.c_str())) {
        syslog.printf("Celsius %s update failed: connection error.\n", address.c_str());
        return;
    }

    const int code = http.GET();
    syslog.printf("Celsius %s responded with code %i.\n", address.c_str(), code);

    if (!code) {
        return;
    }

    if (code >= 200 && code < 300) {
        StaticJsonDocument<256> doc;

        DeserializationError error = deserializeJson(doc, http.getStream());

        if (error) {
            syslog.printf("Celsius %s update failed: JSON parsing failed.\n", address.c_str());
            syslog.println(error.f_str());
        } else {
            for (JsonPair kv : doc.as<JsonObject>()) {
                const String key{kv.key().c_str()};
                const double value = kv.value().as<double>();
                readings[key] = value;
                syslog.printf("  %s = %.2f\n", key.c_str(), value);
            }
        }
    }

    http.end();
}

CelsiusDevice & CelsiusSensor::get_device(const String & address) {
    static std::list<CelsiusDevice> devices;

    for (auto & dev : devices) {
        if (dev.address == address) {
            return dev;
        }
    }
    // not found, create one
    devices.push_back(CelsiusDevice(address));
    return devices.back();
}

CelsiusSensor::CelsiusSensor(const String & address, const String & name): name(name), device(get_device(address)) {
}

void CelsiusSensor::tick() {
    device.tick();
    set_state(std::isnan(device.get_reading(name)) ? State::error : State::ok);
}

double CelsiusSensor::get_reading() const {
    return device.get_reading(name);
}

DynamicJsonDocument CelsiusSensor::get_config() const {
    DynamicJsonDocument json(64);
    json["type"] = "celsius";
    json["address"] = device.address;
    json["name"] = name;
    return json;
}

Sensor * create_sensor(const JsonVariantConst & json) {
    if (json.is<const char *>()) {
        return new KelvinSensor(json.as<const char *>());
    } else if (json["type"] == "kelvin") {
        return new KelvinSensor(json["address"] | "");
    } else if (json["type"] == "celsius") {
        return new CelsiusSensor(json["address"] | "", json["name"] | "");
    } else {
        return new DummySensor();
    }
}
