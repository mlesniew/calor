#include <vector>
#include <set>
#include <string>

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WebServer.h>
#include <uri/UriRegex.h>

#include <ArduinoJson.h>
#include <PicoMQTT.h>
#include <PicoSyslog.h>
#include <PicoUtils.h>
#include <WiFiManager.h>

#include <valve.h>

#include "hass.h"
#include "zone.h"

PicoMQTT::Server & get_mqtt() {
    static PicoMQTT::Server mqtt;
    return mqtt;
}

PicoMQTT::Publisher & get_mqtt_publisher() {
    return get_mqtt();
}

PicoPrometheus::Registry & get_prometheus() {
    static PicoPrometheus::Registry prometheus;
    return prometheus;
}

PicoPrometheus::Gauge heating_demand(get_prometheus(), "heating_demand", "Burner heat demand state");

PicoSyslog::Logger syslog("calor");
PicoUtils::PinInput<D1, false> button;
PicoUtils::ResetButton reset_button(button);

PicoUtils::PinOutput<D5, true> heating_relay;
PicoUtils::PinOutput<D6, true> valve_relay;

PicoUtils::PinOutput<D4, true> wifi_led;
PicoUtils::WiFiControl<WiFiManager> wifi_control(wifi_led);

std::vector<Zone *> zones;
Valve * local_valve;

std::vector<PicoUtils::Tickable *> tickables;

String hass_autodiscovery_topic = "homeassistant";

PicoUtils::RestfulServer<ESP8266WebServer> server(80);

const char CONFIG_FILE[] PROGMEM = "/config.json";

Zone * find_zone_by_name(const String & name) {
    for (auto & zone_ptr : zones) {
        if (name == zone_ptr->name) {
            return zone_ptr;
        }
    }
    return nullptr;
}

DynamicJsonDocument get_config() {
    DynamicJsonDocument json(1024);

    auto zone_config = json["zones"].to<JsonObject>();
    for (const auto & zone : zones) {
        zone_config[zone->name] = zone->get_config();
    }

    json["valve"] = local_valve->get_config();

    {
        auto hass = json["hass"];
        hass["server"] = HomeAssistant::mqtt.host;
        hass["port"] = HomeAssistant::mqtt.port;
        hass["username"] = HomeAssistant::mqtt.username;
        hass["password"] = HomeAssistant::mqtt.password;
    }

    json["syslog"] = syslog.server;

    return json;
}

bool healthy = false;
PicoPrometheus::Gauge health_gauge(get_prometheus(), "health", "Board healthcheck", []{ return healthy ? 1 : 0; });

PicoUtils::PeriodicRun healthcheck(5, []{
    static PicoUtils::Stopwatch last_healthy;

    healthy = (WiFi.status() == WL_CONNECTED) && HomeAssistant::healthcheck();
    for (auto & zone : zones) {
        healthy = healthy && zone->healthcheck();
    }

    if (healthy)
        last_healthy.reset();

    if (last_healthy.elapsed() >= 15 * 60) {
        syslog.println(F("Healthcheck failing for too long.  Reset..."));
        ESP.reset();
    }
});

void setup_server() {

    server.on("/zones", HTTP_GET, [] {
        StaticJsonDocument<1024> json;

        for (const auto & zone : zones) {
            json[zone->name] = zone->get_status();
        }

        server.sendJson(json);
    });

    server.on("/config", HTTP_GET, [] {
        server.sendJson(get_config());
    });

    server.on(UriRegex("/zones/([^/]+)"), HTTP_GET, [] {
        const String name = server.decodedPathArg(0).c_str();

        Zone * zone = find_zone_by_name(name);

        if (!zone) {
            server.send(404);
        } else {
            server.sendJson(zone->get_status());
        }
    });

    get_prometheus().labels["module"] = "calor";

    get_prometheus().register_metrics_endpoint(server);

    server.begin();
}

void setup() {
    heating_relay.init();
    heating_relay.set(false);

    valve_relay.init();
    valve_relay.set(false);

    wifi_led.init();
    wifi_led.set(true);

    Serial.begin(115200);

    Serial.println(F("\n\n"
                     "  ___      _\n"
                     " / __|__ _| |___ _ _\n"
                     "| (__/ _` | / _ \\ '_|\n"
                     " \\___\\__,_|_\\___/_|\n"
                     "\n"
                     "Calor " __DATE__ " " __TIME__ "\n"
                     "\n\n"
                     "Press and hold button now to enter WiFi setup.\n"
                    ));

    delay(3000);
    reset_button.init();

    wifi_control.init(button, "calor");

    LittleFS.begin();

    {
        const auto config = PicoUtils::JsonConfigFile<StaticJsonDocument<1024>>(LittleFS, FPSTR(CONFIG_FILE));

        for (JsonPairConst kv : config["zones"].as<JsonObjectConst>()) {
            Zone * zone = new Zone(kv.key().c_str(), kv.value());
            zones.push_back(zone);
            tickables.push_back(zone);
        }

        local_valve = new Valve(valve_relay, config["valve"]);
        tickables.push_back(local_valve);

        {
            const auto hass = config["hass"];
            HomeAssistant::mqtt.host = hass["server"] | "";
            HomeAssistant::mqtt.port = hass["port"] | 1883;
            HomeAssistant::mqtt.username = hass["username"] | "";
            HomeAssistant::mqtt.password = hass["password"] | "";
        }

        syslog.server = config["syslog"] | "";
    }

    Zone * zone = find_zone_by_name(local_valve->name);
    if (zone) {
        // bind zone desired valve state to local valve
        auto * watch_1 = new PicoUtils::Watch<bool>(
            [zone] { return zone->valve_desired_state(); },
        [zone](bool demand) { local_valve->demand_open = demand; });

        watch_1->fire();
        tickables.push_back(watch_1);

        auto * watch_2 = new PicoUtils::Watch<ValveState>(
            [] { return local_valve->get_state(); },
        [zone](ValveState state) { zone->valve_state = state; });

        watch_2->fire();
        tickables.push_back(watch_2);

        tickables.push_back(new PicoUtils::PeriodicRun(10, [zone]{ zone->valve_state = local_valve->get_state(); }));
    } else {
        local_valve->demand_open = false;
    }

    tickables.push_back(new PicoUtils::Watch<bool>(
    [] {
        for (auto & zone : zones) {
            if (zone->boiler_desired_state()) {
                return true;
            }
        }
        return false;
    },
    [](bool demand) {
        syslog.printf("Turning boiler %s.\n", demand ? "on" : "off");
        heating_relay.set(demand);
        heating_demand.set(demand);
    }));

    tickables.push_back(&healthcheck);

    setup_server();
    get_mqtt().begin();
    HomeAssistant::init();
}

void loop() {
    wifi_control.tick();
    server.handleClient();
    get_mqtt().loop();

    for (auto & tickable : tickables) { tickable->tick(); }

    HomeAssistant::tick();
}


