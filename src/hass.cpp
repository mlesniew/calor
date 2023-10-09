#include <vector>

#include <Arduino.h>
#include <PicoSyslog.h>
#include <PicoUtils.h>
#include <ArduinoJson.h>

#include "hass.h"
#include "zone.h"

extern PicoSyslog::Logger syslog;
extern std::vector<Zone *> zones;
extern String hass_autodiscovery_topic;
extern bool healthy;

namespace {

const String board_id(ESP.getChipId(), HEX);

std::list<PicoUtils::WatchInterface *> watches;

}

namespace HomeAssistant {

PicoMQTT::Client mqtt;

void notify_current_temperature(const Zone & zone) {
    mqtt.publish("calor/" + board_id + "/" + zone.unique_id() + "/current_temperature",
                 String(zone.reading),
                 0, true);
}

void notify_desired_temperature(const Zone & zone) {
    mqtt.publish("calor/" + board_id + "/" + zone.unique_id() + "/desired_temperature",
                 String(zone.desired),
                 0, true);
}

void notify_action(const Zone & zone) {
    mqtt.publish("calor/" + board_id + "/" + zone.unique_id() + "/action",
                 zone.boiler_desired_state() ? "heating" : "idle",
                 0, true);
}

void notify_mode(const Zone & zone) {
    mqtt.publish("calor/" + board_id + "/" + zone.unique_id() + "/mode",
                 zone.enabled ? "heat" : "off",
                 0, true);
}

void notify_health() {
    mqtt.publish("calor/" + board_id + "/problem",
                 !healthy ? "ON" : "OFF",
                 0, true);
}

void autodiscovery() {
    if (hass_autodiscovery_topic.length() == 0) {
        syslog.println("Home Assistant autodiscovery disabled.");
        return;
    }

    syslog.println("Sending Home Assistant autodiscovery messages...");

    const String board_unique_id = "calor-" + board_id;

    for (const auto & zone_ptr : zones) {
        const auto & zone = *zone_ptr;
        const auto unique_id = board_unique_id + "-" + zone.unique_id();

        const String topic_base = "calor/" + board_id + "/" + zone.unique_id();

        StaticJsonDocument<1024> json;

        json["unique_id"] = unique_id;
        json["name"] = "Calor " + String(zone.name);
        json["availability_topic"] = mqtt.will.topic;

        json["temperature_unit"] = "C";
        json["min_temp"] = 7;
        json["max_temp"] = 25;
        json["temp_step"] = 0.25;

        json["current_temperature_topic"] = topic_base + "/current_temperature";
        json["temperature_command_topic"] = topic_base + "/desired_temperature/set";
        json["temperature_state_topic"] = topic_base + "/desired_temperature";
        json["action_topic"] = topic_base + "/action";
        json["mode_state_topic"] = topic_base + "/mode";
        json["mode_command_topic"] = topic_base + "/mode/set";
        json["modes"][0] = "heat";
        json["modes"][1] = "off";
        json["retain"] = true;

        auto device = json["device"];
        device["name"] = json["name"];
        device["suggested_area"] = zone.name;
        device["identifiers"][0] = unique_id;
        device["via_device"] = board_unique_id;

        const String disco_topic = hass_autodiscovery_topic + "/climate/" + unique_id + "/config";
        auto publish = mqtt.begin_publish(disco_topic, measureJson(json), 0, true);
        serializeJson(json, publish);
        publish.send();
    }

    {
        const auto unique_id = board_unique_id + "-problem";
        StaticJsonDocument<1024> json;
        json["unique_id"] = unique_id;
        json["object_id"] = "calor_problem";
        json["name"] = "Calor problem";
        json["device_class"] = "problem";
        json["entity_category"] = "diagnostic";
        json["availability_topic"] = mqtt.will.topic;
        json["state_topic"] = "calor/" + board_id + "/problem";

        auto device = json["device"];
        device["name"] = "Calor";
        device["identifiers"][0] = board_unique_id;
        device["configuration_url"] = "http://" + WiFi.localIP().toString();
        device["manufacturer"] = "mlesniew";
        device["model"] = "Calor";
        device["sw_version"] = __DATE__ " " __TIME__;

        const String disco_topic = hass_autodiscovery_topic + "/binary_sensor/" + unique_id + "/config";
        auto publish = mqtt.begin_publish(disco_topic, measureJson(json), 0, true);
        serializeJson(json, publish);
        publish.send();
    }
}

void init() {
    mqtt.client_id = "calor-" + board_id;
    mqtt.will.topic = "calor/" + board_id + "/availability";
    mqtt.will.payload = "offline";
    mqtt.will.retain = true;

    mqtt.connected_callback = [] {
        // send autodiscovery messages
        autodiscovery();

        // notify about the current state
        for (auto & watch : watches) { watch->fire(); }

        // notify about availability
        mqtt.publish(mqtt.will.topic, "online", 0, true);
    };

    for (auto & zone_ptr : zones) {
        auto & zone = *zone_ptr;
        const String topic_base = "calor/" + board_id + "/" + zone.unique_id();
        mqtt.subscribe(topic_base + "/desired_temperature/set", [&zone](String payload) {
            const double value = payload.toDouble();
            if (value >= 7 && value <= 25) {
                zone.desired = value;
            }
        });

        mqtt.subscribe(topic_base + "/mode/set", [&zone](String payload) {
            if (payload == "heat") {
                zone.enabled = true;
            } else if (payload == "off") {
                zone.enabled = false;
            }
        });

        watches.push_back(
            new PicoUtils::Watch<double>(
                [&zone] { return zone.reading; },
                [&zone] { notify_current_temperature(zone); }));

        watches.push_back(
            new PicoUtils::Watch<double>(
                [&zone] { return zone.desired; },
                [&zone] { notify_desired_temperature(zone); }));

        watches.push_back(
            new PicoUtils::Watch<bool>(
                [&zone] { return zone.boiler_desired_state(); },
                [&zone] { notify_action(zone); }));

        watches.push_back(
            new PicoUtils::Watch<bool>(
                [&zone] { return zone.enabled; },
                [&zone] { notify_mode(zone); }));
    }

    watches.push_back(
        new PicoUtils::Watch<bool>(
            [] { return healthy; },
            notify_health
        ));
}


void tick() {
    mqtt.loop();
    for (auto & watch : watches) { watch->tick(); }
}

bool healthcheck() {
    return !mqtt.host.length() || !mqtt.port || mqtt.connected();
}

}