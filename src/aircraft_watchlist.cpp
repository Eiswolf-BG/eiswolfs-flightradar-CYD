#include "aircraft_watchlist.h"
#include "config.h"
#include "sd_mutex.h"
#include "sd_storage.h"
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>
#include <cctype>

namespace AircraftWatchlist {

namespace {
    constexpr const char* WATCHED_FILE = "/Flightradar_cyd/watched_aircraft.txt";

    char watched[MAX_WATCHED][9] = {{0}};
    uint8_t watchedCount = 0;

    // Schuetzt watched[]/watchedCount - urspruenglich nur von Core 1 (Menue-
    // Screens, Radar) verwendet, seit der WebUI-Listenverwaltung (siehe
    // web_export_server.cpp) aber auch von Core 0 (NetTask) aus erreichbar.
    // Gleiches Muster wie AircraftDetails::mutex.
    SemaphoreHandle_t mutex = nullptr;

    void ensureMutex() {
        if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    }

    // Ueberspringt fuehrende Leerzeichen, uebernimmt bis zu 8 Zeichen und
    // bricht bei einem Leerzeichen ab (ADS-B-Rufzeichen haben oft Padding),
    // alles in Grossbuchstaben.
    void normalize(const char* callsign, char* out) {
        int j = 0;
        int i = 0;
        while (callsign[i] == ' ') i++;
        for (; j < 8 && callsign[i] && callsign[i] != ' '; i++, j++) {
            out[j] = (char)toupper((unsigned char)callsign[i]);
        }
        out[j] = '\0';
    }

    // Einfaches Glob-Pattern-Matching, NUR "*" als Platzhalter fuer
    // beliebig viele Zeichen (Alex' Auftrag - kein echtes RegEx, bewusst
    // simpel gehalten). Klassischer iterativer Zwei-Zeiger-Algorithmus mit
    // Backtracking-Merkpunkt beim letzten "*": faellt text[] zu frueh aus,
    // wird einfach ein Zeichen mehr unter dem letzten "*" "verschluckt" und
    // von dort neu versucht. Ein pattern OHNE "*" verhaelt sich exakt wie
    // ein normaler strcmp()-Vergleich (bisheriges Verhalten bleibt dadurch
    // automatisch abwaertskompatibel, ohne Sonderfall-Code).
    bool globMatch(const char* pattern, const char* text) {
        const char* p = pattern;
        const char* t = text;
        const char* starP = nullptr;
        const char* starT = nullptr;
        while (*t) {
            if (*p == *t) { p++; t++; }
            else if (*p == '*') { starP = p++; starT = t; }
            else if (starP) { p = starP + 1; t = ++starT; }
            else return false;
        }
        while (*p == '*') p++;
        return *p == '\0';
    }

    void saveToSd() {
        if (!SdStorage::isMounted()) return;
        SdMutex::Guard guard;

        File f = SD.open(WATCHED_FILE, FILE_WRITE);
        if (!f) return;
        for (uint8_t i = 0; i < watchedCount; i++) {
            f.println(watched[i]);
        }
        f.close();
    }

    void loadFromSd() {
        watchedCount = 0;
        if (!SdStorage::isMounted()) return;
        SdMutex::Guard guard;

        if (!SD.exists(WATCHED_FILE)) return;
        File f = SD.open(WATCHED_FILE, FILE_READ);
        if (!f) return;

        while (f.available() && watchedCount < MAX_WATCHED) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;
            strncpy(watched[watchedCount], line.c_str(), 8);
            watched[watchedCount][8] = 0;
            watchedCount++;
        }
        f.close();
    }
}

void init() {
    ensureMutex();
    loadFromSd();
}

uint8_t count() {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    uint8_t c = watchedCount;
    xSemaphoreGive(mutex);
    return c;
}

String callsignAt(uint8_t index) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    String out = (index >= watchedCount) ? String() : String(watched[index]);
    xSemaphoreGive(mutex);
    return out;
}

bool addWatched(const char* callsign) {
    if (!callsign || !callsign[0]) return false;

    char normalized[9] = {0};
    normalize(callsign, normalized);
    if (!normalized[0]) return false;

    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool ok = true;
    bool alreadyPresent = false;
    if (watchedCount >= MAX_WATCHED) {
        ok = false;
    } else {
        for (uint8_t j = 0; j < watchedCount; j++) {
            if (strcmp(watched[j], normalized) == 0) { alreadyPresent = true; break; }
        }
        if (!alreadyPresent) {
            strncpy(watched[watchedCount], normalized, 8);
            watched[watchedCount][8] = 0;
            watchedCount++;
        }
    }
    xSemaphoreGive(mutex);

    if (ok && !alreadyPresent) saveToSd();
    return ok;
}

void removeWatched(uint8_t index) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool changed = index < watchedCount;
    if (changed) {
        for (uint8_t i = index; i < watchedCount - 1; i++) {
            strncpy(watched[i], watched[i + 1], 9);
        }
        watchedCount--;
        watched[watchedCount][0] = 0;
    }
    xSemaphoreGive(mutex);
    if (changed) saveToSd();
}

bool isWatched(const char* callsign) {
    ensureMutex();
    char normalized[9];
    normalize(callsign, normalized);
    if (!normalized[0]) return false;

    xSemaphoreTake(mutex, portMAX_DELAY);
    bool found = false;
    for (uint8_t i = 0; i < watchedCount; i++) {
        // globMatch() deckt sowohl Wildcard- ALS AUCH exakte Eintraege ab
        // (ein Pattern ohne "*" verhaelt sich darin identisch zu strcmp(),
        // siehe Kommentar dort) - kein separater Exact-Match-Zweig noetig.
        if (globMatch(watched[i], normalized)) { found = true; break; }
    }
    xSemaphoreGive(mutex);
    return found;
}

}
