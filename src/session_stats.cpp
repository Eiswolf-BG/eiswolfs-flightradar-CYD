#include "session_stats.h"
#include <cstring>
#include <cstdio>
#include <time.h>
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

    // Ab hier: tagesbezogener Zustand (Alex' Auftrag) - eigener
    // Tageswechsel-Mechanismus, gleiches String-Datumsvergleich-Muster wie
    // DailySightings/FlightLogbook::updatePeakTraffic(). Bewusst GETRENNT
    // von seenHex/typeCounts oben (die laufen weiter seit dem Neustart) -
    // ein Tageswechsel setzt NUR die folgenden Felder zurueck.
    char currentDay[11] = {0};

    // KEIN eigenes "schon heute gezaehlt"-Dedup-Set hier - das "heute neu
    // gesehen"-Signal kommt stattdessen als newToday-Parameter von
    // record() (siehe session_stats.h-Kommentar), abgeleitet von
    // DailySightings::record().
    //
    // "Seltenster Typ heute"/"meistgesehene Airline heute" wurden bewusst
    // WIEDER ENTFERNT (Alex' Auftrag urspruenglich enthalten) - jede neue
    // Haeufigkeits-Tabelle dafuer (selbst stark verkleinert auf 20 bzw. 8
    // Eintraege) hat den verfuegbaren statischen RAM-Rest (dram0_0_seg)
    // gesprengt (DRAM-Ueberlauf, zuletzt noch 424 Bytes zu viel selbst MIT
    // den verkleinerten Tabellen) - der Rest-Spielraum nach den vorherigen
    // Features in diesem Batch war schlicht zu klein fuer zwei weitere
    // Tabellen. Alex wurde darueber informiert; die fuenf uebrigen neuen
    // Tagesstatistiken unten brauchen dagegen nur einzelne Skalarwerte
    // (kein Tabellen-Speicher) und passen problemlos.
    bool hasLongestTracked = false;
    char longestTrackedCallsign[9] = {0};
    uint32_t longestTrackedSec = 0;

    bool hasMaxClimb = false;
    char maxClimbCallsign[9] = {0};
    int16_t maxClimbFtMin = 0;
    bool hasMaxDescent = false;
    char maxDescentCallsign[9] = {0};
    int16_t maxDescentFtMin = 0;

    bool hasFirstSeenToday = false;
    char firstSeenTodayCallsign[9] = {0};
    uint32_t firstSeenTodayEpoch = 0;
    bool hasLastSeenToday = false;
    char lastSeenTodayCallsign[9] = {0};
    uint32_t lastSeenTodayEpoch = 0;

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

    // Liefert false (ohne die tagesbezogenen Felder zu aendern), solange
    // die Systemzeit noch nicht NTP-synchronisiert ist - gleiche Pruefung
    // wie DailySightings::updateCurrentDay()/FlightLogbook::checkAutoOff().
    bool updateCurrentDayStats() {
        time_t now = time(nullptr);
        if (now <= 8 * 3600 * 2) return false;

        struct tm tmNow;
        localtime_r(&now, &tmNow);
        char today[11];
        snprintf(today, sizeof(today), "%04d-%02d-%02d", tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);

        if (strcmp(today, currentDay) != 0) {
            strncpy(currentDay, today, sizeof(currentDay) - 1);
            currentDay[sizeof(currentDay) - 1] = 0;
            hasLongestTracked = false;
            longestTrackedCallsign[0] = 0;
            longestTrackedSec = 0;
            hasMaxClimb = false;
            hasMaxDescent = false;
            hasFirstSeenToday = false;
            hasLastSeenToday = false;
        }
        return true;
    }

}

