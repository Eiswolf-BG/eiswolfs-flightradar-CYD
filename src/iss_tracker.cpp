#include "iss_tracker.h"
#include "config.h"
#include "settings_store.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cstdlib>

namespace IssTracker {

namespace {
    Position currentPos;
    uint32_t lastFetchMs = 0;

    void fetchNow() {
        if (WiFi.status() != WL_CONNECTED) return;

        // Open-Notify liefert nur ueber HTTP (kein HTTPS) - kein
        // WiFiClientSecure/TLS-Handshake noetig, spart genau die
        // Speicher-/Verbindungslast, die bei den ADS-B-Abfragen
        // problematisch war (siehe CLAUDE.md "Bekannte Probleme").
        WiFiClient client;
        HTTPClient http;
        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, "http://api.open-notify.org/iss-now.json")) {
            return;
        }

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.printf("[ISS] Abfrage fehlgeschlagen: HTTP %d\n", code);
            http.end();
            return;
        }

        String body = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            Serial.printf("[ISS] Antwort nicht lesbar: %s\n", err.c_str());
            return;
        }

        const char* latStr = doc["iss_position"]["latitude"] | "";
        const char* lonStr = doc["iss_position"]["longitude"] | "";
        if (!latStr[0] || !lonStr[0]) {
            Serial.println("[ISS] Antwort ohne Positionsfelder.");
            return;
        }

        currentPos.lat = atof(latStr);
        currentPos.lon = atof(lonStr);
        currentPos.available = true;
        currentPos.fetchedAtMs = millis();
    }
}

void update() {
    // AUS per Default-Schalter (Menue > Flugoptionen > Anzeigefilter) -
    // dann weder periodische Abfrage noch zusaetzlicher Netzwerk-Traffic.
    // Siehe SettingsStore::issMarkerEnabled().
    if (!SettingsStore::issMarkerEnabled()) return;

    uint32_t now = millis();
    if (now - lastFetchMs < Config::ISS_FETCH_INTERVAL_MS) return;
    lastFetchMs = now;
    fetchNow();
}

Position current() { return currentPos; }

}
