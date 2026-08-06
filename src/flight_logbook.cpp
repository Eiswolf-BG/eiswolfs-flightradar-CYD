#include "flight_logbook.h"
#include "config.h"
#include "aircraft.h"
#include "aircraft_table.h"
#include "settings_store.h"
#include "sd_mutex.h"
#include <SD.h>
#include <time.h>
#include <cstring>

namespace FlightLogbook {

namespace {
    constexpr uint16_t MAX_SEEN = 400;
    char seenHex[MAX_SEEN][7];
    uint16_t seenCount = 0;
    char currentDateStr[11] = {0};

    bool computeDateStr(char* out, size_t outSize) {
        time_t now = time(nullptr);
        if (now < 8 * 3600 * 2) return false;
        struct tm tmNow;
        localtime_r(&now, &tmNow);
        snprintf(out, outSize, "%04d-%02d-%02d", tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);
        return true;
    }

    void logFilename(char* out, size_t outSize) {
        snprintf(out, outSize, "%s/%s.csv", Config::SD_LOG_DIR, currentDateStr);
    }

    bool alreadySeen(const char* hex) {
        for (uint16_t i = 0; i < seenCount; i++) {
            if (strcmp(seenHex[i], hex) == 0) return true;
        }
        return false;
    }

    void markSeen(const char* hex) {
        if (seenCount >= MAX_SEEN) return;
        strncpy(seenHex[seenCount], hex, sizeof(seenHex[seenCount]) - 1);
        seenHex[seenCount][sizeof(seenHex[seenCount]) - 1] = 0;
        seenCount++;
    }

    uint32_t countLinesFast(File& f) {
        constexpr size_t BUF_SIZE = 1024;
        static uint8_t buf[BUF_SIZE];
        uint32_t lines = 0;
        uint32_t blocksRead = 0;
        while (f.available()) {
            size_t n = f.read(buf, BUF_SIZE);
            for (size_t i = 0; i < n; i++) {
                if (buf[i] == '\n') lines++;
            }
            blocksRead++;
            if (blocksRead % 8 == 0) yield();
        }
        return lines;
    }

    void loadSeenFromTodayFile() {
        seenCount = 0;
        char filename[64];
        logFilename(filename, sizeof(filename));
        if (!SD.exists(filename)) return;

        File f = SD.open(filename, FILE_READ);
        if (!f) return;

        constexpr size_t BUF_SIZE = 1024;
        static uint8_t buf[BUF_SIZE];
        char lineBuf[48];
        size_t lineLen = 0;
        bool firstLine = true;
        uint32_t blocksRead = 0;

        auto processLine = [&]() {
            if (lineLen == 0) return;
            if (firstLine) { firstLine = false; return; }
            lineBuf[lineLen] = 0;
            char* firstComma = strchr(lineBuf, ',');
            if (!firstComma) return;
            char* secondComma = strchr(firstComma + 1, ',');
            size_t hexLen = secondComma ? (size_t)(secondComma - (firstComma + 1))
                                         : strlen(firstComma + 1);
            if (hexLen > 0 && hexLen < sizeof(seenHex[0])) {
                char hexBuf[7] = {0};
                memcpy(hexBuf, firstComma + 1, hexLen);
                markSeen(hexBuf);
            }
        };

        while (f.available() && seenCount < MAX_SEEN) {
            size_t n = f.read(buf, BUF_SIZE);
            for (size_t i = 0; i < n && seenCount < MAX_SEEN; i++) {
                char c = (char)buf[i];
                if (c == '\n' || c == '\r') {
                    if (lineLen > 0) processLine();
                    lineLen = 0;
                } else if (lineLen < sizeof(lineBuf) - 1) {
                    lineBuf[lineLen++] = c;
                }
            }
            blocksRead++;
            if (blocksRead % 8 == 0) yield();
        }
        f.close();
    }

    void ensureCurrentDate() {
        char today[11];
        if (!computeDateStr(today, sizeof(today))) return;

        if (strcmp(today, currentDateStr) != 0) {
            strncpy(currentDateStr, today, sizeof(currentDateStr) - 1);
            loadSeenFromTodayFile();
        }
    }

    void writeLogLine(File& f, const Aircraft& a) {
        time_t now = time(nullptr);
        struct tm tmNow;
        localtime_r(&now, &tmNow);
        char timestamp[20];
        snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
                 tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
                 tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);

        f.printf("%s,%s,%s,%s,%s,%.1f,%d\n",
                 timestamp,
                 a.hex,
                 a.callsign[0] ? a.callsign : "",
                 a.reg[0] ? a.reg : "",
                 a.typeCode[0] ? a.typeCode : "",
                 a.distanceKm,
                 (int)a.altBaroFt);
    }
}

void init() {
    SdMutex::Guard guard;
    ensureCurrentDate();
}

