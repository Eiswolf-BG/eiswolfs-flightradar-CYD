#include "daily_sightings.h"
#include <cstring>
#include <cstdio>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace DailySightings {

namespace {
    // BEWUSST kleiner als SessionStats/FlightLogbook::seenHex (dort
    // MAX_SEEN=400, dort aber nur ein 7-Byte-Hex-Array ohne weitere
    // Felder) - jeder Eintrag hier ist mit drei zusaetzlichen
    // Zeitstempel-/Zaehlfeldern deutlich groesser (siehe Entry unten), ein
    // MAX_SEEN=400 hier haette beim ersten Build-Versuch das verfuegbare
    // statische RAM (dram0_0_seg) tatsaechlich gesprengt (Linker-Fehler,
    // ~1,9KB Ueberlauf) - die vom Build-Tool gemeldete RAM-Prozentzahl
    // bildet den dafuer tatsaechlich verfuegbaren Speicherbereich nicht 1:1
    // ab. 100 verschiedene Flugzeuge an einem einzigen Kalendertag ist
    // ausserhalb sehr belebter Flughafennaehe immer noch grosszuegig,
    // danach werden einfach keine neuen Hex-Adressen mehr aufgenommen
    // (bestehende Eintraege bleiben unberuehrt, gleiches Prinzip wie bei
    // SessionStats/FlightLogbook).
    constexpr uint16_t MAX_SEEN = 100;

    struct Entry {
        char hex[7];
        uint16_t count;
        uint32_t lastSeenMs;
        // 0 = die laufende Sichtungsserie ist die allererste des Tages
        // (noch keine Rueckkehr), sonst millis()-Zeitpunkt der letzten
        // Rueckkehr nach einer Pause.
        uint32_t returnedAtMs;
        uint32_t lastGapMs;
    };
    Entry entries[MAX_SEEN];
    uint16_t entryCount = 0;

    // Aktueller Kalendertag als "YYYY-MM-DD"-String (gleiches Muster wie
    // FlightLogbook::formatDateFromEpoch()/peakTrafficDate() - String-
    // Vergleich statt eigener Datums-Arithmetik, um einen Tageswechsel zu
    // erkennen).
    char currentDay[11] = {0};

    SemaphoreHandle_t mutex = nullptr;
    void ensureMutex() {
        if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    }

    // Wie lange ohne Sichtung vergangen sein muss, bevor ein erneutes
    // Auftauchen als echte Rueckkehr statt als normale Kurzluecke
    // (verpasste Fetch-Zyklen, kurzer Signalaussetzer) gilt - deutlich
    // ueber AircraftTable::STALE_TIMEOUT_MS (~24s, danach gilt ein
    // Flugzeug schon als "weg aus der Tabelle"), damit nicht jeder kurze
    // Aussetzer faelschlich als Rueckkehr gewertet wird.
    constexpr uint32_t RETURN_GAP_MS = 5UL * 60UL * 1000UL; // 5 Minuten

    // Wie lange nach einer erkannten Rueckkehr noch "RETURNED AFTER..."
    // statt des allgemeineren "SEEN Nx TODAY" gezeigt wird, falls das
    // Detail-Panel erst etwas spaeter geoeffnet wird - danach ist die
    // Rueckkehr selbst nicht mehr die interessanteste Information.
    constexpr uint32_t RETURN_DISPLAY_MS = 10UL * 60UL * 1000UL; // 10 Minuten

    // Liefert false (und laesst die bisherige Zaehlung unveraendert),
    // solange die Systemzeit noch nicht NTP-synchronisiert ist (gleiche
    // Pruefung wie an anderen Stellen im Projekt, z.B. FlightLogbook::
    // checkAutoOff()) - ohne verlaessliches Kalenderdatum liesse sich kein
    // sinnvoller Tageswechsel erkennen.
    bool updateCurrentDay() {
        time_t now = time(nullptr);
        if (now <= 8 * 3600 * 2) return false;

        struct tm tmNow;
        localtime_r(&now, &tmNow);
        char today[11];
        snprintf(today, sizeof(today), "%04d-%02d-%02d", tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);

        if (strcmp(today, currentDay) != 0) {
            strncpy(currentDay, today, sizeof(currentDay) - 1);
            currentDay[sizeof(currentDay) - 1] = 0;
            entryCount = 0;
        }
        return true;
    }

    Entry* find(const char* hex) {
        for (uint16_t i = 0; i < entryCount; i++) {
            if (strcmp(entries[i].hex, hex) == 0) return &entries[i];
        }
        return nullptr;
    }
}

void record(const Aircraft& a) {
    if (!a.hex[0]) return;
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);

    if (updateCurrentDay()) {
        uint32_t now = millis();
        Entry* e = find(a.hex);
        if (!e) {
            if (entryCount < MAX_SEEN) {
                e = &entries[entryCount++];
                strncpy(e->hex, a.hex, sizeof(e->hex) - 1);
                e->hex[sizeof(e->hex) - 1] = 0;
                e->count = 1;
                e->lastSeenMs = now;
                e->returnedAtMs = 0;
                e->lastGapMs = 0;
            }
        } else {
            uint32_t gapMs = now - e->lastSeenMs; // overflow-sicher (unsigned)
            if (gapMs > RETURN_GAP_MS) {
                e->count++;
                e->returnedAtMs = now;
                e->lastGapMs = gapMs;
            }
            e->lastSeenMs = now;
        }
    }

    xSemaphoreGive(mutex);
}

Info get(const char* hex) {
    Info info;
    if (!hex || !hex[0]) return info;
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);

    Entry* e = find(hex);
    if (e) {
        info.available = true;
        info.count = e->count;
        uint32_t nowMs = millis();
        if (e->count <= 1) {
            info.state = State::NewToday;
        } else if (e->returnedAtMs != 0 && (nowMs - e->returnedAtMs) < RETURN_DISPLAY_MS) {
            info.state = State::Returning;
            info.gapMs = e->lastGapMs;
        } else {
            info.state = State::SeenToday;
        }
    }

    xSemaphoreGive(mutex);
    return info;
}

}
