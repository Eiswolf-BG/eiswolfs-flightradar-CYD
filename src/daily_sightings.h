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
    // record(). Liefert true GENAU beim allerersten Aufruf fuer diesen Hex-
    // Code an diesem Kalendertag (neuer Entry wird angelegt) - SessionStats
    // nutzt das als "heute neu gesehen"-Signal (Alex' Auftrag, Session-
    // Highlights), OHNE dafuer ein zweites, eigenes Dedup-Set im RAM
    // vorzuhalten (haette beim ersten Versuch prompt wieder den bekannten
    // DRAM-Ueberlauf ausgeloest, siehe Kommentar oben zu MAX_SEEN).
    bool record(const Aircraft& a);

    // Von radar_screen.cpp::drawDetailPanel() abgefragt, um den aktuellen
    // Zustand des ausgewaehlten Flugzeugs anzuzeigen.
    Info get(const char* hex);

    // "Pass-by-Zaehler" (Alex' Auftrag) - welches Flugzeug heute am
    // haeufigsten erneut aufgetaucht ist (hoechster count-Wert ueber ALLE
    // heute gesehenen Flugzeuge, nicht nur das aktuell abgefragte). Nur der
    // Hex-Code wird gemerkt (kein Rufzeichen-Feld hier, um die Entry-
    // Struktur klein zu halten, siehe Kommentar zu MAX_SEEN in .cpp) - der
    // Aufrufer (session_stats_screen.cpp) loest das Rufzeichen bei Bedarf
    // selbst ueber die AircraftTable auf, mit Fallback auf den Hex-Code.
    struct TopReturning {
        bool available = false;
        char hex[7] = {0};
        uint16_t count = 0;
    };
    TopReturning topReturning();
}
