#include "aircraft_details.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>

namespace AircraftDetails {

namespace {
    SemaphoreHandle_t mutex = nullptr;

    char pendingHex[7] = {0};
    char pendingCallsign[9] = {0};
    bool hasPending = false;

    char cachedHex[7] = {0};
    Info cached;

    void ensureMutex() {
        if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    }

    // FIX: nimmt den Timeout jetzt als Parameter entgegen und setzt NUR
    // diesen (via http.setTimeout(), was intern client->setTimeout()
    // aufruft - siehe HTTPClient.cpp) - vorher wurde hier IMMER
    // http.setTimeout(5000) hart gesetzt, was den vom Aufrufer kurz zuvor
    // per client.setTimeout(N) gesetzten Wert unbemerkt ueberschrieben hat.
    // Dadurch liefen faktisch ALLE fuenf API-Aufrufe einheitlich mit 5s,
    // unabhaengig vom im jeweiligen Aufrufer dokumentierten Wert (3s/4s) -
    // der beabsichtigte kuerzere Timeout fuer hexdb.io griff also nie
    // (siehe Analyse im Chat-Verlauf).
    bool httpGetString(WiFiClientSecure& client, const String& url, String& outBody, uint32_t timeoutMs) {
        HTTPClient http;
        http.setTimeout(timeoutMs);
        if (!http.begin(client, url)) return false;
        int code = http.GET();
        bool ok = (code == HTTP_CODE_OK);
        if (ok) outBody = http.getString();
        http.end();
        return ok;
    }
}

void request(const char* hex, const char* callsign) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (strcmp(cachedHex, hex) != 0 && strcmp(pendingHex, hex) != 0) {
        strncpy(pendingHex, hex, sizeof(pendingHex) - 1);
        strncpy(pendingCallsign, callsign ? callsign : "", sizeof(pendingCallsign) - 1);
        hasPending = true;
    }
    xSemaphoreGive(mutex);
}

Info get(const char* hex) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    Info out;
    if (strcmp(cachedHex, hex) == 0) {
        out = cached;
    } else if (strcmp(pendingHex, hex) == 0 && hasPending) {
        out.loading = true;
    }
    xSemaphoreGive(mutex);
    return out;
}

bool fetchRoute(WiFiClientSecure& client, const String& callsign,
                 char* origin, size_t originSize, char* dest, size_t destSize,
                 char* originIata, size_t originIataSize, char* destIata, size_t destIataSize) {
    origin[0] = 0;
    dest[0] = 0;
    if (originIata && originIataSize) originIata[0] = 0;
    if (destIata && destIataSize) destIata[0] = 0;

    String trimmedCallsign = callsign;
    trimmedCallsign.trim();
    trimmedCallsign.toUpperCase();
    if (trimmedCallsign.length() == 0) return false;

    constexpr uint32_t HEXDB_TIMEOUT_MS = 1200;
    constexpr uint32_t OTHER_TIMEOUT_MS = 4000;

    auto applyRouteCodes = [&](const String& codes) {
        int dash = codes.indexOf('-');
        if (dash > 0 && dash < (int)codes.length() - 1) {
            strncpy(origin, codes.substring(0, dash).c_str(), originSize - 1);
            origin[originSize - 1] = 0;
            strncpy(dest, codes.substring(dash + 1).c_str(), destSize - 1);
            dest[destSize - 1] = 0;
        }
    };
    auto applyRouteCodesIata = [&](const String& codes) {
        if (!originIata || !destIata) return;
        int dash = codes.indexOf('-');
        if (dash > 0 && dash < (int)codes.length() - 1) {
            strncpy(originIata, codes.substring(0, dash).c_str(), originIataSize - 1);
            originIata[originIataSize - 1] = 0;
            strncpy(destIata, codes.substring(dash + 1).c_str(), destIataSize - 1);
            destIata[destIataSize - 1] = 0;
        }
    };

    // Quellen-Reihenfolge/Timeouts (Messung vom 30.08., siehe CLAUDE.md
    // "Bekannte Probleme" fuer die Herleitung):
    //   1. VRS-Standing-Data-Mirror (adsb.lol) - schnellste/zuverlaessigste
    //      Quelle, liefert ICAO UND IATA im selben JSON.
    if (trimmedCallsign.length() >= 2) {
        String folder = trimmedCallsign.substring(0, 2);
        String body;
        if (httpGetString(client, String("https://vrs-standing-data.adsb.lol/routes/") + folder + "/" + trimmedCallsign + ".json", body, OTHER_TIMEOUT_MS)) {
            JsonDocument doc;
            if (!deserializeJson(doc, body)) {
                applyRouteCodes(String((const char*)(doc["airport_codes"] | "")));
                applyRouteCodesIata(String((const char*)(doc["_airport_codes_iata"] | "")));
            }
        }
    }

    //   2. adsbdb.com Callsign-Endpunkt - ebenfalls ICAO UND IATA im selben
    //      JSON, falls Quelle 1 nichts geliefert hat.
    if (!origin[0] || !dest[0]) {
        String body;
        if (httpGetString(client, String("https://api.adsbdb.com/v0/callsign/") + trimmedCallsign, body, OTHER_TIMEOUT_MS)) {
            JsonDocument doc;
            if (!deserializeJson(doc, body)) {
                const char* originIcao = doc["response"]["flightroute"]["origin"]["icao_code"] | "";
                const char* destIcao = doc["response"]["flightroute"]["destination"]["icao_code"] | "";
                if (originIcao[0] && destIcao[0]) {
                    strncpy(origin, originIcao, originSize - 1);
                    origin[originSize - 1] = 0;
                    strncpy(dest, destIcao, destSize - 1);
                    dest[destSize - 1] = 0;
                    if (originIata && destIata) {
                        const char* oIata = doc["response"]["flightroute"]["origin"]["iata_code"] | "";
                        const char* dIata = doc["response"]["flightroute"]["destination"]["iata_code"] | "";
                        if (oIata[0] && dIata[0]) {
                            strncpy(originIata, oIata, originIataSize - 1);
                            originIata[originIataSize - 1] = 0;
                            strncpy(destIata, dIata, destIataSize - 1);
                            destIata[destIataSize - 1] = 0;
                        }
                    }
                }
            }
        }
    }

    //   3. hexdb.io Route-Endpunkt - letzter Fallback, kurzer Timeout
    //      (liefert NIE IATA-Codes, nur ICAO).
    if (!origin[0] || !dest[0]) {
        String body;
        if (httpGetString(client, String("https://hexdb.io/api/v1/route/icao/") + trimmedCallsign, body, HEXDB_TIMEOUT_MS)) {
            JsonDocument doc;
            if (!deserializeJson(doc, body)) {
                applyRouteCodes(String((const char*)(doc["route"] | "")));
            }
        }
    }

    return origin[0] != 0 && dest[0] != 0;
}

