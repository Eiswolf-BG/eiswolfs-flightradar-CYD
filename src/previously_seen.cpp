#include "previously_seen.h"
#include "flight_logbook.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>

namespace PreviouslySeen {

namespace {
    SemaphoreHandle_t mutex = nullptr;

    char pendingHex[7] = {0};
    bool hasPending = false;

    char cachedHex[7] = {0};
    Info cached;

    void ensureMutex() {
        if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    }
}

void request(const char* hex) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (strcmp(cachedHex, hex) != 0 && strcmp(pendingHex, hex) != 0) {
        strncpy(pendingHex, hex, sizeof(pendingHex) - 1);
        pendingHex[sizeof(pendingHex) - 1] = 0;
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
    bool doWork = false;

    xSemaphoreTake(mutex, portMAX_DELAY);
    if (hasPending) {
        strncpy(hex, pendingHex, sizeof(hex) - 1);
        doWork = true;
    }
    xSemaphoreGive(mutex);

    if (!doWork) return;

    // Der eigentliche (potenziell langsame) SD-Kartenscan - siehe
    // FlightLogbook::countPreviousSightings() fuer Details/den bewusst in
    // Kauf genommenen Zeitaufwand.
    FlightLogbook::PreviousSighting sighting = FlightLogbook::countPreviousSightings(hex);

    Info result;
    result.found = sighting.found;
    result.count = sighting.count;
    strncpy(result.lastDate, sighting.lastDate, sizeof(result.lastDate) - 1);
    result.hasPattern = sighting.hasPattern;
    result.minHour = sighting.minHour;
    result.maxHour = sighting.maxHour;
    result.minAltitudeFt = sighting.minAltitudeFt;
    result.maxAltitudeFt = sighting.maxAltitudeFt;
    result.hasProfile = sighting.hasProfile;
    result.minDistanceKm = sighting.minDistanceKm;
    result.maxSpeedKt = sighting.maxSpeedKt;

    xSemaphoreTake(mutex, portMAX_DELAY);
    // Nur uebernehmen, wenn pendingHex sich waehrend des (unter Umstaenden
    // recht langen) Scans NICHT bereits auf ein ANDERES Flugzeug geaendert
    // hat (Alex' Vorgabe: ein schneller Wechsel zu einem anderen Flugzeug
    // waehrend ein Scan noch laeuft darf kein falsches Ergebnis am
    // falschen Panel zeigen). Ohne diesen Check wuerde "hasPending = false"
    // unten faelschlich einen zwischenzeitlich fuer ein ANDERES Flugzeug
    // neu gesetzten pendingHex als "erledigt" markieren, ohne dass fuer
    // dieses je tatsaechlich gescannt wurde - das Panel bliebe dann
    // faelschlich dauerhaft bei "wird geprueft..." haengen, statt (wie vom
    // naechsten update()-Aufruf eigentlich vorgesehen) tatsaechlich noch
    // gescannt zu werden.
    if (strcmp(pendingHex, hex) == 0) {
        strncpy(cachedHex, hex, sizeof(cachedHex) - 1);
        cachedHex[sizeof(cachedHex) - 1] = 0;
        cached = result;
        hasPending = false;
    }
    xSemaphoreGive(mutex);
}

}
