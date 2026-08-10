#include "weather.h"
#include "config.h"
#include "location_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

namespace Weather {

namespace {
    WiFiClientSecure client;
    bool clientConfigured = false;

    Condition currentCondition = Condition::Unknown;
    uint32_t lastFetchMs = 0;
    double lastLat = 0;
    double lastLon = 0;
    bool hasLastLocation = false;

    // Ordnet den WMO-Wettercode von Open-Meteo (Feld "weathercode", siehe
    // https://open-meteo.com/en/docs) einer der wenigen Icon-Kategorien zu,
    // die main.cpp zeichnen kann.
    Condition conditionFromWmoCode(int code) {
        if (code == 0) return Condition::Clear;
        if (code == 1 || code == 2) return Condition::PartlyCloudy;
        if (code == 3 || code == 45 || code == 48) return Condition::Cloudy;
        if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return Condition::Rain;
        if (code >= 71 && code <= 77) return Condition::Snow;
        if (code >= 95) return Condition::Thunderstorm;
        return Condition::Unknown;
    }

    void fetchNow(double lat, double lon) {
        if (WiFi.status() != WL_CONNECTED) return;

        if (!clientConfigured) {
            client.setInsecure();
            client.setTimeout(Config::HTTP_TIMEOUT_MS);
            clientConfigured = true;
        }

        HTTPClient http;
        char url[160];
        snprintf(url, sizeof(url),
                 "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true",
                 lat, lon);

        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return;

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            http.end();
            return;
        }

        // Body erst komplett als String einsammeln (getString() kuemmert
        // sich zuverlaessig um Chunked-Transfer-Encoding), statt direkt aus
        // http.getStream() zu parsen - Letzteres scheiterte bei Open-Meteo
        // zuverlaessig mit einem ArduinoJson-"InvalidInput"-Fehler.
        String body = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) return;

        int wmoCode = doc["current_weather"]["weathercode"] | -1;
        if (wmoCode < 0) return;

        currentCondition = conditionFromWmoCode(wmoCode);
    }
}

void update() {
    double lat = 0, lon = 0;
    LocationManager::getHomeLocation(lat, lon);
    if (lat == 0 && lon == 0) return;

    // Deutliche Standort-Aenderung (z.B. anderes Standort-Preset aktiviert)
    // - sofort neu abfragen statt bis zum naechsten regulaeren Intervall zu
    // warten, damit das Icon nicht minutenlang das Wetter des alten
    // Standorts zeigt.
    bool locationChanged = !hasLastLocation ||
                            fabs(lat - lastLat) > 0.01 || fabs(lon - lastLon) > 0.01;

    uint32_t now = millis();
    if (!locationChanged && (now - lastFetchMs < Config::WEATHER_FETCH_INTERVAL_MS)) {
        return;
    }

    lastFetchMs = now;
    lastLat = lat;
    lastLon = lon;
    hasLastLocation = true;
    fetchNow(lat, lon);
}

Condition current() { return currentCondition; }

}
