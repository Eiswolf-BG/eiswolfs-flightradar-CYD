#include "route_watchlist.h"
#include "config.h"
#include "sd_mutex.h"
#include "sd_storage.h"
#include "settings_store.h"
#include "aircraft_table.h"
#include "aircraft_details.h"
#include "radar_screen.h"
#include <SD.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>
#include <cctype>

namespace RouteWatchlist {

namespace {
    constexpr const char* WATCHED_FILE = "/Flightradar_cyd/watched_routes.txt";

    struct Entry {
        char origin[5] = {0};
        char dest[5] = {0};
    };

    Entry watched[MAX_WATCHED];
    uint8_t watchedCount = 0;

    // Schuetzt watched[]/watchedCount - gleiches Muster wie bei den drei
    // bestehenden Watchlists (z.B. TypeWatchlist::mutex).
    SemaphoreHandle_t mutex = nullptr;

    void ensureMutex() {
        if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    }

    // Bis zu 4 Zeichen, Grossbuchstaben, fuehrende Leerzeichen ueberspringt -
    // gleiches Prinzip wie TypeWatchlist::normalize(). Leerer String bleibt
    // leer (= Feld nicht gesetzt), kein Fehlerfall.
    void normalize(const char* code, char* out) {
        int j = 0;
        int i = 0;
        if (!code) { out[0] = 0; return; }
        while (code[i] == ' ') i++;
        for (; j < 4 && code[i] && code[i] != ' '; i++, j++) {
            out[j] = (char)toupper((unsigned char)code[i]);
        }
        out[j] = '\0';
    }

    void saveToSd() {
        if (!SdStorage::isMounted()) return;
        SdMutex::Guard guard;

        File f = SD.open(WATCHED_FILE, FILE_WRITE);
        if (!f) return;
        for (uint8_t i = 0; i < watchedCount; i++) {
            f.print(watched[i].origin);
            f.print(',');
            f.println(watched[i].dest);
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
            int comma = line.indexOf(',');
            if (comma < 0) continue; // beschaedigte Zeile ueberspringen
            String origin = line.substring(0, comma);
            String dest = line.substring(comma + 1);
            if (origin.length() == 0 && dest.length() == 0) continue; // ungueltig
            strncpy(watched[watchedCount].origin, origin.c_str(), 4);
            watched[watchedCount].origin[4] = 0;
            strncpy(watched[watchedCount].dest, dest.c_str(), 4);
            watched[watchedCount].dest[4] = 0;
            watchedCount++;
        }
        f.close();
    }