void update() {
    if (!SettingsStore::flightLogbookEnabled()) return;

    SdMutex::Guard guard;

    ensureCurrentDate();
    if (currentDateStr[0] == 0) return;

    static Aircraft snapshot[Config::MAX_TRACKED_AIRCRAFT];
    uint8_t count = 0;

    AircraftTable::lock();
    Aircraft* table = AircraftTable::raw();
    for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
        if (table[i].valid) snapshot[count++] = table[i];
    }
    AircraftTable::unlock();

    bool anyNew = false;
    for (uint8_t i = 0; i < count; i++) {
        if (snapshot[i].hex[0] && !alreadySeen(snapshot[i].hex)) { anyNew = true; break; }
    }
    if (!anyNew) return;

    char filename[64];
    logFilename(filename, sizeof(filename));
    bool needsHeader = !SD.exists(filename);
    yield();

    File f = SD.open(filename, FILE_APPEND);
    if (!f) return;
    if (needsHeader) {
        f.println("timestamp,hex,callsign,reg,type,distance_km,altitude_ft");
    }

    for (uint8_t i = 0; i < count; i++) {
        if (!snapshot[i].hex[0]) continue;
        if (alreadySeen(snapshot[i].hex)) continue;
        markSeen(snapshot[i].hex);
        writeLogLine(f, snapshot[i]);
        yield();
    }

    f.close();
}

uint16_t todayCount() { return seenCount; }

TopAltitude todayMaxAltitude() {
    TopAltitude result;

    SdMutex::Guard guard;
    if (currentDateStr[0] == 0) return result; // Uhrzeit noch nicht bekannt

    char filename[64];
    logFilename(filename, sizeof(filename));
    if (!SD.exists(filename)) return result;

    File f = SD.open(filename, FILE_READ);
    if (!f) return result;

    bool firstLine = true;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (firstLine) { firstLine = false; continue; } // CSV-Header ueberspringen
        if (line.length() == 0) continue;

        // Spalten: timestamp,hex,callsign,reg,type,distance_km,altitude_ft
        int commaIdx[6];
        int found = 0;
        int searchFrom = 0;
        for (int c = 0; c < 6; c++) {
            int idx = line.indexOf(',', searchFrom);
            if (idx < 0) break;
            commaIdx[c] = idx;
            searchFrom = idx + 1;
            found++;
        }
        if (found < 6) continue; // unvollstaendige/kaputte Zeile ueberspringen

        String callsign = line.substring(commaIdx[1] + 1, commaIdx[2]);
        String altStr = line.substring(commaIdx[5] + 1);
        altStr.trim();
        if (altStr.length() == 0) continue;

        int32_t alt = altStr.toInt();
        if (alt > result.altitudeFt || !result.found) {
            result.found = true;
            result.altitudeFt = alt;
            callsign.trim();
            strncpy(result.callsign, callsign.c_str(), sizeof(result.callsign) - 1);
            result.callsign[sizeof(result.callsign) - 1] = 0;
        }
        yield();
    }
    f.close();

    return result;
}

void computeAllTimeStats(uint32_t& totalAircraft, uint16_t& totalDays) {
    totalAircraft = 0;
    totalDays = 0;

    SdMutex::Guard guard;

    File dir = SD.open(Config::SD_LOG_DIR);
    if (!dir || !dir.isDirectory()) return;

    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String name = String(entry.name());
            if (name.endsWith(".csv")) {
                totalDays++;
                uint32_t lines = countLinesFast(entry);
                if (lines > 0) totalAircraft += (lines - 1);
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
}

uint8_t listDays(DayEntry* out, uint8_t maxEntries) {
    uint8_t filled = 0;

    SdMutex::Guard guard;

    File dir = SD.open(Config::SD_LOG_DIR);
    if (!dir || !dir.isDirectory()) return 0;

    File entry = dir.openNextFile();
    while (entry && filled < maxEntries) {
        if (!entry.isDirectory()) {
            String name = String(entry.name());
            if (name.endsWith(".csv")) {
                String dateOnly = name.substring(0, name.length() - 4);
                int slashIdx = dateOnly.lastIndexOf('/');
                if (slashIdx >= 0) dateOnly = dateOnly.substring(slashIdx + 1);

                uint32_t lines = countLinesFast(entry);

                strncpy(out[filled].date, dateOnly.c_str(), sizeof(out[filled].date) - 1);
                out[filled].count = (lines > 0) ? (lines - 1) : 0;
                filled++;
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    return filled;
}

void resetAllData() {
    SdMutex::Guard guard;

    File dir = SD.open(Config::SD_LOG_DIR);
    if (dir && dir.isDirectory()) {
        File entry = dir.openNextFile();
        while (entry) {
            bool isDir = entry.isDirectory();
            String name = String(entry.name());
            entry.close();

            if (!isDir) {
                String fullPath = name.startsWith("/")
                                       ? name
                                       : String(Config::SD_LOG_DIR) + "/" + name;
                SD.remove(fullPath);
            }
            entry = dir.openNextFile();
        }
        dir.close();
    }

    seenCount = 0;
    currentDateStr[0] = 0;
    ensureCurrentDate();
}

}