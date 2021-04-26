#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "wifi_readings.h"

#include <ArduinoJson.h>

bool WiFiReadings::update(Stream & stream) {
    StaticJsonDocument<256> doc;

    DeserializationError error = deserializeJson(doc, stream);

    if (error) {
        Serial.printf("deserializeJson() failed: %s", error.f_str());
        return false;
    }

    readings.presence = doc["presence"].as<bool>();
    Serial.printf("  presence = %i\n", readings.presence);

    for (JsonPair kv : doc["temperature"].as<JsonObject>()) {
        const char * key = kv.key().c_str();
        const double value = kv.value().as<double>();
        readings.temperature[key] = value;
        Serial.printf("  %s = %.2f\n", key, value);
    }

    last_update.reset();
    return true;
}

bool WiFiReadings::update() {
    if (WiFi.status() != WL_CONNECTED)
        return false;

    Serial.println(F("Updating WiFi readings..."));

    WiFiClient client;
    HTTPClient http;

    // this disables chunked transfer encoding
    http.useHTTP10(true);

    // increase timeout
    client.setTimeout(10000);

    if (!http.begin(client, url.c_str()))
        return false;

    const int code = http.GET();
    if (!code)
        return false;

    bool ret = false;
    if (code >= 200 && code < 300) {
        ret = update(client);
    }

    http.end();
    return ret;
}