    // Throttle fuer pollBackground() (Alex' Wunsch: API-Last im Blick
    // behalten) - verhindert, dass unmittelbar nach dem Einschalten des
    // Schalters (viele Flugzeuge gleichzeitig ohne routeLookupDone) sofort
    // Schlag auf Schlag Lookups ausgeloest werden, ohne dem naechsten
    // ADS-B-Abruf ueberhaupt Luft zu lassen. Kein Problem fuer den
    // Normalbetrieb (jedes Flugzeug wird ohnehin nur EINMAL nachgefragt,
    // siehe aircraft.h::routeLookupDone), aber ein sanfter Mindestabstand
    // zwischen zwei Lookups ist trotzdem sinnvoll fuer den Fall vieler
    // gleichzeitig neu erschienener Flugzeuge (z.B. beim Einschalten mit
    // bereits vollem Himmel).
    constexpr uint32_t MIN_GAP_MS = 3000;
    uint32_t lastLookupMs = 0;
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

String originAt(uint8_t index) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    String out = (index >= watchedCount) ? String() : String(watched[index].origin);
    xSemaphoreGive(mutex);
    return out;
}

String destAt(uint8_t index) {
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    String out = (index >= watchedCount) ? String() : String(watched[index].dest);
    xSemaphoreGive(mutex);
    return out;
}

bool addWatched(const char* origin, const char* dest) {
    Entry e;
    normalize(origin, e.origin);
    normalize(dest, e.dest);
    if (!e.origin[0] && !e.dest[0]) return false; // mindestens eines noetig

    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool ok = true;
    bool alreadyPresent = false;
    if (watchedCount >= MAX_WATCHED) {
        ok = false;
    } else {
        for (uint8_t j = 0; j < watchedCount; j++) {
            if (strcmp(watched[j].origin, e.origin) == 0 && strcmp(watched[j].dest, e.dest) == 0) {
                alreadyPresent = true;
                break;
            }
        }
        if (!alreadyPresent) {
            watched[watchedCount] = e;
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
            watched[i] = watched[i + 1];
        }
        watchedCount--;
        watched[watchedCount] = Entry{};
    }
    xSemaphoreGive(mutex);
    if (changed) saveToSd();
}

bool isWatched(const char* aircraftOrigin, const char* aircraftDest) {
    if ((!aircraftOrigin || !aircraftOrigin[0]) && (!aircraftDest || !aircraftDest[0])) return false;

    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool found = false;
    for (uint8_t i = 0; i < watchedCount; i++) {
        const Entry& e = watched[i];
        bool originMatches = !e.origin[0] || (aircraftOrigin && strcmp(e.origin, aircraftOrigin) == 0);
        bool destMatches = !e.dest[0] || (aircraftDest && strcmp(e.dest, aircraftDest) == 0);
        // Mindestens EIN Feld muss im Eintrag tatsaechlich gesetzt sein
        // (sonst waere ein komplett leerer Eintrag ein Treffer auf alles) -
        // addWatched() verhindert das bereits beim Anlegen, hier zur
        // Sicherheit nochmal geprueft.
        if ((e.origin[0] || e.dest[0]) && originMatches && destMatches) {
            found = true;
            break;
        }
    }
    xSemaphoreGive(mutex);
    return found;
}

void pollBackground() {
    if (!SettingsStore::routeWatchlistAlertEnabled()) return;
    if (count() == 0) return; // keine Eintraege -> nichts zu ermitteln

    uint32_t now = millis();
    if (lastLookupMs != 0 && now - lastLookupMs < MIN_GAP_MS) return;

    // Ersten sichtbaren Kandidaten OHNE bereits abgeschlossenen Lookup
    // finden - Tabelle dafuer nur kurz sperren (Hex/Callsign kopieren),
    // NICHT waehrend des anschliessenden blockierenden HTTPS-Aufrufs
    // gesperrt halten (gleiches Prinzip wie AircraftDetails::update()).
    char hex[7] = {0};
    char callsign[9] = {0};
    bool found = false;
    {
        AircraftTable::lock();
        Aircraft* table = AircraftTable::raw();
        for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
            if (!table[i].valid || !table[i].callsign[0] || table[i].routeLookupDone) continue;
            if (!RadarScreen::isAircraftCurrentlyVisible(table[i])) continue;
            strncpy(hex, table[i].hex, sizeof(hex) - 1);
            strncpy(callsign, table[i].callsign, sizeof(callsign) - 1);
            found = true;
            break;
        }
        AircraftTable::unlock();
    }
    if (!found) return;

    lastLookupMs = now;

    WiFiClientSecure client;
    client.setInsecure();
    char origin[5] = {0};
    char dest[5] = {0};
    // Dieselbe Drei-Quellen-Fallback-Kette wie das Detail-Panel (Alex'
    // Wunsch: "nicht neu erfinden") - nur ICAO gebraucht, IATA-Parameter
    // bleiben weg (Default nullptr/0, siehe aircraft_details.h).
    AircraftDetails::fetchRoute(client, callsign, origin, sizeof(origin), dest, sizeof(dest));

    AircraftTable::lock();
    Aircraft* table = AircraftTable::raw();
    for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
        if (strcmp(table[i].hex, hex) == 0) {
            strncpy(table[i].routeOrigin, origin, sizeof(table[i].routeOrigin) - 1);
            strncpy(table[i].routeDest, dest, sizeof(table[i].routeDest) - 1);
            table[i].routeLookupDone = true;
            break;
        }
    }
    AircraftTable::unlock();
}

}
