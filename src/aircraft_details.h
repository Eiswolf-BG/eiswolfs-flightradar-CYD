#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>

// Additional aircraft details (model) that are NOT part of the ADS-B signal
// and get looked up via the free hexdb.io community database by hex code -
// only for the currently selected aircraft (not for all of them, to keep
// network load low).
namespace AircraftDetails {

    struct Info {
        bool loading = false;
        char model[40] = {0}; // e.g. "Airbus A320 216", empty if unknown

        // Departure/destination airport (ICAO code, e.g. "KIAH"/"EDDF") of
        // the current flight route, looked up by callsign via a chain of
        // three free sources (VRS standing-data mirror, hexdb.io, then
        // adsbdb.com as a last fallback - see aircraft_details.cpp) for
        // better coverage than any single source alone. Empty if no
        // callsign is known or no route was found in any of the three.
        char routeOrigin[8] = {0};
        char routeDest[8] = {0};

        // Same route, but as IATA codes (e.g. "IAH"/"FRA") - for the
        // optional IATA display mode (Menue > Land/Region > Einheiten,
        // SettingsStore::useIataAirportCodes()). Two of the three route
        // sources above already carry IATA codes in their response
        // alongside the ICAO ones, so no extra API call is needed - see
        // aircraft_details.cpp. Empty if the source that answered didn't
        // provide one (e.g. hexdb.io's route endpoint never does) or one
        // of the two airports genuinely has no IATA code - the UI falls
        // back to routeOrigin/routeDest (ICAO) in that case.
        char routeOriginIata[4] = {0};
        char routeDestIata[4] = {0};
    };

    // Called from Core 1 (touch selection): marks that details should be
    // fetched for this aircraft (if not already done). callsign may be
    // empty (no route lookup is attempted in that case).
    void request(const char* hex, const char* callsign);

    // Called from Core 1 to get the current (possibly still incomplete)
    // state for 'hex'.
    Info get(const char* hex);

    // Called periodically from NetTask (Core 0): performs a pending request
    // (blocking HTTPS call, but that's fine - runs in the background and
    // only briefly delays the next ADS-B poll).
    void update();

    // Die Flugrouten-Suche (Start-/Zielflughafen) - dieselbe Drei-Quellen-
    // Fallback-Kette (VRS-Standing-Data-Mirror -> adsbdb.com -> hexdb.io,
    // siehe aircraft_details.cpp fuer die ausfuehrliche Herleitung/
    // Reihenfolge-Begruendung) wie von update() oben genutzt - hier als
    // eigenstaendige, wiederverwendbare Funktion herausgezogen, damit sowohl
    // update() ALS AUCH route_watchlist.cpp dieselbe, bereits bewaehrte
    // Implementierung (inkl. Fehlerbehandlung/Timeouts) nutzen, statt sie
    // zweimal zu pflegen ODER doppelte Netzwerkanfragen fuer ICAO- und IATA-
    // Codes separat auszuloesen (beide stecken bereits in derselben
    // Quellen-Antwort). Die vier IATA-Parameter sind optional (nullptr/0 =
    // nicht gebraucht, siehe route_watchlist.cpp, das nur ICAO braucht).
    // Blockierender HTTPS-Aufruf - NUR aus einem Core-0/Hintergrund-Kontext
    // aufrufen, niemals vom UI-Thread (Core 1). 'client' wird vom Aufrufer
    // gestellt (kein eigener Verbindungsaufbau hier), 'callsign' darf
    // Kleinbuchstaben/Leerzeichen enthalten (wird intern normalisiert).
    // Gibt true zurueck, wenn ein ICAO-Origin UND -Dest gefunden wurden.
    bool fetchRoute(WiFiClientSecure& client, const String& callsign,
                     char* origin, size_t originSize, char* dest, size_t destSize,
                     char* originIata = nullptr, size_t originIataSize = 0,
                     char* destIata = nullptr, size_t destIataSize = 0);
}
