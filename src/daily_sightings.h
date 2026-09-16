#pragma once
#include "aircraft.h"

// Taeglich (lokale Kalenderzeit) zuruecksetzender Sichtungszaehler pro
// Flugzeug fuers Detail-Panel (Alex' Wunsch) - unterscheidet drei
// Zustaende: heute zum ersten Mal gesehen, heute schon mehrfach gesehen
// UND aktuell durchgehend sichtbar, oder gerade nach einer Pause
// zurueckgekehrt. Rein RAM-lokal (wie SessionStats), KEINE Persistenz
// ueber einen Neustart hinweg - ein Neustart resettet die Zaehlung
// einfach wie die Sitzungsstatistik auch.
namespace DailySightings {
    enum class State : uint8_t { Unknown = 0, NewToday, SeenToday, Returning };

    struct Info {
        bool available = false;
        State state = State::Unknown;
        uint16_t count = 0;
        // Nur gueltig, wenn state == Returning - Pause in Millisekunden,
        // die zur aktuellen Rueckkehr gefuehrt hat.
        uint32_t gapMs = 0;
    };

    // Von aircraft_table.cpp::postFetchUpdate() JEDEN Zyklus fuer jedes
    // aktuell gueltige Flugzeug aufgerufen - analog zu SessionStats::
    // record().
    void record(const Aircraft& a);

    // Von radar_screen.cpp::drawDetailPanel() abgefragt, um den aktuellen
    // Zustand des ausgewaehlten Flugzeugs anzuzeigen.
    Info get(const char* hex);
}
