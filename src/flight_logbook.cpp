#include "flight_logbook.h"
#include "config.h"
#include "aircraft.h"
#include "aircraft_table.h"
#include "settings_store.h"
#include "sd_mutex.h"
#include <SD.h>
#include <time.h>
#include <cstring>
#include <cctype>
#include <cstdint>
#include <atomic>

namespace FlightLogbook {

namespace {
    // 24h-Sicherheitsabschaltung: falls das Flugbuch aus Versehen dauerhaft
    // aktiviert bleibt, schaltet es sich nach spaetestens 24 Stunden von
    // selbst wieder aus. Wird zuverlaessig durchgesetzt unabhaengig vom
    // Erfolg der ADS-B-Abfrage (siehe enforceAutoOff() unten sowie
    // net_task.cpp).
    constexpr uint32_t LOGBOOK_AUTO_OFF_SECONDS = 24UL * 3600UL;

    // Siehe consumeAutoOffNotice()/checkAutoOff() - Core 0 (NetTask)
    // schreibt, Core 1 (main.cpp::loop()) liest/konsumiert.
    std::atomic<bool> pendingAutoOffNotice{false};

    constexpr uint16_t MAX_SEEN = 400;
    char seenHex[MAX_SEEN][7];
    uint16_t seenCount = 0;

    // "Flugzeug-Steckbrief" (siehe Absprache im Chat): der eigentliche
    // Logbuch-Eintrag wird nicht mehr beim ERSTEN Sichten geschrieben,
    // sondern erst, wenn ein Flugzeug aus AircraftTable VERSCHWINDET (egal
    // ob durch normales Altern oder durch Verdraengung in der 40er-
    // Tabelle) - erst dann stehen die finalen sessionMinDistanceKm/
    // sessionMaxSpeedKt fest. seenHex/seenCount/alreadySeen()/markSeen()
    // oben bleiben UNVERAENDERT die "heute schon gesehen"-Zaehlung (weiter
    // sofort beim ersten Sichten gesetzt, siehe update() unten) - diese
    // beiden Arrays hier sind eine ZUSAETZLICHE, davon unabhaengige
    // Verfolgung "aktuell sichtbar, aber noch nicht geloggt".
    //
    // Ein Hex-Code wird NUR aufgenommen, wenn er noch nicht alreadySeen()
    // ist (also wirklich neu) - kommt ein bereits einmal geloggtes und
    // wieder verschwundenes Flugzeug spaeter in derselben Sitzung erneut
    // ins Bild, wird es NICHT erneut getrackt/geloggt (weiterhin maximal
    // ein Logbuch-Eintrag pro Flugzeug und Sitzung, wie bisher - nur der
    // Zeitpunkt des Schreibens hat sich verschoben). Groesse = maximale
    // AircraftTable-Kapazitaet, mehr gleichzeitig verfolgte Flugzeuge sind
    // physisch nicht moeglich.
    Aircraft trackedAircraft[Config::MAX_TRACKED_AIRCRAFT];
    bool trackedValid[Config::MAX_TRACKED_AIRCRAFT] = {};

    // Tatsaechliche Anzahl bereits geschriebener CSV-Zeilen der aktuellen
    // Sitzungsdatei - ANDERS als seenCount jetzt NICHT mehr identisch mit
    // der Anzahl "heute gesehener" Flugzeuge (die werden sofort gezaehlt,
    // geloggt wird aber erst spaeter). cachedLineCountFor() unten braucht
    // diesen eigenen Zaehler, um weiterhin ohne SD-Zugriff die korrekte
    // Zeilenzahl der noch wachsenden aktiven Datei zu kennen.
    uint16_t loggedLineCount = 0;

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

    // Performance-Fix (Alex' Meldung: Web-UI-Seitenaufruf brauchte ~9,6s,
    // fast komplett in listDays()/countLinesFast()) - eine bereits
    // ABGESCHLOSSENE Logbuch-Datei (nicht mehr currentSessionFile) aendert
    // sich nie wieder, ihre Zeilenzahl kann also nach dem ersten Zaehlen
    // fuer den Rest der Betriebszeit unveraendert wiederverwendet werden,
    // statt bei JEDEM Aufruf (z.B. jedem Web-UI-Seitenaufruf) erneut den
    // kompletten Dateiinhalt einzulesen. Reiner RAM-Cache (keine SD-
    // Persistierung noetig) - der volle Lesevorgang wird so pro Datei
    // hoechstens EINMAL pro Boot bezahlt, nicht bei jedem Aufruf.
    //
    // Fuer die AKTIVE Sitzungsdatei (currentSessionFile) wird NIE gecacht,
    // sondern der Wert direkt aus dem ohnehin schon im RAM gefuehrten
    // seenCount abgeleitet (siehe markSeen()/update() - jede neue Zeile
    // erhoeht seenCount im selben Moment, in dem sie geschrieben wird, die
    // beiden sind also immer exakt synchron) - kein SD-Zugriff noetig, und
    // garantiert nie veraltet, waehrend die Datei noch waechst.
    constexpr uint8_t LINE_COUNT_CACHE_SIZE = 96; // > MAX_RAW_SCAN (90) an anderen Stellen
    struct LineCountCacheEntry {
        char label[16] = {0}; // Dateiname ohne Pfad/".csv", siehe resolveSessionFilename()
        uint32_t lines = 0;
        bool valid = false;
    };
    LineCountCacheEntry lineCountCache[LINE_COUNT_CACHE_SIZE];

