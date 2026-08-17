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
    // 24h-Sicherheitsabschaltung: falls das Flugbuch aus Versehen dauerhaft
    // aktiviert bleibt, schaltet es sich nach spaetestens 24 Stunden von
    // selbst wieder aus. Wird zuverlaessig durchgesetzt unabhaengig vom
    // Erfolg der ADS-B-Abfrage (siehe enforceAutoOff() unten sowie
    // net_task.cpp).
    constexpr uint32_t LOGBOOK_AUTO_OFF_SECONDS = 24UL * 3600UL;

    constexpr uint16_t MAX_SEEN = 400;
    char seenHex[MAX_SEEN][7];
    uint16_t seenCount = 0;

    // Datei der aktuell laufenden Aufzeichnungs-Sitzung (ohne ".csv"), z.B.
    // "2026-08-06" fuer die erste Sitzung eines Tages oder "2026-08-06_2"
    // fuer ein erneutes Einschalten am selben Tag. Leer = noch nicht
    // aufgeloest (Uhrzeit noch nicht synchronisiert oder Flugbuch aus).
    char currentSessionFile[16] = {0};

    void formatDateFromEpoch(uint32_t epoch, char* out, size_t outSize) {
        time_t t = (time_t)epoch;
        struct tm tmv;
        localtime_r(&t, &tmv);
        snprintf(out, outSize, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
    }

    // Findet fuer den gegebenen Aktivierungszeitpunkt eine noch nicht
    // existierende Logbuch-Datei: "<Datum>.csv" fuer die erste Sitzung
    // eines Tages, "<Datum>_2.csv", "_3.csv" usw. fuer erneutes Einschalten
    // am selben Tag - so bekommt jede Sitzung ihre eigene, im
    // Logbuch-Dateien-Screen einzeln loeschbare Datei, statt in eine
    // bestehende hineinzuschreiben.
    void resolveSessionFilename(uint32_t epoch, char* out, size_t outSize) {
        char dateStr[11];
        formatDateFromEpoch(epoch, dateStr, sizeof(dateStr));

        char path[64];
        snprintf(path, sizeof(path), "%s/%s.csv", Config::SD_LOG_DIR, dateStr);
        if (!SD.exists(path)) {
            strncpy(out, dateStr, outSize - 1);
            out[outSize - 1] = 0;
            return;
        }

        for (uint8_t suffix = 2; suffix <= 50; suffix++) {
            char candidate[16];
            snprintf(candidate, sizeof(candidate), "%s_%d", dateStr, suffix);
            snprintf(path, sizeof(path), "%s/%s.csv", Config::SD_LOG_DIR, candidate);
            if (!SD.exists(path)) {
                strncpy(out, candidate, outSize - 1);
                out[outSize - 1] = 0;
                return;
            }
        }

        // Sehr unwahrscheinlicher Fall (>50 Sitzungen an einem Tag): letzten
        // Kandidaten weiterverwenden statt endlos zu suchen.
        strncpy(out, dateStr, outSize - 1);
        out[outSize - 1] = 0;
    }

    void logFilename(char* out, size_t outSize) {
        snprintf(out, outSize, "%s/%s.csv", Config::SD_LOG_DIR, currentSessionFile);
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
            if (blocksRead % 8 == 0) delay(1);
        }
        return lines;
    }

    void loadSeenFromCurrentFile() {
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
            if (blocksRead % 8 == 0) delay(1);
        }
        f.close();
    }

    // Loest die Datei der aktuellen Sitzung auf (einmalig pro Sitzung) bzw.
    // uebernimmt sie nach einem Neustart erneut aus den Einstellungen -
    // nur aufrufen, wenn das Flugbuch gerade eingeschaltet ist.
    void ensureSessionFile() {
        time_t now = time(nullptr);
        if (now <= 8 * 3600 * 2) return; // Uhrzeit noch nicht synchronisiert

        String persisted = SettingsStore::flightLogbookSessionFile();
        if (persisted.length() > 0) {
            if (strcmp(currentSessionFile, persisted.c_str()) != 0) {
                strncpy(currentSessionFile, persisted.c_str(), sizeof(currentSessionFile) - 1);
                currentSessionFile[sizeof(currentSessionFile) - 1] = 0;
                loadSeenFromCurrentFile();
            }
            return;
        }

        // Keine Sitzungsdatei hinterlegt - entweder frisches Einschalten
        // (menu_screen.cpp loescht den Eintrag beim Umschalten bewusst) oder
        // Migration von einer alten Firmware ohne Sitzungslogik. In beiden
        // Faellen jetzt eine neue, garantiert einzigartige Datei anlegen.
        uint32_t enabledAt = SettingsStore::flightLogbookEnabledAtEpoch();
        if (enabledAt == 0) enabledAt = (uint32_t)now;
        resolveSessionFilename(enabledAt, currentSessionFile, sizeof(currentSessionFile));
        SettingsStore::setFlightLogbookSessionFile(currentSessionFile);
        seenCount = 0;
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
    if (SettingsStore::flightLogbookEnabled()) {
        ensureSessionFile();
    }
}

