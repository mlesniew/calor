#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include "celsius.h"

namespace {

std::map<std::string, double> get_celsius_readings(Stream & stream) {
    StaticJsonDocument<256> doc;

    DeserializationError error = deserializeJson(doc, stream);

    if (error) {
        Serial.print(F("deserializeJson failed: "));
        Serial.println(error.f_str());
        return {};
    }

    std::map<std::string, double> ret;

    for (JsonPair kv : doc.as<JsonObject>()) {
        const std::string key{kv.key().c_str()};
        const double value = kv.value().as<double>();
        ret[key] = value;
        Serial.printf("  %s = %.2f\n", key.c_str(), value);
    }

    return ret;
}

}

std::map<std::string, double> get_celsius_readings(const std::string & ip) {
    const std::string uri = "http://" + ip + "/temperature.json";

    printf("Checking %s...\n", uri.c_str());

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("WiFi not connected"));
        return {};
    }

    WiFiClient client;
    HTTPClient http;

    // this disables chunked transfer encoding
    http.useHTTP10(true);

    // increase timeout
    client.setTimeout(10000);

    if (!http.begin(client, uri.c_str())) {
        Serial.println(F("error connecting"));
        return {};
    }

    const int code = http.GET();
    printf("got HTTP code %i\n", code);

    if (!code) {
        return {};
    }

    std::map<std::string, double> ret;

    if (code >= 200 && code < 300) {
        ret = get_celsius_readings(client);
    }

    http.end();
    return ret;
}
