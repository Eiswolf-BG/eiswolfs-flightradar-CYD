#pragma once
#include <Arduino.h>

// Sehr einfache Wetteranzeige (Icon im Header, dort wo frueher der
// Cam-Button war) - fragt periodisch die aktuelle Wetterlage fuer den
// gerade aktiven Standort ab (also inkl. aktivem Standort-Preset, siehe
// LocationManager::getHomeLocation() - wenn dort z.B. Mailand ausgewaehlt
// ist, zeigt das Icon das Wetter in Mailand). Nutzt die kostenlose
// Open-Meteo-API (kein API-Key noetig).
namespace Weather {
    enum class Condition {
        Unknown,       // noch keine erfolgreiche Abfrage - Icon zeigt nichts an
        Clear,
        PartlyCloudy,
        Cloudy,
        Rain,
        Snow,
        Thunderstorm
    };

    // Muss regelmaessig aus dem NetTask (Core 0) aufgerufen werden - kuemmert
    // sich intern um das Abfrage-Intervall (Config::WEATHER_FETCH_INTERVAL_MS)
    // und erkennt eine Standort-Aenderung (z.B. anderes aktives Preset), um
    // dann sofort neu abzufragen statt bis zum naechsten reguleren Intervall
    // zu warten.
    void update();

    // Aktuell bekannte Wetterlage, threadsicher genug fuer diesen Zweck (ein
    // einzelnes uint8-artiges Enum, das nur vom NetTask geschrieben und vom
    // UI-Thread gelesen wird - kein Lock noetig, ein kurzfristig veraltetes
    // Lesen ist unkritisch).
    Condition current();
}
