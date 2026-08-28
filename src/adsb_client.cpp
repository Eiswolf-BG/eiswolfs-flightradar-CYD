#include "adsb_client.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

namespace AdsbClient {

namespace {
    const char* kNoValidate = nullptr;

    WiFiClientSecure persistentClient;
    bool clientConfigured = false;
}

void primeTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    uint32_t start = millis();
    while (now < 8 * 3600 * 2 && millis() - start < 5000) {
        delay(100);
        now = time(nullptr);
    }
}

FetchResult fetch(double homeLat, double homeLon, float radiusKm,
                   Aircraft* table, uint8_t tableCapacity) {
    FetchResult result;

    if (WiFi.status() != WL_CONNECTED) {
        return result;
    }

    if (!clientConfigured) {
        persistentClient.setInsecure();
        persistentClient.setTimeout(Config::ADSB_HTTP_TIMEOUT_MS);
        clientConfigured = true;
    }

    HTTPClient http;
    char url[160];
    snprintf(url, sizeof(url),
             "https://%s/v2/lat/%.5f/lon/%.5f/dist/%.0f",
             Config::ADSB_API_HOST, homeLat, homeLon, radiusKm);

    http.setTimeout(Config::ADSB_HTTP_TIMEOUT_MS);
    if (!http.begin(persistentClient, url)) {
        return result;
    }
    http.setReuse(true);
    http.setUserAgent("EiswolfsFlightradarCYD/1.0 (+https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD)");
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    result.httpCode = code;

    if (code != HTTP_CODE_OK) {
        http.end();
        return result;
    }
    JsonDocument filter;
    JsonObject filterAc = filter["ac"].add<JsonObject>();
    filterAc["hex"]      = true;
    filterAc["flight"]   = true;
    filterAc["r"]        = true;
    filterAc["t"]        = true;
    filterAc["lat"]      = true;
    filterAc["lon"]      = true;
    filterAc["alt_baro"] = true;
    filterAc["baro_rate"]= true;
    filterAc["gs"]       = true;
    filterAc["track"]    = true;
    filterAc["squawk"]   = true;
    filterAc["category"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, http.getStream(), DeserializationOption::Filter(filter));

    http.end();

    if (err) {
        result.ok = false;
        return result;
    }

    JsonArray acArray = doc["ac"].as<JsonArray>();
    uint8_t idx = 0;
    for (JsonObject ac : acArray) {
        if (idx >= tableCapacity) break;

        const char* hex = ac["hex"] | "";

        // Manche ADS-B-Aggregatoren (adsb.fi eingeschlossen) melden ein und
        // dasselbe Flugzeug in seltenen Faellen zweimal in derselben Antwort
        // (z.B. ueber unterschiedliche Empfangs-/MLAT-Pfade) - das zeigte
        // sich als Alex' Bugmeldung: nach einer Reichweitenaenderung kurz
        // zwei Punkte fuer dasselbe Flugzeug auf dem Radar, die sich erst
        // nach 1-2 Abfragen von selbst korrigierten (sobald die Quelle
        // ihrerseits wieder nur einen Eintrag lieferte). Hier bereits beim
        // Einlesen ausschliessen statt erst beim Zeichnen zu bemerken - ein
        // bereits uebernommener Hex-Code wird ignoriert, der erste Eintrag
        // (idx 0..idx-1) bleibt massgeblich.
        bool duplicate = false;
        if (hex[0] != '\0') {
            for (uint8_t j = 0; j < idx; j++) {
                if (strcmp(table[j].hex, hex) == 0) {
                    duplicate = true;
                    break;
                }
            }
        }
        if (duplicate) continue;

        Aircraft& a = table[idx];
        a = Aircraft{};

        strncpy(a.hex, hex, sizeof(a.hex) - 1);

        const char* flight = ac["flight"] | "";
        strncpy(a.callsign, flight, sizeof(a.callsign) - 1);

        const char* reg = ac["r"] | "";
        strncpy(a.reg, reg, sizeof(a.reg) - 1);

        const char* type = ac["t"] | "";
        strncpy(a.typeCode, type, sizeof(a.typeCode) - 1);

        a.lat = ac["lat"] | 0.0f;
        a.lon = ac["lon"] | 0.0f;

        if (ac["alt_baro"].is<const char*>()) {
            a.altBaroFt = 0;
        } else {
            a.altBaroFt = ac["alt_baro"] | 0;
        }

        a.vertRateFtMin = ac["baro_rate"] | 0;
        a.groundSpeedKt = ac["gs"] | 0.0f;
        a.headingDeg    = ac["track"] | 0.0f;

        const char* squawk = ac["squawk"] | "";
        strncpy(a.squawk, squawk, sizeof(a.squawk) - 1);

        const char* category = ac["category"] | "";
        strncpy(a.category, category, sizeof(a.category) - 1);

        a.lastSeenMs = millis();
        a.valid = (a.lat != 0.0f || a.lon != 0.0f);

        if (a.valid) idx++;
    }

    result.ok = true;
    result.aircraftCount = idx;
    return result;
}

}