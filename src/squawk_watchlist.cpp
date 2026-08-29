#include "squawk_watchlist.h"
#include "config.h"
#include "sd_mutex.h"
#include "sd_storage.h"
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>

namespace SquawkWatchlist {

namespace {
    constexpr const char* WATCHED_FILE = "/Flightradar_cyd/watched_squawks.txt";

    char watched[MAX_WATCHED][5] = {{0}};
    uint8_t watchedCount = 0;

    // Schuetzt watched[]/watchedCount - gleiches Muster/gleicher Grund wie
    // AircraftWatchlist::mutex (Zugriff sowohl von Core 1 (Menue-Screens,
    // Radar) als auch potenziell von Core 0).
    SemaphoreHandle_t mutex = nullptr;

    void ensureMutex() {
        if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    }

    // Gueltig sind genau 4 Ziffern, jede davon 0-7 (Squawk-Codes sind
    // oktal - Ziffern 8/9 kommen in echten ADS-B-Squawks nicht vor).
    bool isValidSquawk(const char* s) {
        if (!s) return false;
        for (uint8_t i = 0; i < 4; i++) {
            if (s[i] < '0' || s[i] > '7') return false;
        }
        return s[4] == '\0';
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
            if (!isValidSquawk(line.c_str())) continue;
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

String squawkAt(uint8_t index) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    String out = (index >= watchedCount) ? String() : String(watched[index]);
    xSemaphoreGive(mutex);
    return out;
}

bool addWatched(const char* squawk) {
    if (!isValidSquawk(squawk)) return false;

    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool ok = true;
    bool alreadyPresent = false;
    if (watchedCount >= MAX_WATCHED) {
        ok = false;
    } else {
        for (uint8_t j = 0; j < watchedCount; j++) {
            if (strcmp(watched[j], squawk) == 0) { alreadyPresent = true; break; }
        }
        if (!alreadyPresent) {
            strncpy(watched[watchedCount], squawk, 4);
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

bool isWatched(const char* squawk) {
    if (!squawk || !squawk[0]) return false;

    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool found = false;
    for (uint8_t i = 0; i < watchedCount; i++) {
        if (strcmp(watched[i], squawk) == 0) { found = true; break; }
    }
    xSemaphoreGive(mutex);
    return found;
}

}
