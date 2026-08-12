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

    // Flugroute (Start-/Zielflughafen) - derselbe adsbdb.com-Dienst wie oben,
    // aber ueber den Callsign-Endpunkt statt Hex-Code abgefragt (liefert
    // dafuer origin/destination-Flughafendaten zurueck). Nur versuchen, wenn
    // ueberhaupt ein Rufzeichen bekannt ist - Sichtflug-Maschinen ohne
    // Callsign haben ohnehin keine darueber auswertbare Route.
    String trimmedCallsign = String(callsign);
    trimmedCallsign.trim();
    if (trimmedCallsign.length() > 0) {
        client.setTimeout(4000);
        String body3;
        if (httpGetString(client, String("https://api.adsbdb.com/v0/callsign/") + trimmedCallsign, body3)) {
            JsonDocument doc3;
            DeserializationError err3 = deserializeJson(doc3, body3);
            if (!err3) {
                const char* originIcao = doc3["response"]["flightroute"]["origin"]["icao_code"] | "";
                const char* destIcao = doc3["response"]["flightroute"]["destination"]["icao_code"] | "";
                strncpy(result.routeOrigin, originIcao, sizeof(result.routeOrigin) - 1);
                strncpy(result.routeDest, destIcao, sizeof(result.routeDest) - 1);
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
