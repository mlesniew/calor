#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "wifi_readings.h"

#include <ArduinoJson.h>

bool WiFiReadings::add(const char * name, float value) {
    Serial.printf("WiFi temperature reading %s = %.2f\n", name, value);

    if (count >= TEMPERATURE_MAX_ENTRIES)
        return false;

    strncpy(entries[count].name, name, TEMPERATURE_NAME_MAX_SIZE);
    entries[count].value = value;
    ++count;

    return true;
}

bool WiFiReadings::load(Stream & stream) {
    StaticJsonDocument<256> doc;

    DeserializationError error = deserializeJson(doc, stream);

    if (error) {
        Serial.printf("deserializeJson() failed: %s", error.f_str());
        return false;
    }

    presence = doc["presence"].as<bool>();

    count = 0;
    for (JsonPair kv : doc["temperature"].as<JsonObject>()) {
        add(kv.key().c_str(), kv.value().as<float>());
    }

    return true;
}

bool WiFiReadings::load(const char * url) {
    if (WiFi.status() != WL_CONNECTED)
        return false;

    WiFiClient client;
    HTTPClient http;

    // this disables chunked transfer encoding
    http.useHTTP10(true);

    // increase timeout
    client.setTimeout(10000);

    if (!http.begin(client, url))
        return false;

    const int code = http.GET();
    if (!code)
        return false;

    bool ret = false;
    if (code >= 200 && code < 300) {
        ret = load(client);
    }

    http.end();
    return ret;
}

bool WiFiReadings::get(const char * name, float & value) const {
    for (unsigned int i = 0; i < count; ++i) {
        if (strncmp(name, entries[i].name, TEMPERATURE_NAME_MAX_SIZE) == 0) {
            value = entries[i].value;
            return true;
        }
    }
    return false;
}