void update() {
    ensureMutex();

    char hex[7] = {0};
    char callsign[9] = {0};
    bool doWork = false;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (hasPending) {
        strncpy(hex, pendingHex, sizeof(hex) - 1);
        strncpy(callsign, pendingCallsign, sizeof(callsign) - 1);
        doWork = true;
    }
    xSemaphoreGive(mutex);

    if (!doWork) return;

    Info result;

    WiFiClientSecure client;
    client.setInsecure();

    // hexdb.io zuerst versuchen (bisherige Quelle) - mit kurzem, JETZT
    // TATSAECHLICH wirksamem Timeout (1200ms, siehe httpGetString()-Fix
    // oben): ein Verbindungstest hat gezeigt, dass hexdb.io TCP/TLS zwar
    // sofort annimmt, aber teils gar keine HTTP-Antwort mehr liefert -
    // 1200ms reichen fuer eine funktionierende Antwort (in Tests <250ms)
    // locker, begrenzen einen Ausfall aber auf ein Minimum, bevor der
    // Fallback greift.
    constexpr uint32_t HEXDB_TIMEOUT_MS = 1200;
    constexpr uint32_t OTHER_TIMEOUT_MS = 4000;
    String body;
    if (httpGetString(client, String("https://hexdb.io/api/v1/aircraft/") + hex, body, HEXDB_TIMEOUT_MS)) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (!err) {
            const char* manufacturer = doc["Manufacturer"] | "";
            const char* type = doc["Type"] | "";
            if (manufacturer[0] && type[0]) {
                snprintf(result.model, sizeof(result.model), "%s %s", manufacturer, type);
            } else if (type[0]) {
                strncpy(result.model, type, sizeof(result.model) - 1);
            }
        }
    }

    // Fallback: hexdb.io war nicht erreichbar/lieferte kein Modell -
    // adsbdb.com als zweite, unabhaengige Quelle versuchen (andere API-Form,
    // aber inhaltlich aequivalent: Hersteller + Typ ueber den Hex-Code).
    if (!result.model[0]) {
        String body2;
        if (httpGetString(client, String("https://api.adsbdb.com/v0/aircraft/") + hex, body2, OTHER_TIMEOUT_MS)) {
            JsonDocument doc2;
            DeserializationError err2 = deserializeJson(doc2, body2);
            if (!err2) {
                const char* manufacturer = doc2["response"]["aircraft"]["manufacturer"] | "";
                const char* type = doc2["response"]["aircraft"]["type"] | "";
                if (manufacturer[0] && type[0]) {
                    snprintf(result.model, sizeof(result.model), "%s %s", manufacturer, type);
                } else if (type[0]) {
                    strncpy(result.model, type, sizeof(result.model) - 1);
                }
            }
        }
    }

    // Flugroute (Start-/Zielflughafen) - ueber die gemeinsame fetchRoute()-
    // Fallback-Kette oben (VRS-Standing-Data-Mirror -> adsbdb.com ->
    // hexdb.io, siehe dortige Kommentare zur Quellen-Reihenfolge/Messung
    // vom 30.08.), inklusive IATA-Codes (werden von zwei der drei Quellen
    // im selben JSON mitgeliefert, siehe routeOriginIata/routeDestIata in
    // aircraft_details.h) - kein zweiter, eigener Aufruf-Code mehr noetig
    // (frueher hier dupliziert, jetzt mit route_watchlist.cpp geteilt).
    fetchRoute(client, callsign, result.routeOrigin, sizeof(result.routeOrigin),
               result.routeDest, sizeof(result.routeDest),
               result.routeOriginIata, sizeof(result.routeOriginIata),
               result.routeDestIata, sizeof(result.routeDestIata));

    xSemaphoreTake(mutex, portMAX_DELAY);
    strncpy(cachedHex, hex, sizeof(cachedHex) - 1);
    cached = result;
    hasPending = false;
    xSemaphoreGive(mutex);
}

}
