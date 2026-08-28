#pragma once
#include <Arduino.h>

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
}