    // Liefert die Zeilenzahl (inkl. Kopfzeile, wie countLinesFast()) fuer
    // die Logbuch-Datei mit diesem Label. "entry" muss bereits geoeffnet
    // sein - wird nur bei einem tatsaechlichen Cache-Miss gelesen.
    uint32_t cachedLineCountFor(File& entry, const char* label) {
        if (currentSessionFile[0] && strcmp(label, currentSessionFile) == 0) {
            // NICHT mehr seenCount (das zaehlt seit dem "Flugzeug-
            // Steckbrief"-Umbau nur noch "heute schon gesehen", nicht mehr
            // 1:1 die tatsaechliche Zeilenzahl - siehe loggedLineCount
            // oben) - sondern die tatsaechlich geschriebenen CSV-Zeilen.
            return (uint32_t)loggedLineCount + 1; // +1 fuer die Kopfzeile
        }
        for (uint8_t i = 0; i < LINE_COUNT_CACHE_SIZE; i++) {
            if (lineCountCache[i].valid && strcmp(lineCountCache[i].label, label) == 0) {
                return lineCountCache[i].lines;
            }
        }
        uint32_t lines = countLinesFast(entry);
        // Ersten freien Platz belegen - ist der Cache voll (sehr viele
        // Tage), wird der Wert einfach nicht abgelegt (naechster Aufruf
        // zaehlt diese eine Datei dann erneut) statt einen bestehenden,
        // noch gueltigen Eintrag zu verdraengen.
        for (uint8_t i = 0; i < LINE_COUNT_CACHE_SIZE; i++) {
            if (!lineCountCache[i].valid) {
                strncpy(lineCountCache[i].label, label, sizeof(lineCountCache[i].label) - 1);
                lineCountCache[i].lines = lines;
                lineCountCache[i].valid = true;
                break;
            }
        }
        return lines;
    }

    // Extrahiert das Label (Dateiname ohne Verzeichnis-Praefix und ohne
    // ".csv") aus einem SD-Dateinamen - dieselbe Logik, die bisher einzeln
    // in listDays() und computeAllTimeStats() stand, jetzt an einer Stelle
    // fuer beide (und cachedLineCountFor() oben).
    String labelFromEntryName(const String& name) {
        String label = name.substring(0, name.length() - 4); // ".csv" abschneiden
        int slashIdx = label.lastIndexOf('/');
        if (slashIdx >= 0) label = label.substring(slashIdx + 1);
        return label;
    }

    // Entfernt (falls vorhanden) den Cache-Eintrag fuer genau dieses Label -
    // noetig bei deleteFile()/resetAllData(), sonst koennte eine SPAETER neu
    // angelegte Datei mit demselben Datums-Label (z.B. Flugbuch am selben
    // Tag geloescht und erneut eingeschaltet) faelschlich die alte,
    // gecachte Zeilenzahl der geloeschten Datei uebernehmen.
    void invalidateLineCountCache(const char* label) {
        for (uint8_t i = 0; i < LINE_COUNT_CACHE_SIZE; i++) {
            if (lineCountCache[i].valid && strcmp(lineCountCache[i].label, label) == 0) {
                lineCountCache[i].valid = false;
                return;
            }
        }
    }

    void clearLineCountCache() {
        for (uint8_t i = 0; i < LINE_COUNT_CACHE_SIZE; i++) lineCountCache[i].valid = false;
    }

    // Performance-Fix Teil 2 (Alex' Meldung: selbst OHNE Zeilenzaehlung
    // brauchte der reine SD-Verzeichnis-Scan - nur openNextFile()/close()
    // fuer 7 Eintraege, kein Dateiinhalt - noch ~4s, weit ausserhalb dessen,
    // was fuer einen SD-Kartenzugriff normal ist). Die TAGESLISTE selbst
    // (welche .csv-Dateien ueberhaupt existieren) aendert sich nur an drei
    // Stellen: (1) eine neue Datei entsteht bei Tageswechsel/erstem
    // Einschalten (siehe update()'s needsHeader-Zweig), (2) deleteFile(),
    // (3) resetAllData() - an genau diesen drei Stellen wird der Cache
    // unten invalidiert/aktualisiert, sonst bleibt er fuer den Rest der
    // Betriebszeit unveraendert im RAM stehen, statt bei JEDEM Web-UI-
    // Aufruf das komplette SD-Verzeichnis erneut zu durchsuchen.
    struct DayListCache {
        DayEntry entries[LINE_COUNT_CACHE_SIZE];
        uint8_t count = 0;
        bool valid = false;
    };
    DayListCache dayListCache;

