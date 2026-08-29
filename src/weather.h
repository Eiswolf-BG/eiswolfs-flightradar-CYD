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

    // Windrichtung in Grad (meteorologische Konvention: Richtung, AUS der
    // der Wind weht, 0=Nord, im Uhrzeigersinn) - Teil derselben Open-Meteo-
    // "current_weather"-Antwort wie current() oben (fetchNow() in
    // weather.cpp). Aktuell von keinem Aufrufer genutzt (ein testweise
    // gebauter Regenfront-Layer auf dem Radar wurde wieder entfernt, da er
    // keinen Mehrwert zum bestehenden Header-Wetter-Icon brachte) - bewusst
    // als Rohdaten-Zugriff fuer spaetere Zwecke stehen gelassen. -1 = noch
    // keine erfolgreiche Abfrage. Wie current() nur vom NetTask
    // geschrieben, vom UI-Thread gelesen - kein Lock noetig.
    float currentWindDirectionDeg();

    // Roher METAR-Text (Flugwetterbericht) fuer den naechstgelegenen
    // Flughafen (per AirportLookup::findNearest() zum aktuell aktiven
    // Standort ermittelt) - ergaenzt die einfache Icon-Wetteranzeige um die
    // "echten", von Piloten genutzten Rohdaten. Abgefragt ueber die
    // kostenlose, anmeldefreie aviationweather.gov-Daten-API (US National
    // Weather Service), im selben Aufruf/Intervall wie das Icon-Wetter.
    struct Metar {
        bool available = false;
        char icao[5] = {0};
        char raw[128] = {0};
    };

    // Wie current() nur vom NetTask geschrieben, vom UI-Thread gelesen -
    // gleiche Begruendung, kein Lock noetig (ein kurzfristig veraltetes/
    // theoretisch "zerrissenes" Lesen des raw[]-Texts waere rein kosmetisch
    // und korrigiert sich spaetestens beim naechsten Abfrage-Intervall von
    // selbst).
    Metar currentMetar();

    // Kurzfristige Kurzvorhersage (ein einzelner stuendlicher Datenpunkt
    // "jetzt + hoursAhead Stunden", siehe fetchNow() in weather.cpp) fuer
    // den Wetter-Info-Screen (main.cpp::showWeatherInfo()) - bewusst nur
    // EIN Zeitpunkt statt einer mehrstuendigen Vorhersagereihe, damit das
    // kleine Display uebersichtlich bleibt (siehe Absprache mit Alex).
    // Benoetigt eine bereits per NTP synchronisierte Uhrzeit, um den
    // gewuenschten zukuenftigen Zeitpunkt ueberhaupt berechnen zu koennen -
    // ohne das bleibt available=false, genau wie bei fehlendem Standort.
    struct Forecast {
        bool available = false;
        float temperatureC = 0;
        Condition condition = Condition::Unknown;
        uint8_t hoursAhead = 3;
    };

    // Wie current()/currentMetar() nur vom NetTask geschrieben, vom
    // UI-Thread gelesen - gleiche Begruendung, kein Lock noetig.
    Forecast currentForecast();
}
