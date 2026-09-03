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
    // TESTWEISE - Backoff-Logik in net_task.cpp respektiert einen vom
    // Server mitgeschickten "Retry-After"-Header bei HTTP 429, statt nur
    // selbst zu schaetzen (siehe Absprache mit Karl). Muss VOR GET()
    // registriert werden, sonst liefert http.header() dafuer nichts.
    const char* collectedHeaders[] = {"Retry-After"};
    http.collectHeaders(collectedHeaders, 1);

    int code = http.GET();
    result.httpCode = code;

    if (code == 429) {
        String retryAfter = http.header("Retry-After");
        if (retryAfter.length()) {
            result.retryAfterSec = retryAfter.toInt();
        }
    }

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

    // Lokales, pro Aufruf freigegebenes JsonDocument (siehe CLAUDE.md,
    // Abschnitt "Bekannte Probleme" - eine dauerhaft wiederverwendete
    // Variante wurde ausprobiert und wieder zurueckgerollt, da sie ~45KB
    // Heap permanent blockierte und dadurch TLS-Handshakes zum Scheitern
    // brachte).
    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, http.getStream(), DeserializationOption::Filter(filter));

    http.end();

    if (err) {
        result.ok = false;
        return result;
    }

    // Schnappschuss von prevAirportDistKm (Best-Effort-Anflug-Erkennung,
    // siehe aircraft.h/aircraft_table.cpp::postFetchUpdate()) je Hex-Code,
    // BEVOR die Schleife unten beginnt, einzelne table[]-Eintraege per
    // "a = Aircraft{}" zurueckzusetzen. table[] ist tempTable aus
    // net_task.cpp, bleibt zwischen Aufrufen bestehen und enthaelt zu
    // Beginn dieses Aufrufs noch die Werte vom VORHERIGEN Abfragezyklus -
    // ohne diesen Schnappschuss wuerde "a = Aircraft{}" weiter unten
    // prevAirportDistKm bei JEDEM Zyklus auf den Default (-1) zuruecksetzen,
    // NOCH BEVOR aircraft_table.cpp::postFetchUpdate() (aufgerufen direkt
    // nach diesem fetch()) ueberhaupt pruefen kann, ob die Distanz zum
    // naechstgelegenen Flughafen sinkt - die Anflug-Erkennung wuerde dadurch
    // nie ausloesen (in einer Simulation mit stetig sinkender Distanz
    // bestaetigt: approachLikely blieb ueber alle Zyklen 0). Der Schnapp-
    // schuss wird bewusst VOR der Haupt-Schleife komplett erstellt statt
    // erst waehrend ihres Durchlaufs nachgeschlagen, weil sonst bereits in
    // diesem Zyklus ueberschriebene Slots fuer noch nicht bearbeitete
    // Hex-Codes falsche (schon geloeschte) Werte liefern wuerden.
    struct PrevAirportDist { char hex[7]; float dist; };
    PrevAirportDist prevAirportDistByHex[Config::MAX_TRACKED_AIRCRAFT];
    uint8_t prevAirportDistCount = 0;
    for (uint8_t j = 0; j < tableCapacity && j < Config::MAX_TRACKED_AIRCRAFT; j++) {
        if (table[j].hex[0] != '\0') {
            strncpy(prevAirportDistByHex[prevAirportDistCount].hex, table[j].hex,
                    sizeof(prevAirportDistByHex[0].hex) - 1);
            prevAirportDistByHex[prevAirportDistCount].hex[sizeof(prevAirportDistByHex[0].hex) - 1] = 0;
            prevAirportDistByHex[prevAirportDistCount].dist = table[j].prevAirportDistKm;
            prevAirportDistCount++;
        }
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

        // prevAirportDistKm aus dem oben erstellten Schnappschuss
        // wiederherstellen, falls dieses Flugzeug schon im vorherigen Zyklus
        // bekannt war (siehe Kommentar dort) - alle anderen Felder bleiben
        // bewusst beim Aircraft{}-Default, nur dieser eine Tracking-Wert
        // muss ueber den Reset hinweg erhalten bleiben.
        for (uint8_t j = 0; j < prevAirportDistCount; j++) {
            if (strcmp(prevAirportDistByHex[j].hex, hex) == 0) {
                a.prevAirportDistKm = prevAirportDistByHex[j].dist;
                break;
            }
        }

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