// 24h-Sicherheitsabschaltung: verhindert, dass ein unbemerkt aktives
// Flugbuch die SD-Karte nach und nach vollschreibt (siehe
// Bestaetigungsdialog beim Einschalten in menu_screen.cpp). Nur pruefen,
// wenn die Uhrzeit schon synchronisiert ist.
//
// FRUEHER Teil von update() und wurde deshalb NUR bei einer ERFOLGREICHEN
// ADS-B-Abfrage geprueft (update() wird in net_task.cpp nur im "if
// (result.ok)"-Zweig aufgerufen) - schlugen die Abfragen laengere Zeit fehl
// (WLAN-Aussetzer, Ausfall des ADS-B-Anbieters), lief die 24h-Grenze
// unbemerkt weiter, ohne dass die Sicherheitsabschaltung je greifen konnte.
// Jetzt eine eigene Funktion, die NetTask bei JEDEM Schleifendurchlauf
// aufruft (siehe enforceAutoOff()), unabhaengig vom Abfrageerfolg.
//
// Rueckgabe: true, wenn das Flugbuch danach noch aktiv ist - false, wenn es
// gerade abgeschaltet wurde oder ohnehin schon aus war.
bool checkAutoOff() {
    if (!SettingsStore::flightLogbookEnabled()) return false;

    time_t nowCheck = time(nullptr);
    if (nowCheck <= 8 * 3600 * 2) return true; // Uhrzeit noch nicht synchronisiert - noch nicht pruefbar

    uint32_t enabledAt = SettingsStore::flightLogbookEnabledAtEpoch();
    if (enabledAt == 0) {
        // Migrations-Fall: Flugbuch war schon vor diesem Update aktiv (alte
        // Einstellungsdatei ohne Zeitstempel) - Startzeitpunkt jetzt setzen,
        // damit die 24h-Grenze trotzdem sicher greift.
        SettingsStore::setFlightLogbookEnabledAtEpoch((uint32_t)nowCheck);
        return true;
    }

    if ((uint32_t)nowCheck >= enabledAt && (uint32_t)nowCheck - enabledAt >= LOGBOOK_AUTO_OFF_SECONDS) {
        SettingsStore::setFlightLogbookEnabled(false);
        SettingsStore::setFlightLogbookEnabledAtEpoch(0);
        SettingsStore::setFlightLogbookSessionFile("");
        return false;
    }

    return true;
}

// Von NetTask bei JEDEM Schleifendurchlauf aufgerufen (siehe net_task.cpp),
// unabhaengig davon, ob die letzte ADS-B-Abfrage erfolgreich war - siehe
// Kommentar bei checkAutoOff() fuer den Grund.
void enforceAutoOff() {
    checkAutoOff();
}

