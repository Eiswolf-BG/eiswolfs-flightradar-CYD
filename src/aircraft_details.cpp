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

    bool httpGetString(WiFiClientSecure& client, const String& url, String& outBody) {
        HTTPClient http;
        http.setTimeout(5000);
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

    // hexdb.io zuerst versuchen (bisherige Quelle) - aber mit kuerzerem
    // Timeout (3s statt 5s), damit ein kompletter Ausfall des Dienstes die
    // ADS-B-Abfrage nicht unnoetig lange blockiert, bevor der Fallback greift.
    client.setTimeout(3000);
    String body;
    if (httpGetString(client, String("https://hexdb.io/api/v1/aircraft/") + hex, body)) {
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
        client.setTimeout(4000);
        String body2;
        if (httpGetString(client, String("https://api.adsbdb.com/v0/aircraft/") + hex, body2)) {
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

    // Flugroute (Start-/Zielflughafen) - jetzt ueber eine Kette aus DREI
    // unabhaengigen kostenlosen Quellen statt nur einer, absteigend nach in
    // Tests beobachteter Trefferquote sortiert. Vorher lieferte adsbdb.com
    // allein in ca. 80% der Faelle "unknown" (siehe Alex' Feedback) - die
    // drei Quellen speisen sich aus unterschiedlichen, ueberlappenden aber
    // nicht identischen Community-Datenbanken, daher deutlich bessere
    // Gesamtabdeckung durch Verketten:
    //   1. VRS-Standing-Data-Mirror (adsb.lol) - stuendlich aktualisierter
    //      Spiegel des Virtual-Radar-Server-Projekts, in Tests die mit
    //      Abstand zuverlaessigste Quelle. Pfad = erste 2 Zeichen des
    //      (GROSSGESCHRIEBENEN - der Dienst ist case-sensitiv) Rufzeichens
    //      als Ordner, liefert "airport_codes":"ORIG-DEST" (ICAO).
    //   2. hexdb.io - eigener Route-Endpunkt (andere URL als der
    //      Aircraft-Endpunkt weiter oben), liefert "route":"ORIG-DEST".
    //   3. adsbdb.com Callsign-Endpunkt - bisherige einzige Quelle, bleibt
    //      als letzter Fallback, da sie gelegentlich Daten hat, die die
    //      anderen beiden nicht haben.
    // Nur versuchen, wenn ueberhaupt ein Rufzeichen bekannt ist -
    // Sichtflug-Maschinen ohne Callsign haben ohnehin keine darueber
    // auswertbare Route.
    String trimmedCallsign = String(callsign);
    trimmedCallsign.trim();
    trimmedCallsign.toUpperCase();

    auto applyRouteCodes = [&](const String& codes) {
        int dash = codes.indexOf('-');
        if (dash > 0 && dash < (int)codes.length() - 1) {
            strncpy(result.routeOrigin, codes.substring(0, dash).c_str(), sizeof(result.routeOrigin) - 1);
            strncpy(result.routeDest, codes.substring(dash + 1).c_str(), sizeof(result.routeDest) - 1);
        }
    };

    // Gleiches "ORIG-DEST"-Aufsplitten wie applyRouteCodes() oben, nur fuer
    // die IATA-Variante (siehe routeOriginIata/routeDestIata in
    // aircraft_details.h) - eigene Funktion statt Parameter an
    // applyRouteCodes(), da die Ziel-Puffer eine andere Groesse haben.
    auto applyRouteCodesIata = [&](const String& codes) {
        int dash = codes.indexOf('-');
        if (dash > 0 && dash < (int)codes.length() - 1) {
            strncpy(result.routeOriginIata, codes.substring(0, dash).c_str(), sizeof(result.routeOriginIata) - 1);
            strncpy(result.routeDestIata, codes.substring(dash + 1).c_str(), sizeof(result.routeDestIata) - 1);
        }
    };

    if (trimmedCallsign.length() >= 2) {
        // 1. VRS-Standing-Data-Mirror.
        client.setTimeout(4000);
        String folder = trimmedCallsign.substring(0, 2);
        String body3;
        if (httpGetString(client, String("https://vrs-standing-data.adsb.lol/routes/") + folder + "/" + trimmedCallsign + ".json", body3)) {
            JsonDocument doc3;
            if (!deserializeJson(doc3, body3)) {
                const char* codes = doc3["airport_codes"] | "";
                applyRouteCodes(String(codes));
                // Diese Quelle liefert die IATA-Variante gleich im selben
                // Aufruf mit ("_airport_codes_iata") - kein zusaetzlicher
                // API-Call noetig, siehe routeOriginIata/routeDestIata in
                // aircraft_details.h.
                const char* codesIata = doc3["_airport_codes_iata"] | "";
                applyRouteCodesIata(String(codesIata));
            }
        }

        // 2. hexdb.io Route-Endpunkt, falls Quelle 1 nichts geliefert hat.
        if (!result.routeOrigin[0] || !result.routeDest[0]) {
            client.setTimeout(3000);
            String body4;
            if (httpGetString(client, String("https://hexdb.io/api/v1/route/icao/") + trimmedCallsign, body4)) {
                JsonDocument doc4;
                if (!deserializeJson(doc4, body4)) {
                    const char* route = doc4["route"] | "";
                    applyRouteCodes(String(route));
                }
            }
        }
    }

    // 3. adsbdb.com Callsign-Endpunkt als letzter Fallback.
    if (trimmedCallsign.length() > 0 && (!result.routeOrigin[0] || !result.routeDest[0])) {
        client.setTimeout(4000);
        String body5;
        if (httpGetString(client, String("https://api.adsbdb.com/v0/callsign/") + trimmedCallsign, body5)) {
            JsonDocument doc5;
            if (!deserializeJson(doc5, body5)) {
                const char* originIcao = doc5["response"]["flightroute"]["origin"]["icao_code"] | "";
                const char* destIcao = doc5["response"]["flightroute"]["destination"]["icao_code"] | "";
                if (originIcao[0] && destIcao[0]) {
                    strncpy(result.routeOrigin, originIcao, sizeof(result.routeOrigin) - 1);
                    strncpy(result.routeDest, destIcao, sizeof(result.routeDest) - 1);
                    // Auch diese Quelle liefert IATA-Codes im selben JSON
                    // mit ("iata_code" statt "icao_code") - kein
                    // zusaetzlicher API-Call noetig.
                    const char* originIata = doc5["response"]["flightroute"]["origin"]["iata_code"] | "";
                    const char* destIata = doc5["response"]["flightroute"]["destination"]["iata_code"] | "";
                    if (originIata[0] && destIata[0]) {
                        strncpy(result.routeOriginIata, originIata, sizeof(result.routeOriginIata) - 1);
                        strncpy(result.routeDestIata, destIata, sizeof(result.routeDestIata) - 1);
                    }
                }
            }
        }
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    strncpy(cachedHex, hex, sizeof(cachedHex) - 1);
    cached = result;
    hasPending = false;
    xSemaphoreGive(mutex);
}

}
