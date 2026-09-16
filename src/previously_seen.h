#pragma once
#include <Arduino.h>

// Rein aus den bestehenden Logbuch-CSV-Dateien abgeleitet (siehe
// flight_logbook.h::countPreviousSightings()) - wie oft ein Flugzeug (per
// Hex-Code) bereits an FRUEHEREN Tagen (nicht heute, siehe dortiger
// Kommentar) gesehen wurde, plus das Datum der letzten dieser frueheren
// Sichtungen. Laeuft asynchron im Hintergrund (Core 0, NetTask), exakt
// nach demselben Anfrage-/Abhol-Muster wie aircraft_details.h
// (request()/get()/update()) - der eigentliche SD-Kartenscan kann bei
// vielen ueber Wochen/Monate angesammelten Logbuch-Dateien spuerbar
// dauern (siehe Analyse mit Alex: SD.open()/close()-Overhead pro Datei
// dominiert) und darf deshalb nicht blockierend im Touch-Handler laufen.
namespace PreviouslySeen {

    struct Info {
        bool loading = false;
        bool found = false;
        uint16_t count = 0;
        char lastDate[11] = {0}; // "YYYY-MM-DD", nur gueltig wenn found true
        // Uhrzeit derselben letzten Sichtung (siehe FlightLogbook::
        // PreviousSighting::lastHour/lastMinute) - nur gueltig wenn found
        // true.
        uint8_t lastHour = 0;
        uint8_t lastMinute = 0;

        // "Smart Aircraft Recognition" - siehe FlightLogbook::
        // PreviousSighting im selben Scan-Durchlauf mit ermittelt, nur
        // gueltig wenn hasPattern true ist (mind. 3 fruehere Sichtungen).
        bool hasPattern = false;
        uint8_t minHour = 0;
        uint8_t maxHour = 0;
        int32_t minAltitudeFt = 0;
        int32_t maxAltitudeFt = 0;

        // "Flugzeug-Steckbrief" - siehe FlightLogbook::PreviousSighting,
        // im selben Scan-Durchlauf mit ermittelt, nur gueltig wenn
        // hasProfile true ist (mind. eine fruehere Logbuch-Zeile mit den
        // beiden neuen Spalten - aeltere Dateien ohne sie liefern hier
        // bewusst nichts, statt einen falschen Wert zu erfinden).
        bool hasProfile = false;
        float minDistanceKm = 0;
        float maxSpeedKt = 0;
    };

    // Core 1 (Touch-Auswahl, siehe radar_screen.cpp::handleTap()/
    // selectAircraft()): merkt einen Scan fuer dieses Flugzeug vor, falls
    // nicht schon geschehen oder bereits im Gange.
    void request(const char* hex);

    // Core 1: aktuellen (evtl. noch ladenden) Zustand fuer 'hex' abholen.
    // Info::loading bleibt false UND Info::found bleibt false, wenn fuer
    // dieses hex weder ein Ergebnis vorliegt noch ein Scan angefordert
    // wurde - das Detail-Panel zeigt dann bewusst gar keine Zeile.
    Info get(const char* hex);

    // Core 0 (NetTask), periodisch aufgerufen: fuehrt einen vorgemerkten
    // Scan aus (SD-Zugriff, kann bei vielen Dateien einen Moment dauern -
    // laeuft im Hintergrund, verzoegert bestenfalls den naechsten ADS-B-
    // Abruf etwas, exakt wie AircraftDetails::update() das fuer seine
    // Netzwerk-Anfragen bereits akzeptiert).
    void update();
}
