#include "airline_filter.h"
#include "config.h"
#include "sd_mutex.h"
#include "sd_storage.h"
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>
#include <cctype>

namespace AirlineFilter {

namespace {
    constexpr const char* HIDDEN_FILE = "/Flightradar_cyd/hidden_airlines.txt";

    char hidden[MAX_HIDDEN][4] = {{0}};
    uint8_t hiddenCount = 0;

    // Schuetzt hidden[]/hiddenCount - urspruenglich nur von Core 1 (Menue-
    // Screens) verwendet, seit der WebUI-Listenverwaltung (siehe
    // web_export_server.cpp) aber auch von Core 0 (NetTask) aus erreichbar.
    // Gleiches Muster wie AircraftDetails::mutex.
    SemaphoreHandle_t mutex = nullptr;

    void ensureMutex() {
        if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    }

    void extractPrefix(const char* callsign, char* out) {
        int i = 0;
        for (; i < 3 && callsign[i] && isalpha((unsigned char)callsign[i]); i++) {
            out[i] = (char)toupper((unsigned char)callsign[i]);
        }
        out[i] = '\0';
    }

    void saveToSd() {
        if (!SdStorage::isMounted()) return;
        SdMutex::Guard guard;

        File f = SD.open(HIDDEN_FILE, FILE_WRITE);
        if (!f) return;
        for (uint8_t i = 0; i < hiddenCount; i++) {
            f.println(hidden[i]);
        }
        f.close();
    }

    void loadFromSd() {
        hiddenCount = 0;
        if (!SdStorage::isMounted()) return;
        SdMutex::Guard guard;

        if (!SD.exists(HIDDEN_FILE)) return;
        File f = SD.open(HIDDEN_FILE, FILE_READ);
        if (!f) return;

        while (f.available() && hiddenCount < MAX_HIDDEN) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;
            strncpy(hidden[hiddenCount], line.c_str(), 3);
            hidden[hiddenCount][3] = 0;
            hiddenCount++;
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
    uint8_t c = hiddenCount;
    xSemaphoreGive(mutex);
    return c;
}

String icaoAt(uint8_t index) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    String out = (index >= hiddenCount) ? String() : String(hidden[index]);
    xSemaphoreGive(mutex);
    return out;
}

bool addHidden(const char* icaoPrefix) {
    if (!icaoPrefix || !icaoPrefix[0]) return false;

    char normalized[4] = {0};
    uint8_t i = 0;
    for (; i < 3 && icaoPrefix[i]; i++) {
        normalized[i] = (char)toupper((unsigned char)icaoPrefix[i]);
    }
    normalized[i] = 0;
    if (i == 0) return false;

    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool ok = true;
    bool alreadyPresent = false;
    if (hiddenCount >= MAX_HIDDEN) {
        ok = false;
    } else {
        for (uint8_t j = 0; j < hiddenCount; j++) {
            if (strcmp(hidden[j], normalized) == 0) { alreadyPresent = true; break; }
        }
        if (!alreadyPresent) {
            strncpy(hidden[hiddenCount], normalized, 3);
            hidden[hiddenCount][3] = 0;
            hiddenCount++;
        }
    }
    xSemaphoreGive(mutex);

    if (ok && !alreadyPresent) saveToSd();
    return ok;
}

void removeHidden(uint8_t index) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool changed = index < hiddenCount;
    if (changed) {
        for (uint8_t i = index; i < hiddenCount - 1; i++) {
            strncpy(hidden[i], hidden[i + 1], 4);
        }
        hiddenCount--;
        hidden[hiddenCount][0] = 0;
    }
    xSemaphoreGive(mutex);
    if (changed) saveToSd();
}

bool isHidden(const char* callsign) {
    ensureMutex();
    char prefix[4];
    extractPrefix(callsign, prefix);
    if (!prefix[0]) return false;

    xSemaphoreTake(mutex, portMAX_DELAY);
    bool found = false;
    for (uint8_t i = 0; i < hiddenCount; i++) {
        if (strcmp(hidden[i], prefix) == 0) { found = true; break; }
    }
    xSemaphoreGive(mutex);
    return found;
}

}