void record(const Aircraft& a, bool newToday) {
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

    // Ab hier: tagesbezogene Werte (Alex' Auftrag) - nur, wenn die Uhrzeit
    // bereits NTP-synchronisiert ist (sonst liesse sich kein Tageswechsel
    // erkennen, siehe updateCurrentDayStats()).
    if (updateCurrentDayStats()) {
        const char* label = a.callsign[0] ? a.callsign : a.hex;

        if (newToday) {
            // Erstes/letztes NEU gesehenes Flugzeug heute - chronologisch
            // nach firstSeenEpoch (nur gueltig, wenn > 0, siehe aircraft.h).
            if (a.firstSeenEpoch > 0) {
                if (!hasFirstSeenToday || a.firstSeenEpoch < firstSeenTodayEpoch) {
                    hasFirstSeenToday = true;
                    firstSeenTodayEpoch = a.firstSeenEpoch;
                    strncpy(firstSeenTodayCallsign, label, sizeof(firstSeenTodayCallsign) - 1);
                    firstSeenTodayCallsign[sizeof(firstSeenTodayCallsign) - 1] = 0;
                }
                if (!hasLastSeenToday || a.firstSeenEpoch > lastSeenTodayEpoch) {
                    hasLastSeenToday = true;
                    lastSeenTodayEpoch = a.firstSeenEpoch;
                    strncpy(lastSeenTodayCallsign, label, sizeof(lastSeenTodayCallsign) - 1);
                    lastSeenTodayCallsign[sizeof(lastSeenTodayCallsign) - 1] = 0;
                }
            }
        }

        // Laengste durchgehend verfolgte Sichtung heute - JEDEN Zyklus
        // geprueft (nicht nur beim erstmaligen Sichten), da die Dauer mit
        // jedem weiteren Zyklus desselben Flugzeugs weiter waechst.
        if (a.firstSeenMs > 0 && millis() >= a.firstSeenMs) {
            uint32_t trackedSec = (millis() - a.firstSeenMs) / 1000;
            if (!hasLongestTracked || trackedSec > longestTrackedSec) {
                hasLongestTracked = true;
                longestTrackedSec = trackedSec;
                strncpy(longestTrackedCallsign, label, sizeof(longestTrackedCallsign) - 1);
                longestTrackedCallsign[sizeof(longestTrackedCallsign) - 1] = 0;
            }
        }

        // Hoechste Steig-/Sinkrate heute - zwei getrennte Rekorde (siehe
        // Kommentar in session_stats.h).
        if (a.vertRateFtMin > 0 && (!hasMaxClimb || a.vertRateFtMin > maxClimbFtMin)) {
            hasMaxClimb = true;
            maxClimbFtMin = a.vertRateFtMin;
            strncpy(maxClimbCallsign, label, sizeof(maxClimbCallsign) - 1);
            maxClimbCallsign[sizeof(maxClimbCallsign) - 1] = 0;
        }
        if (a.vertRateFtMin < 0 && (!hasMaxDescent || a.vertRateFtMin < maxDescentFtMin)) {
            hasMaxDescent = true;
            maxDescentFtMin = a.vertRateFtMin;
            strncpy(maxDescentCallsign, label, sizeof(maxDescentCallsign) - 1);
            maxDescentCallsign[sizeof(maxDescentCallsign) - 1] = 0;
        }
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

    s.hasLongestTracked = hasLongestTracked;
    strncpy(s.longestTrackedCallsign, longestTrackedCallsign, sizeof(s.longestTrackedCallsign) - 1);
    s.longestTrackedSec = longestTrackedSec;

    s.hasMaxClimb = hasMaxClimb;
    strncpy(s.maxClimbCallsign, maxClimbCallsign, sizeof(s.maxClimbCallsign) - 1);
    s.maxClimbFtMin = maxClimbFtMin;
    s.hasMaxDescent = hasMaxDescent;
    strncpy(s.maxDescentCallsign, maxDescentCallsign, sizeof(s.maxDescentCallsign) - 1);
    s.maxDescentFtMin = maxDescentFtMin;

    s.hasFirstSeenToday = hasFirstSeenToday;
    strncpy(s.firstSeenTodayCallsign, firstSeenTodayCallsign, sizeof(s.firstSeenTodayCallsign) - 1);
    s.firstSeenTodayEpoch = firstSeenTodayEpoch;
    s.hasLastSeenToday = hasLastSeenToday;
    strncpy(s.lastSeenTodayCallsign, lastSeenTodayCallsign, sizeof(s.lastSeenTodayCallsign) - 1);
    s.lastSeenTodayEpoch = lastSeenTodayEpoch;

    xSemaphoreGive(mutex);
    return s;
}

}
