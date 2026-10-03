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
    // SessionStats/FlightLogbook). Auf 80 abgesenkt (Alex' Auftrag, Session-
    // Highlights/Pass-by-Zaehler) - der Session-Highlights-Batch brauchte
    // an anderer Stelle (session_stats.cpp, neue tagesbezogene Felder)
    // zusaetzliches statisches RAM, das knapp nicht mehr frei war (48 Byte
    // DRAM-Ueberlauf) - 80 statt 100 verschiedene Flugzeuge pro Tag bleibt
    // immer noch grosszuegig und schafft hier genug Spielraum, ohne eines
    // der neuen Features wieder streichen zu muessen.
    constexpr uint16_t MAX_SEEN = 80;

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

    // Pass-by-Zaehler (Alex' Auftrag) - EIN laufender "bester bisheriger
    // Wert" statt einer vollen Rangliste, um den Entry-Speicher nicht
    // weiter zu vergroessern (siehe Kommentar oben zu MAX_SEEN - die
    // Struktur hatte frueher schon einmal einen DRAM-Ueberlauf verursacht).
    char topReturningHex[7] = {0};
    uint16_t topReturningCount = 0;

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
            topReturningHex[0] = 0;
            topReturningCount = 0;
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

bool record(const Aircraft& a) {
    if (!a.hex[0]) return false;
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);

    bool isNewToday = false;
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
                isNewToday = true;
            }
        } else {
            uint32_t gapMs = now - e->lastSeenMs; // overflow-sicher (unsigned)
            if (gapMs > RETURN_GAP_MS) {
                e->count++;
                e->returnedAtMs = now;
                e->lastGapMs = gapMs;

                // Pass-by-Rekord (Alex' Auftrag) - nur bei einer ECHTEN
                // Rueckkehr aktualisiert (count==1 waere trivial "noch nie
                // zurueckgekehrt", keine sinnvolle Rangliste).
                if (e->count > topReturningCount) {
                    topReturningCount = e->count;
                    strncpy(topReturningHex, e->hex, sizeof(topReturningHex) - 1);
                    topReturningHex[sizeof(topReturningHex) - 1] = 0;
                }
            }
            e->lastSeenMs = now;
        }
    }

    xSemaphoreGive(mutex);
    return isNewToday;
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

TopReturning topReturning() {
    TopReturning result;
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (topReturningCount > 0) {
        result.available = true;
        result.count = topReturningCount;
        strncpy(result.hex, topReturningHex, sizeof(result.hex) - 1);
        result.hex[sizeof(result.hex) - 1] = 0;
    }
    xSemaphoreGive(mutex);
    return result;
}

}
