#include "type_watchlist.h"
#include "config.h"
#include "sd_mutex.h"
#include "sd_storage.h"
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>
#include <cctype>

namespace TypeWatchlist {

namespace {
    constexpr const char* WATCHED_FILE = "/Flightradar_cyd/watched_types.txt";

    // 4 Zeichen + Nullterminierung - passend zu Aircraft::typeCode
    // (aircraft.h, char[5]), gegen das hier verglichen wird.
    char watched[MAX_WATCHED][5] = {{0}};
    uint8_t watchedCount = 0;

    // Schuetzt watched[]/watchedCount - gleiches Muster wie
    // AircraftWatchlist::mutex (von Core 1/Menue UND Core 0/NetTask aus
    // erreichbar, siehe WatchlistAlert::isHit()).
    SemaphoreHandle_t mutex = nullptr;

    void ensureMutex() {
        if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    }

    // Ueberspringt fuehrende Leerzeichen, uebernimmt bis zu 4 Zeichen und
    // bricht bei einem Leerzeichen ab, alles in Grossbuchstaben - gleiches
    // Prinzip wie AircraftWatchlist::normalize(), nur kuerzer (Typ-Codes
    // statt Rufzeichen).
    void normalize(const char* typeCode, char* out) {
        int j = 0;
        int i = 0;
        while (typeCode[i] == ' ') i++;
        for (; j < 4 && typeCode[i] && typeCode[i] != ' '; i++, j++) {
            out[j] = (char)toupper((unsigned char)typeCode[i]);
        }
        out[j] = '\0';
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
            strncpy(watched[watchedCount], line.c_str(), 4);
            watched[watchedCount][4] = 0;
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

String typeAt(uint8_t index) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    String out = (index >= watchedCount) ? String() : String(watched[index]);
    xSemaphoreGive(mutex);
    return out;
}

bool addWatched(const char* typeCode) {
    if (!typeCode || !typeCode[0]) return false;

    char normalized[5] = {0};
    normalize(typeCode, normalized);
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
            strncpy(watched[watchedCount], normalized, 4);
            watched[watchedCount][4] = 0;
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
            strncpy(watched[i], watched[i + 1], 5);
        }
        watchedCount--;
        watched[watchedCount][0] = 0;
    }
    xSemaphoreGive(mutex);
    if (changed) saveToSd();
}

bool isWatched(const char* typeCode) {
    ensureMutex();
    char normalized[5];
    normalize(typeCode, normalized);
    if (!normalized[0]) return false;

    xSemaphoreTake(mutex, portMAX_DELAY);
    bool found = false;
    for (uint8_t i = 0; i < watchedCount; i++) {
        if (strcmp(watched[i], normalized) == 0) { found = true; break; }
    }
    xSemaphoreGive(mutex);
    return found;
}

}
