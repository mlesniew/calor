#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include "valvola.h"

namespace {

String serialize_map(const std::map<std::string, bool> mapping) {
    StaticJsonDocument<256> doc;

    for (auto & kv : mapping) {
        doc[kv.first] = kv.second;
    }

    String ret;
    serializeJson(doc, ret);
    return ret;
}

std::map<std::string, bool> parse_valvola_response(Stream & stream) {
    StaticJsonDocument<256> doc;

    DeserializationError error = deserializeJson(doc, stream);

    if (error) {
        Serial.print(F("deserializeJson failed: "));
        Serial.println(error.f_str());
        return {};
    }

    std::map<std::string, bool> ret;

    for (JsonPair kv : doc.as<JsonObject>()) {
        ret[kv.key().c_str()] = kv.value().as<bool>();
    }

    return ret;
}

}

std::map<std::string, bool> update_valvola(
    const std::string & address,
    const std::map<std::string, bool> desired) {

    const std::string uri = "http://" + address + "/valves";

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

    const int code = http.PUT(serialize_map(desired));
    printf("got HTTP code %i\n", code);
    if (!code || (code < 200) || (code >= 300)) {
        return {};
    }

    return parse_valvola_response(client);

    // NOTE: there's no need to call http.end() if we don't plan to reuse the object
}
