#include <vector>
#include <set>
#include <string>

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <uri/UriRegex.h>

#include <ArduinoJson.h>
#include <PicoMQ.h>
#include <PicoMQTT.h>
#include <PicoPrometheus.h>
#include <PicoSlugify.h>
#include <PicoSyslog.h>
#include <PicoUtils.h>

#include "schalter.h"
#include "hass.h"
#include "zone.h"

PicoPrometheus::Registry prometheus;

PicoSyslog::Logger syslog("calor");
PicoUtils::PinInput button(D1);
PicoUtils::ResetButton reset_button(button);

PicoUtils::PinOutput heating_relay(D5, true);

PicoUtils::PinOutput wifi_led(D4, true);
PicoUtils::WiFiControlSmartConfig wifi_control(wifi_led);

std::vector<Zone *> zones;

std::vector<PicoUtils::Tickable *> tickables;

String hass_autodiscovery_topic = "homeassistant";
String hostname = "calor";

PicoUtils::RestfulServer<ESP8266WebServer> server(80);

PicoMQ picomq;

class MQTTServer : public PicoMQTT::Server {
    public:
        const PicoUtils::Stopwatch & get_last_message_stopwatch() const { return last_message; }

    protected:
        PicoUtils::Stopwatch last_message;

        void on_message(const char * topic, PicoMQTT::IncomingPacket & packet) {
            last_message.reset();
            PicoMQTT::Server::on_message(topic, packet);
        }
};

MQTTServer mqtt;

PicoPrometheus::Gauge heating_demand(prometheus, "heating_demand", "Burner heat demand state",
                                     [] { return heating_relay.get() ? 1 : 0; });

const char CONFIG_FILE[] PROGMEM = "/config.json";

Zone * find_zone_by_name(const String & name) {
    for (auto & zone_ptr : zones) {
        if (name == zone_ptr->name) {
            return zone_ptr;
        }
    }
    return nullptr;
}

JsonDocument get_config() {
    JsonDocument json;

    auto zone_config = json["zones"].to<JsonObject>();
    for (const auto & zone : zones) {
        zone_config[zone->name] = zone->get_config();
    }

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
PicoPrometheus::Gauge health_gauge(prometheus, "health", "Board healthcheck", [] { return healthy ? 1 : 0; });

PicoUtils::PeriodicRun healthcheck(5, [] {
    static PicoUtils::Stopwatch last_healthy;

    healthy = ((WiFi.status() == WL_CONNECTED) && HomeAssistant::healthcheck()) || (millis() <= 30 * 1000);

    for (auto & zone : zones) {
        healthy = healthy && zone->healthcheck();
    }

    if (healthy)
        last_healthy.reset();

    if ((last_healthy.elapsed() >= 12 * 60 * 60)
            || (mqtt.get_last_message_stopwatch().elapsed() >= 30 * 60)) {
        syslog.println(F("Healthcheck failing for too long.  Reset..."));
        ESP.reset();
    }
});

void setup_server() {

    server.on("/zones", HTTP_GET, [] {
        JsonDocument json;

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

    prometheus.labels["module"] = "calor";

    prometheus.register_metrics_endpoint(server);

    server.begin();
}

void setup() {
    heating_relay.init();
    heating_relay.set(false);

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

    LittleFS.begin();

    {
        const auto config = PicoUtils::JsonConfigFile<JsonDocument>(LittleFS, FPSTR(CONFIG_FILE));

        for (JsonPairConst kv : config["zones"].as<JsonObjectConst>()) {
            Zone * zone = new Zone(kv.key().c_str(), kv.value());
            zones.push_back(zone);
            tickables.push_back(zone);
        }

        {
            const auto hass = config["hass"];
            HomeAssistant::mqtt.host = hass["server"] | "";
            HomeAssistant::mqtt.port = hass["port"] | 1883;
            HomeAssistant::mqtt.username = hass["username"] | "";
            HomeAssistant::mqtt.password = hass["password"] | "";
        }

        syslog.server = config["syslog"] | "";
        hostname = PicoSlugify::slugify(config["hostname"] | "calor");
    }

    wifi_control.init(button);

    wifi_control.get_connectivity_level = [] {
        return 1 + (HomeAssistant::connected() ? 1 : 0) + (healthy ? 1 : 0);
    };

    tickables.push_back(new PicoUtils::Watch<bool>(
    [] {
        for (auto & zone : zones) {
            if (zone->heat()) {
                return true;
            }
        }
        return false;
    },
    [](bool demand) {
        syslog.printf("Turning boiler %s.\n", demand ? "on" : "off");
        heating_relay.set(demand);
    }));

    tickables.push_back(&healthcheck);
    tickables.push_back(&wifi_control);

    setup_server();
    picomq.begin();
    mqtt.begin();
    HomeAssistant::init();

    ArduinoOTA.setHostname(hostname.c_str());
    ArduinoOTA.begin();
}

void loop() {
    ArduinoOTA.handle();
    server.handleClient();
    picomq.loop();
    mqtt.loop();
    for (auto & tickable : tickables) { tickable->tick(); }
    HomeAssistant::tick();
}
