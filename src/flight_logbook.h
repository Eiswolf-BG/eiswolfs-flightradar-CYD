#pragma once
#include <Arduino.h>

namespace FlightLogbook {

    void init();
    void update();

    // Prueft/erzwingt NUR die 24h-Sicherheitsabschaltung (siehe
    // flight_logbook.cpp), OHNE neue Sichtungen zu loggen - dafuer wird
    // keine frische ADS-B-Abfrage benoetigt. Von net_task.cpp bei JEDEM
    // Schleifendurchlauf aufgerufen, unabhaengig vom Erfolg der letzten
    // ADS-B-Abfrage (anders als update(), das nur nach einer erfolgreichen
    // Abfrage laeuft und die 24h-Grenze deshalb allein nicht zuverlaessig
    // durchsetzen konnte).
    void enforceAutoOff();

    uint16_t todayCount();

    struct TopAltitude {
        bool found = false;
        char callsign[9] = {0};
        int32_t altitudeFt = 0;
    };

    // Sucht in der Datei der aktuellen Sitzung den Eintrag mit der hoechsten
    // geloggten Flughoehe (jeweils die Hoehe BEIM ERSTEN Sichten, nicht der
    // aktuelle Wert) und gibt dessen Rufzeichen + Hoehe zurueck. found=false,
    // wenn noch nichts geloggt wurde oder die Datei fehlt.
    TopAltitude todayMaxAltitude();

    void computeAllTimeStats(uint32_t& totalAircraft, uint16_t& totalDays);

    struct DayEntry {
        // Nicht mehr zwingend nur ein Kalenderdatum: bei mehrfachem
        // Ein-/Ausschalten am selben Tag bekommt jede Sitzung eine eigene
        // Datei mit Suffix (z.B. "2026-08-06_2") - siehe
        // resolveSessionFilename() in flight_logbook.cpp. Puffer
        // entsprechend groesser als ein reines "YYYY-MM-DD".
        char date[16] = {0};
        uint32_t count = 0;
    };

    // Eine Zeile pro Logbuch-DATEI (also ggf. mehrere pro Kalendertag, wenn
    // das Flugbuch mehrfach am selben Tag ein-/ausgeschaltet wurde). Fuer
    // den Logbuch-Dateien-Screen gedacht, wo jede Datei einzeln geloescht
    // werden kann.
    uint8_t listDays(DayEntry* out, uint8_t maxEntries);

    // Wie listDays(), fasst aber alle Sitzungs-Dateien desselben
    // Kalendertags zu einem Eintrag zusammen (Summe der Anzahl) - fuer den
    // 7-Tage-Verlauf im Statistik-Bildschirm, der weiterhin pro Tag statt
    // pro einzelner Sitzung zaehlen soll.
    uint8_t listDaySummaries(DayEntry* out, uint8_t maxEntries);

    // Loescht eine einzelne Logbuch-Datei (Label wie von listDays()
    // zurueckgegeben, ohne ".csv"). Fuer die einzelnen Loesch-Buttons im
    // Logbuch-Dateien-Screen gedacht. Wird gerade die aktive Sitzungsdatei
    // geloescht, faengt die Aufzeichnung sauber neu in derselben Datei an.
    bool deleteFile(const char* label);

    // Loescht ALLE Logbuch-CSV-Dateien auf der SD-Karte unwiderruflich und
    // setzt die "heute schon gesehen"-Liste zurueck. Fuer den Reset-Button
    // im Statistik-Bildschirm gedacht.
    void resetAllData();

    struct TopAircraft {
        // Kennzeichen (Registrierung), falls in mindestens einer Zeile
        // bekannt - sonst leer, dann zeigt der Top-Aircraft-Screen den
        // Hex-Code als Ersatz an.
        char reg[10] = {0};
        char hex[7] = {0};
        // Anzahl Logbuch-Zeilen mit diesem Hex-Code ueber ALLE Dateien
        // hinweg - da jedes Flugzeug pro Sitzung nur EINMAL geloggt wird
        // (siehe alreadySeen()/markSeen()), entspricht das der Anzahl
        // unterschiedlicher Sitzungen/Tage, an denen es gesehen wurde.
        uint32_t sightings = 0;
    };

    // Ermittelt die am haeufigsten geloggten Flugzeuge (nach Hex-Code
    // gezaehlt) ueber ALLE Logbuch-Dateien auf der SD-Karte hinweg - fuer
    // die "Meistgesehene Flugzeuge"-Rangliste (Statistik-Screen). Absteigend
    // sortiert, out[0] = am haeufigsten gesehen. Gibt die Anzahl gefuellter
    // Eintraege zurueck (<= maxEntries).
    uint8_t computeTopAircraft(TopAircraft* out, uint8_t maxEntries);
}
