#include "hass.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PicoHA.h>
#include <PicoSyslog.h>
#include <PicoUtils.h>

#include <vector>

#include "zone.h"

extern PicoSyslog::Logger syslog;
extern std::vector<Zone *> zones;
extern bool healthy;
extern PicoUtils::PinOutput heating_relay;
extern String hostname;

namespace HomeAssistant {

PicoMQTT::Client mqtt;

PicoHA::Device device(mqtt, "Calor", "mlesniew", "Calor");
PicoHA::QueuedEvent reboot_event(device, "reboot", "Reboot");

PicoHA::BinarySensor problem_sensor(device, "problem", "Problem");
PicoHA::BinarySensor boiler_sensor(device, "boiler", "Boiler");

void init() {
    device.name = hostname;

    boiler_sensor.device_class = "power";
    boiler_sensor.icon = "fire";
    boiler_sensor.getter = [] { return heating_relay.get(); };

    problem_sensor.device_class = "problem";
    problem_sensor.icon = "alert";
    problem_sensor.getter = [] { return !healthy; };

    PicoHA::add_diagnostic_entities(device);

    for (auto zone : zones) {
        PicoHA::ChildDevice * zone_device =
            new PicoHA::ChildDevice(device, zone->name, "Calor " + zone->name,
                                    "mlesniew", "Calor Zone", zone->name);

        PicoHA::Climate * climate =
            new PicoHA::Climate(*zone_device, "climate", "");

        climate->min_temp = 7;
        climate->max_temp = 25;
        climate->temp_step = 0.25;
        climate->temperature_unit = PicoHA::Climate::TemperatureUnit::celsius;
        climate->modes = {PicoHA::Climate::Mode::heat,
                          PicoHA::Climate::Mode::off};

        climate->mode_getter = [zone] {
            return zone->enabled ? PicoHA::Climate::Mode::heat
                                 : PicoHA::Climate::Mode::off;
        };
        climate->mode_setter = [zone](PicoHA::Climate::Mode mode) {
            zone->enabled = (mode == PicoHA::Climate::Mode::heat);
        };

        climate->action_getter = [zone] {
            if (!zone->enabled) {
                return PicoHA::Climate::Action::off;
            }
            if (zone->get_state() == Zone::State::heat) {
                return PicoHA::Climate::Action::heating;
            } else {
                return PicoHA::Climate::Action::idle;
            }
        };

        climate->bind_power(&(zone->enabled));
        climate->bind_target_temperature(&(zone->desired));
        climate->current_temperature_getter = [zone] {
            return zone->get_reading();
        };

        PicoHA::Switch * boost =
            new PicoHA::Switch(*zone_device, "boost", "Boost");

        boost->getter = [zone] { return zone->boost_active(); };
        boost->setter = [zone](bool value) {
            if (value) {
                zone->boost();
            } else {
                zone->boost(0);
            }
        };
    }

    mqtt.begin();
    device.begin();
}

void tick() {
    mqtt.loop();
    device.tick();
}

bool connected() { return mqtt.connected(); }

bool healthcheck() {
    return !mqtt.host.length() || !mqtt.port || mqtt.connected();
}

}  // namespace HomeAssistant
