#include "location_manager.h"
#include "config.h"
#include "location_presets.h"
#include "settings_store.h"
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace LocationManager {

namespace {
    Preferences prefs;

    TinyGPSPlus gps;
    HardwareSerial gpsSerial(1);

    bool gpsEnabled = false;
    uint8_t gpsPinIndex = 0;
    bool gpsSerialStarted = false;

    double lastLat = 0, lastLon = 0;
    bool havePersisted = false;

    bool ipLookupDone = false;
    uint32_t lastIpLookupAttemptMs = 0;
    constexpr uint32_t IP_LOOKUP_RETRY_MS = 15000;

    Source source = Source::None;

    bool haveUtcOffset = false;
    int32_t utcOffsetSecs = 0;
    bool metricUnits = true;

    SemaphoreHandle_t mutex = nullptr;

    void startGpsSerialIfNeeded() {
        if (!gpsEnabled || gpsSerialStarted) return;
        const auto& pins = Config::GPS_PIN_CANDIDATES[gpsPinIndex];
        gpsSerial.begin(Config::GPS_BAUD, SERIAL_8N1, pins.rx, pins.tx);
        gpsSerialStarted = true;
    }

    void persistLocationAndSource(double lat, double lon, Source newSource) {
        prefs.putDouble("homeLat", lat);
        prefs.putDouble("homeLon", lon);
        xSemaphoreTake(mutex, portMAX_DELAY);
        lastLat = lat;
        lastLon = lon;
        havePersisted = true;
        source = newSource;
        xSemaphoreGive(mutex);
    }
}

void init() {
    if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    prefs.begin("adsb_radar", false);
    gpsEnabled = prefs.getBool("gpsEn", false);
    gpsPinIndex = prefs.getUChar("gpsPinIdx", 0);
    if (gpsPinIndex >= Config::GPS_PIN_CANDIDATE_COUNT) gpsPinIndex = 0;

    double lat = prefs.getDouble("homeLat", 0.0);
    double lon = prefs.getDouble("homeLon", 0.0);
    if (lat != 0.0 || lon != 0.0) {
        lastLat = lat;
        lastLon = lon;
        havePersisted = true;
        source = Source::Persisted;
    }

    startGpsSerialIfNeeded();
}

void update() {
    if (!gpsEnabled) return;
    startGpsSerialIfNeeded();

    while (gpsSerial.available() > 0) {
        gps.encode(gpsSerial.read());
    }

    if (gps.location.isValid() && gps.location.isUpdated()) {
        persistLocationAndSource(gps.location.lat(), gps.location.lng(), Source::GpsFix);
    }
}

void requestIpLookupIfNeeded() {
    if (ipLookupDone) return;
    if (gps.location.isValid()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    uint32_t now = millis();

    if (lastIpLookupAttemptMs != 0 && now - lastIpLookupAttemptMs < IP_LOOKUP_RETRY_MS) {
        return;
    }
    lastIpLookupAttemptMs = now;

    WiFiClient client;
    HTTPClient http;
    char url[96];
    snprintf(url, sizeof(url), "http://%s%s", Config::IP_GEO_HOST, Config::IP_GEO_PATH);

    if (!http.begin(client, url)) return;

    http.setTimeout(5000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) { http.end(); return; } 

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) return;

    const char* status = doc["status"] | "";
    if (strcmp(status, "success") != 0) return;

    double lat = doc["lat"] | 0.0;
    double lon = doc["lon"] | 0.0;
    if (lat == 0.0 && lon == 0.0) return;

    utcOffsetSecs = doc["offset"] | 0;
    haveUtcOffset = true;

    const char* countryCode = doc["countryCode"] | "";
    metricUnits = (strcmp(countryCode, "US") != 0);

    persistLocationAndSource(lat, lon, Source::IpGeolocation);
    ipLookupDone = true;
}

void getHomeLocation(double& lat, double& lon) {
    int8_t presetIdx = LocationPresets::activeIndex();
    if (presetIdx >= 0) {
        LocationPresets::getLatLon((uint8_t)presetIdx, lat, lon);
        return;
    }

    if (gps.location.isValid()) {
        lat = gps.location.lat();
        lon = gps.location.lng();
        return;
    }
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (havePersisted) {
        lat = lastLat;
        lon = lastLon;
    }
    xSemaphoreGive(mutex);
}

Source currentSource() {
    if (LocationPresets::activeIndex() >= 0) return Source::Manual;

    xSemaphoreTake(mutex, portMAX_DELAY);
    Source s = source;
    xSemaphoreGive(mutex);
    return s;
}

void setManualLocation(double lat, double lon) {
    persistLocationAndSource(lat, lon, Source::Manual);
}

void setGpsEnabled(bool enabled) {
    gpsEnabled = enabled;
    prefs.putBool("gpsEn", enabled);
    if (enabled) {
        gpsSerialStarted = false;
        startGpsSerialIfNeeded();
    }
}

bool isGpsEnabled() { return gpsEnabled; }

void cycleGpsPinPair() {
    gpsPinIndex = (gpsPinIndex + 1) % Config::GPS_PIN_CANDIDATE_COUNT;
    prefs.putUChar("gpsPinIdx", gpsPinIndex);
    gpsSerialStarted = false;
    if (gpsEnabled) startGpsSerialIfNeeded();
}

const char* currentGpsPinLabel() {
    return Config::GPS_PIN_CANDIDATES[gpsPinIndex].label;
}

bool hasGpsFix() { return gps.location.isValid(); }

bool currentGpsPosition(double& lat, double& lon) {
    if (!gps.location.isValid()) return false;
    lat = gps.location.lat();
    lon = gps.location.lng();
    return true;
}

bool hasGpsAltitude() { return gps.altitude.isValid(); }
double gpsAltitudeMeters() { return gps.altitude.meters(); }

bool hasUtcOffset() { return haveUtcOffset; }
int32_t utcOffsetSeconds() { return utcOffsetSecs; }
bool useMetricUnits() {
    uint8_t mode = SettingsStore::unitsMode();
    if (mode == 1) return true;
    if (mode == 2) return false;
    return metricUnits;
}

}