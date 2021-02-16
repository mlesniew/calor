#ifndef WIFI_READINGS_H
#define WIFI_READINGS_H

#define TEMPERATURE_NAME_MAX_SIZE 16
#define TEMPERATURE_MAX_ENTRIES 10

class Stream;

struct TemperatureEntry {
    char name[TEMPERATURE_NAME_MAX_SIZE];
    float value;
};

class WiFiReadings {
    public:
        WiFiReadings(): presence(false), count(0) {}

        bool load(Stream & stream);
        bool load(const char * url);
        bool get(const char * name, float & value) const;

    protected:
        bool add(const char * name, float value);

        bool presence;
        unsigned int count;
        TemperatureEntry entries[TEMPERATURE_MAX_ENTRIES];
};

#endif
