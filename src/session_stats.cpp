#include "session_stats.h"
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace SessionStats {

namespace {
    // Gleiche Kappung wie FlightLogbook::seenHex (dort MAX_SEEN=400,
    // gleicher Zweck: eindeutige Hex-Adressen ueber viele Stunden zaehlen,
    // ohne unbegrenzt zu wachsen) - ueber 400 verschiedene Flugzeuge in
    // einer einzigen Sitzung sind selbst bei 100km Radius ueber viele
    // Stunden ein unrealistischer Extremfall, danach werden einfach keine
    // weiteren NEUEN Hex-Adressen mehr gezaehlt (bestehende Rekorde bleiben
    // unberuehrt).
    constexpr uint16_t MAX_SEEN = 400;
    char seenHex[MAX_SEEN][7];
    uint16_t seenCount = 0;

    // Haeufigster Typcode - kleine lineare Zaehltabelle, reicht fuer die
    // realistische Anzahl unterschiedlicher ICAO-Typcodes in einer Sitzung
    // locker aus (analog zur Kappung oben).
    constexpr uint8_t MAX_TYPES = 40;
    struct TypeCount { char code[5]; uint16_t count; };
    TypeCount typeCounts[MAX_TYPES];
    uint8_t typeCountsUsed = 0;

    bool hasClosest = false;
    float closestDistanceKm = 0;
    char closestCallsign[9] = {0};

    bool hasMaxSpeed = false;
    float maxSpeedKt = 0;

    bool hasMaxAlt = false;
    int32_t maxAltFt = 0;

    SemaphoreHandle_t mutex = nullptr;
    void ensureMutex() {
        if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    }

    // Liefert true, wenn "hex" NEU in dieser Sitzung ist (und merkt es sich
    // dann gleich) - false, wenn es schon bekannt war ODER die Kappung
    // erreicht ist.
    bool markSeenIfNew(const char* hex) {
        if (!hex || !hex[0]) return false;
        for (uint16_t i = 0; i < seenCount; i++) {
            if (strcmp(seenHex[i], hex) == 0) return false;
        }
        if (seenCount >= MAX_SEEN) return false;
        strncpy(seenHex[seenCount], hex, sizeof(seenHex[seenCount]) - 1);
        seenHex[seenCount][sizeof(seenHex[seenCount]) - 1] = 0;
        seenCount++;
        return true;
    }

    void bumpType(const char* typeCode) {
        if (!typeCode || !typeCode[0]) return;
        for (uint8_t i = 0; i < typeCountsUsed; i++) {
            if (strcmp(typeCounts[i].code, typeCode) == 0) {
                typeCounts[i].count++;
                return;
            }
        }
        if (typeCountsUsed >= MAX_TYPES) return;
        strncpy(typeCounts[typeCountsUsed].code, typeCode, sizeof(typeCounts[typeCountsUsed].code) - 1);
        typeCounts[typeCountsUsed].code[sizeof(typeCounts[typeCountsUsed].code) - 1] = 0;
        typeCounts[typeCountsUsed].count = 1;
        typeCountsUsed++;
    }
}

void record(const Aircraft& a) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);

    // Typ-Zaehlung nur beim erstmaligen Sichten dieses Flugzeugs in der
    // Sitzung (Alex' Vorgabe: eindeutige Flugzeuge zaehlen, nicht
    // Sichtungen/Zyklen) - Distanz/Geschwindigkeit/Hoehe darunter dagegen
    // bewusst bei JEDEM Zyklus geprueft, da sich ein Rekord auch bei einem
    // spaeteren Vorbeiflug desselben Flugzeugs noch verbessern kann.
    if (markSeenIfNew(a.hex)) {
        bumpType(a.typeCode);
    }

    if (!hasClosest || a.distanceKm < closestDistanceKm) {
        hasClosest = true;
        closestDistanceKm = a.distanceKm;
        const char* label = a.callsign[0] ? a.callsign : a.hex;
        strncpy(closestCallsign, label, sizeof(closestCallsign) - 1);
        closestCallsign[sizeof(closestCallsign) - 1] = 0;
    }
    if (!hasMaxSpeed || a.groundSpeedKt > maxSpeedKt) {
        hasMaxSpeed = true;
        maxSpeedKt = a.groundSpeedKt;
    }
    if (!hasMaxAlt || a.altBaroFt > maxAltFt) {
        hasMaxAlt = true;
        maxAltFt = a.altBaroFt;
    }

    xSemaphoreGive(mutex);
}

Snapshot get() {
    ensureMutex();
    Snapshot s;
    xSemaphoreTake(mutex, portMAX_DELAY);

    s.uniqueAircraftCount = seenCount;
    s.hasClosest = hasClosest;
    s.closestDistanceKm = closestDistanceKm;
    strncpy(s.closestCallsign, closestCallsign, sizeof(s.closestCallsign) - 1);
    s.hasMaxSpeed = hasMaxSpeed;
    s.maxSpeedKt = maxSpeedKt;
    s.hasMaxAlt = hasMaxAlt;
    s.maxAltFt = maxAltFt;

    uint16_t bestCount = 0;
    for (uint8_t i = 0; i < typeCountsUsed; i++) {
        if (typeCounts[i].count > bestCount) {
            bestCount = typeCounts[i].count;
            strncpy(s.topType, typeCounts[i].code, sizeof(s.topType) - 1);
            s.topType[sizeof(s.topType) - 1] = 0;
        }
    }
    s.hasTopType = bestCount > 0;
    s.topTypeCount = bestCount;

    xSemaphoreGive(mutex);
    return s;
}

}
