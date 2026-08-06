#pragma once
#include <Arduino.h>

namespace FlightLogbook {

    void init();
    void update();

    uint16_t todayCount();

    struct TopAltitude {
        bool found = false;
        char callsign[9] = {0};
        int32_t altitudeFt = 0;
    };

    // Sucht in der heutigen Logbuch-Datei den Eintrag mit der hoechsten
    // geloggten Flughoehe (jeweils die Hoehe BEIM ERSTEN Sichten, nicht der
    // aktuelle Wert) und gibt dessen Rufzeichen + Hoehe zurueck. found=false,
    // wenn heute noch nichts geloggt wurde oder die Datei fehlt.
    TopAltitude todayMaxAltitude();

    void computeAllTimeStats(uint32_t& totalAircraft, uint16_t& totalDays);

    struct DayEntry {
        char date[11] = {0};
        uint32_t count = 0;
    };

    uint8_t listDays(DayEntry* out, uint8_t maxEntries);

    // Loescht ALLE Logbuch-CSV-Dateien auf der SD-Karte unwiderruflich und
    // setzt die "heute schon gesehen"-Liste zurueck. Fuer den Reset-Button
    // im Statistik-Bildschirm gedacht.
    void resetAllData();
}