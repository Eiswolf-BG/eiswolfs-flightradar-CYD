#include "location_manager.h"
#include "config.h"
#include "location_presets.h"
#include "settings_store.h"
#include <TinyGPSPlus.h>
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

    bool gpsEnabled = false;

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

    double lat = prefs.getDouble("homeLat", 0.0);
    double lon = prefs.getDouble("homeLon", 0.0);
    if (lat != 0.0 || lon != 0.0) {
        lastLat = lat;
        lastLon = lon;
        havePersisted = true;
        source = Source::Persisted;
    }
}

// GPIO1 (Config::GPS_RX_PIN) ist derselbe Pin wie die USB-Serial-Konsole
// (UART0, "Serial") - es gibt auf diesem Board keinen zweiten, unabhaengigen
// UART auf den tatsaechlich verkabelten Pins (siehe Config::GPS_RX_PIN-
// Kommentar in config.h). Deshalb wird "Serial" hier periodisch, fuer ein
// kurzes festes Zeitfenster, auf GPS-Baudrate umgeschaltet: Konsole aus,
// GPS-Bytes einsammeln, Konsole wieder auf 115200 herstellen. Waehrend
// dieses kurzen Fensters gehen ggf. einzelne Serial.print()-Aufrufe aus
// anderen Teilen der App ins Leere (HardwareSerial gibt in diesem Zustand
// einfach folgenlos zurueck, kein Absturzrisiko) - ein bewusst akzeptierter
// Kompromiss, da es keine Alternative auf den tatsaechlich verkabelten Pins
// gibt. Alle paar Sekunden statt bei jedem update()-Aufruf (~alle 50ms aus
// net_task.cpp), damit die Konsole ganz ueberwiegend normal nutzbar bleibt.
constexpr uint32_t GPS_READ_INTERVAL_MS = 3000;
constexpr uint32_t GPS_READ_WINDOW_MS = 500;

void update() {
    if (!gpsEnabled) return;

    static uint32_t lastReadMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastReadMs < GPS_READ_INTERVAL_MS) return;
    lastReadMs = nowMs;

    Serial.flush();
    Serial.end();
    delay(20);
    Serial.begin(Config::GPS_BAUD, SERIAL_8N1, Config::GPS_RX_PIN, Config::GPS_TX_PIN);

    uint32_t windowStartMs = millis();
    while (millis() - windowStartMs < GPS_READ_WINDOW_MS) {
        while (Serial.available() > 0) {
            gps.encode((char)Serial.read());
        }
    }

    Serial.end();
    delay(20);
    Serial.begin(115200);

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
}

bool isGpsEnabled() { return gpsEnabled; }

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