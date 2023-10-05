#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include "celsius.h"

namespace {

std::map<std::string, double> parse_celsius_response(Stream & stream) {
    StaticJsonDocument<256> doc;

    DeserializationError error = deserializeJson(doc, stream);

    if (error) {
        Serial.print(F("deserializeJson failed: "));
        Serial.println(error.f_str());
        return {};
    }

    std::map<std::string, double> ret;

    for (JsonPair kv : doc.as<JsonObject>()) {
        ret[kv.key().c_str()] = kv.value().as<double>();
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
    http.setTimeout(10000);

    if (!http.begin(client, uri.c_str())) {
        Serial.println(F("error connecting"));
        return {};
    }

    const int code = http.GET();
    printf("got HTTP code %i\n", code);
    if (!code || (code < 200) || (code >= 300)) {
        return {};
    }

    return parse_celsius_response(http.getStream());
    // NOTE: there's no need to call http.end() if we don't plan to reuse the object
}
