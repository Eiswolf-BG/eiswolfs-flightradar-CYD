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

    // True (und setzt sich dabei EINMALIG zurueck), wenn seit dem letzten
    // Abfragen die 24h-Sicherheitsabschaltung tatsaechlich gegriffen hat -
    // egal ob waehrend des laufenden Betriebs (checkAutoOff() greift live)
    // oder weil das Geraet laenger als 24h vom Strom getrennt war und der
    // gespeicherte Zeitstempel schon beim ersten Check nach dem Booten
    // (sobald NTP synchronisiert ist) abgelaufen ist - beide Faelle laufen
    // ueber denselben checkAutoOff()-Codepfad in flight_logbook.cpp, daher
    // hier bewusst EIN gemeinsames Flag statt zweier getrennter Meldewege.
    // Wird von Core 0 (NetTask, siehe enforceAutoOff()/update()) gesetzt,
    // von Core 1 (main.cpp::loop(), fuer den Hinweis-Screen) konsumiert -
    // gleiches Cross-Core-Flag-Muster wie RadarScreen::
    // consumeHeaderRedrawFlag().
    bool consumeAutoOffNotice();

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

    struct PreviousSighting {
        bool found = false;
        uint16_t count = 0;
        char lastDate[11] = {0}; // "YYYY-MM-DD", nur gueltig wenn found true

        // "Smart Aircraft Recognition" - einfache Zeitmuster ueber alle
        // fruehreren Sichtungen hinweg, IM SELBEN Scan-Durchlauf wie
        // count/lastDate oben ermittelt (kein zweiter SD-Scan). Nur
        // gueltig, wenn hasPattern true ist - das ist erst ab
        // MIN_SIGHTINGS_FOR_PATTERN (siehe flight_logbook.cpp) frueheren
        // Sichtungen der Fall, da ein Muster aus 1-2 Datenpunkten
        // statistisch nicht aussagekraeftig waere.
        bool hasPattern = false;
        uint8_t minHour = 0;
        uint8_t maxHour = 0;
        int32_t minAltitudeFt = 0;
        int32_t maxAltitudeFt = 0;

        // "Flugzeug-Steckbrief" - kleinste je geloggte Distanz bzw. hoechste
        // je geloggte Geschwindigkeit ueber ALLE frueheren Logbuch-Eintraege
        // dieses Flugzeugs (min_distance_km/max_speed_kt-Spalten, siehe
        // flight_logbook.cpp::writeLogLine()), im selben Scan-Durchlauf wie
        // count/lastDate/hasPattern oben ermittelt. Nur gueltig, wenn
        // hasProfile true ist - ALTE Logbuch-Dateien ohne diese beiden
        // Spalten liefern hier bewusst KEINEN Wert (nicht 0), gleiches
        // Prinzip wie firstSeenEpoch bei Aircraft.
        bool hasProfile = false;
        float minDistanceKm = 0;
        float maxSpeedKt = 0;
    };

    // Zaehlt, wie oft ein Flugzeug (per Hex-Code) bereits an FRUEHEREN
    // Tagen (also NICHT in der/den Datei(en) des heutigen Kalendertags,
    // egal ob durch die aktuelle Sitzung oder eine fruehere Sitzung
    // desselben Tages entstanden - siehe PreviouslySeen::request(), das
    // typischerweise genau in dem Moment aufgerufen wird, in dem das
    // Flugzeug per Antippen ausgewaehlt/gerade erst in die heutige Datei
    // eingetragen wird) in den Logbuch-Dateien vorkommt, sowie das Datum
    // der letzten dieser frueheren Sichtungen. Scannt bis zu 90 Dateien
    // (gleicher Deckel wie MAX_RAW_SCAN in listDaySummaries()), damit auch
    // bei einem sehr lange genutzten Geraet mit vielen angesammelten
    // Dateien keine unbegrenzt lange Aufgabe entsteht. BEWUSST NICHT
    // blockierend im Touch-Handler aufrufen (siehe Analyse mit Alex: der
    // dominante Kostenfaktor ist der SD.open()/close()-Overhead PRO Datei,
    // bei vielen angesammelten Dateien spuerbar) - siehe stattdessen
    // previously_seen.h fuer den asynchronen Anfrage-/Abhol-Mechanismus
    // (Core 0/NetTask), der diese Funktion tatsaechlich aufruft.
    PreviousSighting countPreviousSightings(const char* hex);

    // "Peak Traffic" - Tages-Hoechstwert gleichzeitig sichtbarer Flugzeuge
    // (AircraftTable::validCount()). Bewusst UNABHAENGIG vom Flugbuch-Ein/
    // Aus-Schalter (anders als die CSV-Aufzeichnung) - von net_task.cpp bei
    // JEDEM erfolgreichen ADS-B-Update aufgerufen. Erkennt einen
    // Tageswechsel genau wie ensureSessionFile() (persistiertes Datum vs.
    // tatsaechliches heutiges Datum vergleichen) und setzt den Hoechstwert
    // dann auf 0 zurueck, BEVOR der aktuelle Wert einsortiert wird. Ohne
    // NTP-synchronisierte Uhrzeit (kurz nach dem Booten) wird der Aufruf
    // uebersprungen, statt einen Tageswechsel anhand einer falschen
    // Zeitbasis zu erkennen/verpassen - der Wert wird spaetestens im
    // naechsten Zyklus nach der Synchronisierung korrekt nachgeholt.
    // Persistiert (siehe SettingsStore::peakTrafficCount() etc.), uebersteht
    // also einen Geraete-Neustart.
    void updatePeakTraffic(uint8_t currentCount);

    struct PeakTraffic {
        uint16_t count = 0;
        // Nur gueltig, wenn hasTime true ist - fehlt sie (Hoechstwert wurde
        // erreicht, bevor die Uhrzeit NTP-synchronisiert war), wird die
        // Uhrzeit in der Anzeige bewusst weggelassen statt eine falsche zu
        // zeigen (gleiches Fallback-Prinzip wie Aircraft::firstSeenEpoch).
        bool hasTime = false;
        char timeStr[6] = {0}; // "HH:MM"
    };

    // Liefert den aktuellen Tages-Hoechstwert - NUR wenn der gespeicherte
    // Wert tatsaechlich zum heutigen Kalendertag gehoert (sonst waere es
    // z.B. direkt nach dem Booten, bevor updatePeakTraffic() ueberhaupt
    // einmal gelaufen ist, faelschlich noch der Wert von gestern).
    // count==0 bedeutet "heute noch kein Hoechstwert ermittelt" - die
    // Anzeige (stats_history_screen.cpp) laesst die Zeile dann komplett weg.
    PeakTraffic todayPeakTraffic();

    // Verbleibende Sekunden bis zur 24h-Sicherheitsabschaltung greift (siehe
    // checkAutoOff()), fuer die Countdown-Anzeige im Menue (Flugbuch-Zeile).
    // Nutzt denselben gespeicherten Aktivierungs-Zeitstempel wie die
    // Abschaltung selbst - keine eigene Datenquelle. Liefert -1, wenn der
    // Countdown gerade nicht sinnvoll anzeigbar ist: Flugbuch aus, oder die
    // Uhrzeit noch nicht NTP-synchronisiert (dann waere "verbleibende Zeit"
    // nicht verlaesslich berechenbar) - der Aufrufer laesst die Zeile in
    // diesem Fall einfach weg.
    int32_t secondsUntilAutoOff();
}