void update() {
    if (!checkAutoOff()) return;

    SdMutex::Guard guard;

    ensureSessionFile();
    if (currentSessionFile[0] == 0) return;

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
    if (currentSessionFile[0] == 0) return result; // noch keine Sitzungsdatei bekannt

    char filename[64];
    logFilename(filename, sizeof(filename));
    if (!SD.exists(filename)) return result;

    File f = SD.open(filename, FILE_READ);
    if (!f) return result;

    bool firstLine = true;
    uint32_t lineIdx = 0;
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
        lineIdx++;
        if (lineIdx % 16 == 0) delay(1);
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
                out[filled].date[sizeof(out[filled].date) - 1] = 0;
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

uint8_t listDaySummaries(DayEntry* out, uint8_t maxEntries) {
    // Scannt grosszuegiger als maxEntries, damit auch bei vielen einzelnen
    // Sitzungs-Dateien pro Tag noch korrekt pro Kalendertag aufsummiert
    // wird, bevor auf die angeforderte Anzahl Tage begrenzt wird.
    constexpr uint8_t MAX_RAW_SCAN = 90;
    static DayEntry raw[MAX_RAW_SCAN];
    uint8_t rawCount = listDays(raw, MAX_RAW_SCAN);

    uint8_t outCount = 0;
    for (uint8_t i = 0; i < rawCount; i++) {
        char dayKey[11];
        strncpy(dayKey, raw[i].date, 10);
        dayKey[10] = 0;

        int8_t existing = -1;
        for (uint8_t j = 0; j < outCount; j++) {
            if (strcmp(out[j].date, dayKey) == 0) { existing = j; break; }
        }
        if (existing >= 0) {
            out[existing].count += raw[i].count;
        } else if (outCount < maxEntries) {
            strncpy(out[outCount].date, dayKey, sizeof(out[outCount].date) - 1);
            out[outCount].date[sizeof(out[outCount].date) - 1] = 0;
            out[outCount].count = raw[i].count;
            outCount++;
        }
    }
    return outCount;
}

bool deleteFile(const char* label) {
    SdMutex::Guard guard;

    char path[64];
    snprintf(path, sizeof(path), "%s/%s.csv", Config::SD_LOG_DIR, label);
    if (!SD.exists(path)) return false;

    bool ok = SD.remove(path);
    if (ok && strcmp(label, currentSessionFile) == 0) {
        // Die gerade aktive Sitzungsdatei wurde geloescht - Dopplungs-Liste
        // zuruecksetzen, damit neue Sichtungen wieder korrekt in die (beim
        // naechsten Schreibvorgang neu angelegte) Datei geloggt werden.
        seenCount = 0;
    }
    return ok;
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
    currentSessionFile[0] = 0;
    if (SettingsStore::flightLogbookEnabled()) {
        ensureSessionFile();
    }
}

uint8_t computeTopAircraft(TopAircraft* out, uint8_t maxEntries) {
    SdMutex::Guard guard;

    // Begrenzte Merkliste unterschiedlicher Flugzeuge (nach Hex-Code) ueber
    // ALLE Logbuch-Dateien hinweg - 160 reicht fuer den ueblichen Gebrauch an
    // einem Heimstandort deutlich (zum Vergleich: MAX_SEEN=400 gilt nur fuer
    // EINEN Tag). Wird die Grenze doch erreicht, werden weitere NEUE
    // Flugzeuge einfach nicht mehr mitgezaehlt - bereits erfasste Flugzeuge
    // zaehlen aber korrekt weiter. Kein Fehlerfall, nur eine sehr
    // theoretische Einschraenkung bei extrem vielen unterschiedlichen
    // Flugzeugen ueber die gesamte Aufzeichnungsdauer.
    constexpr uint16_t MAX_TRACKED = 160;
    static char trackHex[MAX_TRACKED][7];
    static char trackReg[MAX_TRACKED][10];
    static uint32_t trackCount[MAX_TRACKED];
    uint16_t trackedN = 0;

    File dir = SD.open(Config::SD_LOG_DIR);
    if (!dir || !dir.isDirectory()) return 0;

    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String name = String(entry.name());
            if (name.endsWith(".csv")) {
                constexpr size_t BUF_SIZE = 512;
                static uint8_t buf[BUF_SIZE];
                char lineBuf[64];
                size_t lineLen = 0;
                bool firstLine = true;
                uint32_t blocksRead = 0;

                // Spalten: timestamp,hex,callsign,reg,type,distance_km,altitude_ft
                auto processLine = [&]() {
                    if (lineLen == 0) return;
                    if (firstLine) { firstLine = false; return; }
                    lineBuf[lineLen] = 0;

                    char* p1 = strchr(lineBuf, ',');
                    if (!p1) return;
                    char* p2 = strchr(p1 + 1, ',');
                    if (!p2) return;
                    char* p3 = strchr(p2 + 1, ',');
                    if (!p3) return;
                    char* p4 = strchr(p3 + 1, ',');
                    if (!p4) return;

                    size_t hexLen = (size_t)(p2 - (p1 + 1));
                    if (hexLen == 0 || hexLen >= sizeof(trackHex[0])) return;
                    char hexBuf[7] = {0};
                    memcpy(hexBuf, p1 + 1, hexLen);

                    size_t regLen = (size_t)(p4 - (p3 + 1));
                    char regBuf[10] = {0};
                    if (regLen > 0 && regLen < sizeof(regBuf)) {
                        memcpy(regBuf, p3 + 1, regLen);
                    }

                    int16_t idx = -1;
                    for (uint16_t i = 0; i < trackedN; i++) {
                        if (strcmp(trackHex[i], hexBuf) == 0) { idx = (int16_t)i; break; }
                    }
                    if (idx < 0) {
                        if (trackedN >= MAX_TRACKED) return;
                        idx = (int16_t)trackedN;
                        strncpy(trackHex[idx], hexBuf, sizeof(trackHex[idx]) - 1);
                        trackHex[idx][sizeof(trackHex[idx]) - 1] = 0;
                        trackReg[idx][0] = 0;
                        trackCount[idx] = 0;
                        trackedN++;
                    }
                    trackCount[idx]++;
                    if (regBuf[0]) {
                        strncpy(trackReg[idx], regBuf, sizeof(trackReg[idx]) - 1);
                        trackReg[idx][sizeof(trackReg[idx]) - 1] = 0;
                    }
                };

                while (entry.available()) {
                    size_t n = entry.read(buf, BUF_SIZE);
                    for (size_t i = 0; i < n; i++) {
                        char c = (char)buf[i];
                        if (c == '\n' || c == '\r') {
                            if (lineLen > 0) processLine();
                            lineLen = 0;
                        } else if (lineLen < sizeof(lineBuf) - 1) {
                            lineBuf[lineLen++] = c;
                        }
                    }
                    blocksRead++;
                    // Gleicher Watchdog-Fix wie ueberall sonst in dieser
                    // Datei - siehe ausfuehrlicher Kommentar in
                    // countLinesFast() oben (delay(1) statt yield()).
                    if (blocksRead % 8 == 0) delay(1);
                }
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    // Einfache Auswahl-Sortierung (Selection Sort) - trackedN ist klein
    // genug (max. MAX_TRACKED=160), dass O(n^2) hier keine Rolle spielt.
    uint8_t resultCount = (trackedN < maxEntries) ? (uint8_t)trackedN : maxEntries;
    for (uint8_t r = 0; r < resultCount; r++) {
        uint16_t bestIdx = r;
        for (uint16_t i = (uint16_t)(r + 1); i < trackedN; i++) {
            if (trackCount[i] > trackCount[bestIdx]) bestIdx = i;
        }
        if (bestIdx != r) {
            uint32_t tmpCount = trackCount[r];
            trackCount[r] = trackCount[bestIdx];
            trackCount[bestIdx] = tmpCount;
            char tmpHex[7];
            strcpy(tmpHex, trackHex[r]);
            strcpy(trackHex[r], trackHex[bestIdx]);
            strcpy(trackHex[bestIdx], tmpHex);
            char tmpReg[10];
            strcpy(tmpReg, trackReg[r]);
            strcpy(trackReg[r], trackReg[bestIdx]);
            strcpy(trackReg[bestIdx], tmpReg);
        }
        strncpy(out[r].hex, trackHex[r], sizeof(out[r].hex) - 1);
        out[r].hex[sizeof(out[r].hex) - 1] = 0;
        strncpy(out[r].reg, trackReg[r], sizeof(out[r].reg) - 1);
        out[r].reg[sizeof(out[r].reg) - 1] = 0;
        out[r].sightings = trackCount[r];
    }
    return resultCount;
}

}