    void invalidateDayListCache() { dayListCache.valid = false; }

    // Baut dayListCache per vollem SD-Verzeichnis-Scan neu auf (genau EIN
    // solcher Scan pro tatsaechlicher Aenderung der Dateiliste, nicht mehr
    // einer pro Web-UI-Aufruf) - dieselbe Schleife, die vorher direkt in
    // listDays() stand, schreibt jetzt in den Cache statt in den
    // Aufrufer-Puffer. Erwartet, dass der SdMutex bereits gehalten wird.
    void rebuildDayListCache() {
        dayListCache.count = 0;
        dayListCache.valid = true; // schon hier setzen: ein leeres Verzeichnis ist ein gueltiges (nicht staendig neu zu scannendes) Ergebnis

        File dir = SD.open(Config::SD_LOG_DIR);
        if (!dir || !dir.isDirectory()) return;

        File entry = dir.openNextFile();
        while (entry && dayListCache.count < LINE_COUNT_CACHE_SIZE) {
            if (!entry.isDirectory()) {
                String name = String(entry.name());
                if (name.endsWith(".csv")) {
                    String label = labelFromEntryName(name);
                    uint32_t lines = cachedLineCountFor(entry, label.c_str());
                    DayEntry& out = dayListCache.entries[dayListCache.count];
                    strncpy(out.date, label.c_str(), sizeof(out.date) - 1);
                    out.date[sizeof(out.date) - 1] = 0;
                    out.count = (lines > 0) ? (lines - 1) : 0;
                    dayListCache.count++;
                }
            }
            entry.close();
            entry = dir.openNextFile();
        }
        dir.close();
    }

