#pragma once
#include <Arduino.h>

// Berechnet Sonnenauf-/untergang (lokale Zeit) fuer einen gegebenen Ort und
// Tag - Grundlage fuer die standortbasierte Nachtdimmung (siehe main.cpp::
// isNightDimHours()), die das vorherige feste 22:00-06:00-Fenster ersetzt.
namespace SunTimes {

    struct Result {
        bool valid = false;       // false = Standort noch unbekannt (lat/lon == 0,0)
        bool alwaysDay = false;   // Polartag: Sonne geht heute an diesem Ort nicht unter
        bool alwaysNight = false; // Polarnacht: Sonne geht heute an diesem Ort nicht auf
        float sunriseHour = 0;    // Lokale Dezimalstunde (0-24), nur gueltig wenn weder
        float sunsetHour = 0;     // alwaysDay noch alwaysNight gesetzt ist.
    };

    // year/month/day sind lokale Kalenderdatumswerte (z.B. aus localtime_r()),
    // utcOffsetSeconds die bekannte Zeitzonenverschiebung (siehe
    // LocationManager::utcOffsetSeconds()). Nutzt den klassischen "Almanac"-
    // Sonnenauf-/untergangsalgorithmus (Sonnenzenit 90.833 Grad, beruecksichtigt
    // atmosphaerische Refraktion) - Genauigkeit liegt typischerweise im Bereich
    // weniger Minuten, fuer eine sanfte Backlight-Dimmung mehr als ausreichend.
    Result compute(double lat, double lon, int year, int month, int day, int32_t utcOffsetSeconds);
}