    void loadSeenFromCurrentFile() {
        seenCount = 0;
        loggedLineCount = 0;
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
            // Jede echte (Nicht-Kopf-)Zeile ist eine bereits geschriebene
            // CSV-Zeile - siehe loggedLineCount-Kommentar oben (nach einem
            // Neustart muss dieser Zaehler aus der bestehenden Datei
            // rekonstruiert werden, exakt wie seenCount/seenHex).
            loggedLineCount++;
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
    //
    // BUGFIX (Alex' Meldung: der 7-Tage-Verlauf zeigte wochenlang nur einen
    // einzigen, uralten Tageseintrag - alle Sichtungen seitdem landeten
    // faelschlich weiter in genau dieser einen Datei): Diese Funktion hat
    // bisher NIE geprueft, ob sich das Kalenderdatum gegenueber der
    // persistierten Sitzungsdatei geaendert hat - eine einmal angelegte
    // Datei wurde fuer immer weiterverwendet, bis das Flugbuch manuell aus-
    // und wieder eingeschaltet wurde (menu_screen.cpp loescht den Eintrag
    // dabei bewusst) oder die 24h-Sicherheitsabschaltung griff. Lief das
    // Geraet einfach durch (oder wurde - wie waehrend dieser Testphase -
    // oft neu gestartet, was den 24h-Timer via enabledAt==0-Zweig in
    // checkAutoOff() jedesmal neu startete, ohne je 24h zu erreichen),
    // blieb es dauerhaft bei derselben Datei stehen. Jetzt wird bei JEDEM
    // Aufruf das Datum der persistierten Datei mit dem tatsaechlichen
    // heutigen Datum verglichen - weicht es ab, wird genau wie beim
    // allerersten Einschalten eine neue Datei fuer HEUTE angelegt.
    void ensureSessionFile() {
        time_t now = time(nullptr);
        if (now <= 8 * 3600 * 2) return; // Uhrzeit noch nicht synchronisiert

        char todayStr[11];
        formatDateFromEpoch((uint32_t)now, todayStr, sizeof(todayStr));

        String persisted = SettingsStore::flightLogbookSessionFile();
        bool sameDay = persisted.length() >= 10 && persisted.substring(0, 10) == String(todayStr);

        if (persisted.length() > 0 && sameDay) {
            if (strcmp(currentSessionFile, persisted.c_str()) != 0) {
                strncpy(currentSessionFile, persisted.c_str(), sizeof(currentSessionFile) - 1);
                currentSessionFile[sizeof(currentSessionFile) - 1] = 0;
                loadSeenFromCurrentFile();
            }
            return;
        }

        // Keine (gueltige, tagesaktuelle) Sitzungsdatei hinterlegt -
        // frisches Einschalten, Migration von einer alten Firmware ohne
        // Sitzungslogik, oder ein neuer Kalendertag hat begonnen (siehe
        // Tageswechsel-Check oben). In jedem Fall jetzt eine neue,
        // garantiert einzigartige Datei fuer HEUTE anlegen - "now" statt
        // des potenziell tagealten "enabledAt", damit das Dateidatum immer
        // dem tatsaechlichen Anlegezeitpunkt entspricht.
        resolveSessionFilename((uint32_t)now, currentSessionFile, sizeof(currentSessionFile));
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

        // min_distance_km/max_speed_kt (Spalten 8/9, "Flugzeug-Steckbrief")
        // - final zum Zeitpunkt des Verschwindens aus der Tabelle, siehe
        // aircraft.h::sessionMinDistanceKm/sessionMaxSpeedKt und deren
        // laufende Aktualisierung in aircraft_table.cpp::postFetchUpdate().
        // Beide sollten hier praktisch nie mehr -1 (Default) sein, da diese
        // Funktion erst aufgerufen wird, nachdem das Flugzeug mindestens
        // einen postFetchUpdate()-Durchlauf durchlaufen hat - trotzdem mit
        // max(0, ...) abgesichert, um niemals einen negativen Wert in die
        // CSV zu schreiben (siehe Testauftrag: "sinnvolle, nicht negative
        // Werte").
        float loggedMinDist = a.sessionMinDistanceKm >= 0 ? a.sessionMinDistanceKm : a.distanceKm;
        float loggedMaxSpeed = a.sessionMaxSpeedKt >= 0 ? a.sessionMaxSpeedKt : a.groundSpeedKt;
        f.printf("%s,%s,%s,%s,%s,%.1f,%d,%.1f,%.1f\n",
                 timestamp,
                 a.hex,
                 a.callsign[0] ? a.callsign : "",
                 a.reg[0] ? a.reg : "",
                 a.typeCode[0] ? a.typeCode : "",
                 a.distanceKm,
                 (int)a.altBaroFt,
                 loggedMinDist,
                 loggedMaxSpeed);
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
        // Markiert, dass diese Abschaltung automatisch (nicht durch
        // bewusstes Antippen) erfolgte - steuert einen kleinen Hinweis-Punkt
        // im Menue (siehe menu_screen.cpp), damit das nicht mehr unbemerkt
        // bleibt (Alex' Meldung: Flugbuch war wochenlang unbemerkt aus).
        SettingsStore::setFlightLogbookAutoOffTriggered(true);
        // Loest den proaktiven Hinweis-Screen aus (siehe
        // consumeAutoOffNotice()/main.cpp::loop()) - unabhaengig davon, ob
        // dies waehrend des laufenden Betriebs greift oder der allererste
        // Check nach einem laengeren Stromausfall ist (siehe Kommentar bei
        // consumeAutoOffNotice() in flight_logbook.h).
        pendingAutoOffNotice.store(true, std::memory_order_relaxed);
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

int32_t secondsUntilAutoOff() {
    if (!SettingsStore::flightLogbookEnabled()) return -1;

    time_t nowCheck = time(nullptr);
    if (nowCheck <= 8 * 3600 * 2) return -1; // Uhrzeit noch nicht synchronisiert

    uint32_t enabledAt = SettingsStore::flightLogbookEnabledAtEpoch();
    if (enabledAt == 0) return -1; // Migrations-Fall, siehe checkAutoOff()

    if ((uint32_t)nowCheck <= enabledAt) return (int32_t)LOGBOOK_AUTO_OFF_SECONDS;
    uint32_t elapsed = (uint32_t)nowCheck - enabledAt;
    if (elapsed >= LOGBOOK_AUTO_OFF_SECONDS) return 0;
    return (int32_t)(LOGBOOK_AUTO_OFF_SECONDS - elapsed);
}

bool consumeAutoOffNotice() {
    return pendingAutoOffNotice.exchange(false, std::memory_order_relaxed);
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

    // Schritt 1: fuer jedes aktuell sichtbare Flugzeug den laufenden
    // "Steckbrief"-Datensatz aktualisieren (neueste sessionMinDistanceKm/
    // sessionMaxSpeedKt aus aircraft_table.cpp::postFetchUpdate()
    // uebernehmen), bzw. ein wirklich NEUES Flugzeug (noch nicht
    // alreadySeen) frisch aufnehmen - "heute schon gesehen" (seenHex/
    // seenCount) wird dabei weiterhin SOFORT gesetzt wie bisher, nur das
    // eigentliche CSV-Schreiben passiert noch nicht hier (siehe Schritt 2).
    for (uint8_t j = 0; j < count; j++) {
        if (!snapshot[j].hex[0]) continue;

        int8_t trackedIdx = -1;
        for (uint8_t i = 0; i < Config::MAX_TRACKED_AIRCRAFT; i++) {
            if (trackedValid[i] && strcmp(trackedAircraft[i].hex, snapshot[j].hex) == 0) {
                trackedIdx = (int8_t)i;
                break;
            }
        }
        if (trackedIdx >= 0) {
            trackedAircraft[trackedIdx] = snapshot[j];
            continue;
        }

        // Nicht getrackt - entweder brandneu, oder bereits frueher in
        // dieser Sitzung geloggt und wieder verschwunden (dann bewusst
        // NICHT erneut tracken, siehe Kommentar bei trackedAircraft oben).
        if (alreadySeen(snapshot[j].hex)) continue;

        markSeen(snapshot[j].hex);
        for (uint8_t i = 0; i < Config::MAX_TRACKED_AIRCRAFT; i++) {
            if (!trackedValid[i]) {
                trackedAircraft[i] = snapshot[j];
                trackedValid[i] = true;
                break;
            }
        }
    }

    // Schritt 2: getrackte Flugzeuge, die JETZT nicht mehr in der aktuellen
    // Tabelle sind (egal ob normales Altern ueber AircraftTable::
    // postFetchUpdate()s STALE_TIMEOUT_MS, oder Verdraengung durch ein
    // naeheres Flugzeug in der "naechste 40"-Auswahl in adsb_client.cpp),
    // sind jetzt endgueltig verschwunden - genau DANN wird die Logbuch-
    // Zeile geschrieben, mit dem letzten bekannten Datensatz (inkl. finaler
    // sessionMinDistanceKm/sessionMaxSpeedKt). Ein Flugzeug, das das Geraet
    // waehrend eines laufenden Neustarts noch sichtbar war, geht dabei
    // verloren (trackedAircraft ist rein RAM-basiert, wie alle anderen
    // session-lokalen Aircraft-Felder auch) - akzeptierter Rand-Fall,
    // gleiches Prinzip wie bei firstSeenMs/prevDistanceKm.
    bool anyDisappeared = false;
    for (uint8_t i = 0; i < Config::MAX_TRACKED_AIRCRAFT; i++) {
        if (!trackedValid[i]) continue;
        bool stillPresent = false;
        for (uint8_t j = 0; j < count; j++) {
            if (snapshot[j].hex[0] && strcmp(snapshot[j].hex, trackedAircraft[i].hex) == 0) {
                stillPresent = true;
                break;
            }
        }
        if (!stillPresent) { anyDisappeared = true; break; }
    }
    if (!anyDisappeared) return;

    char filename[64];
    logFilename(filename, sizeof(filename));
    bool needsHeader = !SD.exists(filename);
    yield();

    File f = SD.open(filename, FILE_APPEND);
    if (!f) return;
    if (needsHeader) {
        f.println("timestamp,hex,callsign,reg,type,distance_km,altitude_ft,min_distance_km,max_speed_kt");
        // Eine neue Logbuch-Datei ist entstanden (Tageswechsel oder erstes
        // Einschalten) - die Tagesliste hat sich damit geaendert, siehe
        // rebuildDayListCache()-Kommentar oben.
        invalidateDayListCache();
    }

    for (uint8_t i = 0; i < Config::MAX_TRACKED_AIRCRAFT; i++) {
        if (!trackedValid[i]) continue;
        bool stillPresent = false;
        for (uint8_t j = 0; j < count; j++) {
            if (snapshot[j].hex[0] && strcmp(snapshot[j].hex, trackedAircraft[i].hex) == 0) {
                stillPresent = true;
                break;
            }
        }
        if (stillPresent) continue;
        writeLogLine(f, trackedAircraft[i]);
        loggedLineCount++;
        trackedValid[i] = false;
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
                uint32_t lines = cachedLineCountFor(entry, labelFromEntryName(name).c_str());
                if (lines > 0) totalAircraft += (lines - 1);
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();
}

uint8_t listDays(DayEntry* out, uint8_t maxEntries) {
    SdMutex::Guard guard;

    // Voller SD-Verzeichnis-Scan nur noch, wenn sich die Dateiliste seit
    // dem letzten Mal tatsaechlich geaendert haben KANN (siehe
    // invalidateDayListCache()-Aufrufe unten in update()/deleteFile()/
    // resetAllData()) - im Normalfall (mehrere Web-UI-Aufrufe zwischen zwei
    // solchen Aenderungen) ist dayListCache bereits gueltig, kein SD-
    // Zugriff noetig.
    if (!dayListCache.valid) rebuildDayListCache();

    uint8_t filled = 0;
    for (uint8_t i = 0; i < dayListCache.count && filled < maxEntries; i++) {
        out[filled] = dayListCache.entries[i];
        // Die AKTIVE Sitzungsdatei waechst laufend weiter, ohne dass sich
        // die Dateiliste selbst aendert (kein invalidateDayListCache()-
        // Aufruf dafuer) - ihr count wird deshalb hier bei JEDEM Aufruf
        // live aus dem ohnehin im RAM gefuehrten seenCount ueberschrieben
        // (kein SD-Zugriff), damit "heute" nie veraltet erscheint.
        if (currentSessionFile[0] && strcmp(out[filled].date, currentSessionFile) == 0) {
            out[filled].count = seenCount;
        }
        filled++;
    }
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
    if (ok) {
        invalidateLineCountCache(label);
        invalidateDayListCache();
        if (strcmp(label, currentSessionFile) == 0) {
            // Die gerade aktive Sitzungsdatei wurde geloescht - Dopplungs-
            // Liste zuruecksetzen, damit neue Sichtungen wieder korrekt in
            // die (beim naechsten Schreibvorgang neu angelegte) Datei
            // geloggt werden.
            seenCount = 0;
            loggedLineCount = 0;
        }
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
    loggedLineCount = 0;
    currentSessionFile[0] = 0;
    clearLineCountCache();
    invalidateDayListCache();
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

// Streaming-Suche nach genau einem Hex-Code in einer bereits geoeffneten
// Logbuch-CSV-Datei - fruehestmoeglicher Abbruch, sobald ein Treffer
// gefunden wurde (jeder Hex-Code kommt dank alreadySeen()/markSeen() beim
// Schreiben ohnehin hoechstens einmal pro Datei vor). Gleiches Zeilen-
// Parsing-Prinzip wie computeTopAircraft()/loadSeenFromCurrentFile() oben.
namespace {
    // Wie zuvor "fileContainsHex" (nur Treffer/kein Treffer), liefert bei
    // einem Treffer zusaetzlich die Sichtungsstunde (aus Spalte 0,
    // "YYYY-MM-DD HH:MM:SS"), die Flughoehe in ft (Spalte 7) sowie -
    // sofern vorhanden - den "Flugzeug-Steckbrief" (min_distance_km/
    // max_speed_kt, Spalten 8/9, siehe writeLogLine()) mit zurueck - fuer
    // Smart Aircraft Recognition/den Steckbrief (countPreviousSightings())
    // im SELBEN Scan-Durchlauf statt eines zweiten, separaten SD-Scans
    // fuer dasselbe Flugzeug.
    //
    // WICHTIG: positionsbasiert (Komma-Index), NICHT mehr ueber das
    // LETZTE Komma - seit min_distance_km/max_speed_kt als neue Spalten 8/9
    // dazugekommen sind, waere altitude_ft (Spalte 7) sonst falsch
    // ausgelesen. Alte Logbuch-Dateien ohne diese beiden Spalten (Zeile
    // endet nach altitude_ft) liefern outHasProfile=false, statt einen
    // falschen/geratenen Wert zu erfinden - "fehlende Spalte" bedeutet
    // "kein Wert bekannt", nicht 0.
    bool fileFindHexRow(File& f, const char* hex, uint8_t& outHour, uint8_t& outMinute, int32_t& outAltitudeFt,
                         bool& outHasProfile, float& outMinDistanceKm, float& outMaxSpeedKt) {
        constexpr size_t BUF_SIZE = 512;
        static uint8_t buf[BUF_SIZE];
        char lineBuf[128];
        size_t lineLen = 0;
        bool firstLine = true;
        bool found = false;
        uint32_t blocksRead = 0;
        size_t hexQueryLen = strlen(hex);

        auto processLine = [&]() {
            if (found || lineLen == 0) return;
            if (firstLine) { firstLine = false; return; } // Header-Zeile
            lineBuf[lineLen] = 0;
            char* p1 = strchr(lineBuf, ',');
            if (!p1) return;
            char* p2 = strchr(p1 + 1, ',');
            size_t hexLen = p2 ? (size_t)(p2 - (p1 + 1)) : strlen(p1 + 1);
            if (hexLen == hexQueryLen && strncmp(p1 + 1, hex, hexLen) == 0) {
                found = true;
                // Spalte 0 (Zeitstempel) ist "YYYY-MM-DD HH:MM:SS" - Stunde
                // steht immer an Zeichen 11-12, Minute an 14-15,
                // unabhaengig vom Rest (fuer "LOCAL OVERFLIGHTS"/"LAST"-
                // Anzeige im Steckbrief, Alex' Wunsch - selbe Zeile wie
                // outHour, kein zweiter Scan noetig).
                if ((size_t)(p1 - lineBuf) >= 16 && isdigit((unsigned char)lineBuf[11]) && isdigit((unsigned char)lineBuf[12])) {
                    outHour = (uint8_t)((lineBuf[11] - '0') * 10 + (lineBuf[12] - '0'));
                } else {
                    outHour = 0;
                }
                if ((size_t)(p1 - lineBuf) >= 16 && isdigit((unsigned char)lineBuf[14]) && isdigit((unsigned char)lineBuf[15])) {
                    outMinute = (uint8_t)((lineBuf[14] - '0') * 10 + (lineBuf[15] - '0'));
                } else {
                    outMinute = 0;
                }
                outAltitudeFt = 0;
                outHasProfile = false;
                char* p3 = p2 ? strchr(p2 + 1, ',') : nullptr;
                char* p4 = p3 ? strchr(p3 + 1, ',') : nullptr;
                char* p5 = p4 ? strchr(p4 + 1, ',') : nullptr;
                char* p6 = p5 ? strchr(p5 + 1, ',') : nullptr; // Spalte 6 (distance_km) endet hier, Spalte 7 (altitude_ft) beginnt
                if (p6) {
                    outAltitudeFt = (int32_t)atol(p6 + 1);
                    char* p7 = strchr(p6 + 1, ','); // Spalte 7 endet hier, Spalte 8 (min_distance_km) beginnt - nur bei neueren Zeilen vorhanden
                    if (p7) {
                        char* p8 = strchr(p7 + 1, ','); // Spalte 8 endet hier, Spalte 9 (max_speed_kt) beginnt
                        if (p8) {
                            outMinDistanceKm = (float)atof(p7 + 1);
                            outMaxSpeedKt = (float)atof(p8 + 1);
                            outHasProfile = true;
                        }
                    }
                }
            }
        };

        while (f.available() && !found) {
            size_t n = f.read(buf, BUF_SIZE);
            for (size_t i = 0; i < n && !found; i++) {
                char c = (char)buf[i];
                if (c == '\n' || c == '\r') {
                    if (lineLen > 0) processLine();
                    lineLen = 0;
                } else if (lineLen < sizeof(lineBuf) - 1) {
                    lineBuf[lineLen++] = c;
                }
            }
            blocksRead++;
            if (blocksRead % 4 == 0) delay(1);
        }
        return found;
    }
}

PreviousSighting countPreviousSightings(const char* hex) {
    PreviousSighting result;
    if (!hex || !hex[0]) return result;

    SdMutex::Guard guard;

    time_t now = time(nullptr);
    bool timeKnown = now > 8 * 3600 * 2;
    char todayStr[11] = {0};
    if (timeKnown) formatDateFromEpoch((uint32_t)now, todayStr, sizeof(todayStr));

    File dir = SD.open(Config::SD_LOG_DIR);
    if (!dir || !dir.isDirectory()) return result;

    // Gleicher Deckel wie MAX_RAW_SCAN in listDaySummaries() - verhindert
    // eine unbegrenzt lange Hintergrund-Aufgabe bei einem sehr lange
    // genutzten Geraet mit vielen angesammelten Dateien.
    constexpr uint8_t MAX_FILES_SCANNED = 90;
    uint8_t filesScanned = 0;
    // SD.openNextFile() liefert keine garantierte Sortierung - "zuletzt
    // gesehen" wird deshalb ueber einen fortlaufenden String-Vergleich
    // ermittelt (funktioniert dank "YYYY-MM-DD"-Format lexikographisch
    // korrekt wie ein Datumsvergleich).
    char latestDate[11] = {0};
    uint8_t latestHour = 0, latestMinute = 0;

    // "Smart Aircraft Recognition" - Zeit-/Hoehen-Spanne ueber alle
    // Treffer, im selben Durchlauf wie count/lastDate gesammelt (siehe
    // fileFindHexRow() oben). MIN_SIGHTINGS_FOR_PATTERN = 3 (Alex' Vorgabe:
    // weniger Datenpunkte sind statistisch nicht aussagekraeftig).
    constexpr uint16_t MIN_SIGHTINGS_FOR_PATTERN = 3;
    uint8_t minHour = 255, maxHour = 0;
    int32_t minAlt = INT32_MAX, maxAlt = INT32_MIN;

    // "Flugzeug-Steckbrief" (siehe PreviousSighting::hasProfile) - ueber
    // alle Treffer MIT Profildaten hinweg (aeltere Zeilen ohne die beiden
    // neuen Spalten liefern hasProfile=false und fliessen hier nicht ein).
    bool anyProfileFound = false;
    float minDistOverall = 0;
    float maxSpeedOverall = 0;

    File entry = dir.openNextFile();
    while (entry && filesScanned < MAX_FILES_SCANNED) {
        if (!entry.isDirectory()) {
            String name = String(entry.name());
            if (name.endsWith(".csv")) {
                filesScanned++;

                String dateOnly = name.substring(0, name.length() - 4);
                int slashIdx = dateOnly.lastIndexOf('/');
                if (slashIdx >= 0) dateOnly = dateOnly.substring(slashIdx + 1);
                // Das Kalenderdatum ist immer das feste "YYYY-MM-DD"-
                // Praefix, auch bei mehreren Sitzungen desselben Tages
                // ("YYYY-MM-DD_2" usw., siehe resolveSessionFilename()).
                String dayKey = dateOnly.length() >= 10 ? dateOnly.substring(0, 10) : dateOnly;
                bool isToday = timeKnown && dayKey == String(todayStr);

                uint8_t rowHour = 0, rowMinute = 0;
                int32_t rowAltitudeFt = 0;
                bool rowHasProfile = false;
                float rowMinDist = 0, rowMaxSpeed = 0;
                if (!isToday && fileFindHexRow(entry, hex, rowHour, rowMinute, rowAltitudeFt,
                                                rowHasProfile, rowMinDist, rowMaxSpeed)) {
                    result.count++;
                    if (strcmp(dayKey.c_str(), latestDate) > 0) {
                        strncpy(latestDate, dayKey.c_str(), sizeof(latestDate) - 1);
                        latestDate[sizeof(latestDate) - 1] = 0;
                        latestHour = rowHour;
                        latestMinute = rowMinute;
                    }
                    if (rowHour < minHour) minHour = rowHour;
                    if (rowHour > maxHour) maxHour = rowHour;
                    if (rowAltitudeFt < minAlt) minAlt = rowAltitudeFt;
                    if (rowAltitudeFt > maxAlt) maxAlt = rowAltitudeFt;
                    if (rowHasProfile) {
                        if (!anyProfileFound || rowMinDist < minDistOverall) minDistOverall = rowMinDist;
                        if (!anyProfileFound || rowMaxSpeed > maxSpeedOverall) maxSpeedOverall = rowMaxSpeed;
                        anyProfileFound = true;
                    }
                }
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    result.found = result.count > 0;
    if (result.found) {
        strncpy(result.lastDate, latestDate, sizeof(result.lastDate) - 1);
        result.lastDate[sizeof(result.lastDate) - 1] = 0;
        result.lastHour = latestHour;
        result.lastMinute = latestMinute;
    }
    if (result.count >= MIN_SIGHTINGS_FOR_PATTERN) {
        result.hasPattern = true;
        result.minHour = minHour;
        result.maxHour = maxHour;
        // Auf 1000ft gerundet (Alex' Vorgabe) - min abwaerts, max aufwaerts,
        // damit die tatsaechlichen Werte immer innerhalb der angezeigten
        // Spanne liegen.
        result.minAltitudeFt = (minAlt / 1000) * 1000;
        result.maxAltitudeFt = ((maxAlt + 999) / 1000) * 1000;
    }
    if (anyProfileFound) {
        result.hasProfile = true;
        result.minDistanceKm = minDistOverall;
        result.maxSpeedKt = maxSpeedOverall;
    }
    return result;
}

void updatePeakTraffic(uint8_t currentCount) {
    time_t now = time(nullptr);
    if (now <= 8 * 3600 * 2) return; // Uhrzeit noch nicht synchronisiert - naechster Zyklus holt es nach

    char todayStr[11];
    formatDateFromEpoch((uint32_t)now, todayStr, sizeof(todayStr));

    if (SettingsStore::peakTrafficDate() != String(todayStr)) {
        // Tageswechsel (oder allererster Lauf ueberhaupt) - Hoechstwert
        // zuruecksetzen, BEVOR der aktuelle Wert unten einsortiert wird.
        SettingsStore::setPeakTrafficDate(todayStr);
        SettingsStore::setPeakTrafficCount(0);
        SettingsStore::setPeakTrafficEpoch(0);
    }

    if (currentCount > SettingsStore::peakTrafficCount()) {
        SettingsStore::setPeakTrafficCount(currentCount);
        SettingsStore::setPeakTrafficEpoch((uint32_t)now);
    }
}

PeakTraffic todayPeakTraffic() {
    PeakTraffic result;

    time_t now = time(nullptr);
    if (now <= 8 * 3600 * 2) return result; // Uhrzeit unbekannt - kein verlaesslicher Tagesvergleich moeglich

    char todayStr[11];
    formatDateFromEpoch((uint32_t)now, todayStr, sizeof(todayStr));
    // Nur einen Wert zeigen, der auch tatsaechlich zu HEUTE gehoert (siehe
    // Kommentar bei flight_logbook.h::todayPeakTraffic()) - sonst koennte
    // kurz nach dem Booten (bevor updatePeakTraffic() den Tageswechsel
    // selbst erkannt hat) faelschlich noch der Wert von gestern erscheinen.
    if (SettingsStore::peakTrafficDate() != String(todayStr)) return result;

    result.count = SettingsStore::peakTrafficCount();
    uint32_t epoch = SettingsStore::peakTrafficEpoch();
    if (epoch > 0) {
        time_t t = (time_t)epoch;
        struct tm tmv;
        localtime_r(&t, &tmv);
        snprintf(result.timeStr, sizeof(result.timeStr), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
        result.hasTime = true;
    }
    return result;
}

}
