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

    // Intensitaetsabstufung fuer den animierten Regen-Effekt (Radarscreen,
    // Ruhebildschirm, WebUI-Live-Radar - alle drei lesen von hier, damit sie
    // konsistent auf dieselbe Intensitaet reagieren, siehe currentRainIntensity()
    // unten). Aus dem ohnehin bereits abgefragten Open-Meteo-"weathercode"
    // (WMO-Code) abgeleitet, keine zusaetzliche API-Anfrage noetig - siehe
    // intensityFromWmoCode() in weather.cpp fuer die genaue Code-Zuordnung.
    // None = kein Regen-Code (bzw. current()!=Rain/Thunderstorm).
    enum class RainIntensity {
        None,
        Light,
        Moderate,
        Heavy
    };

    // Windrichtung in Grad (meteorologische Konvention: Richtung, AUS der
    // der Wind weht, 0=Nord, im Uhrzeigersinn) - Teil derselben Open-Meteo-
    // "current_weather"-Antwort wie current() oben (fetchNow() in
    // weather.cpp). Steuert den Neigungswinkel des Regen-/Schnee-Effekts
    // auf Radarscreen, Ruhebildschirm und WebUI (siehe radar_screen.cpp/
    // web_export_server.cpp). -1 = noch keine erfolgreiche Abfrage. Wie
    // current() nur vom NetTask geschrieben, vom UI-Thread gelesen - kein
    // Lock noetig.
    float currentWindDirectionDeg();

    // Windgeschwindigkeit in km/h - Teil derselben current_weather-Antwort
    // wie currentWindDirectionDeg() oben (Open-Meteo liefert "windspeed"
    // dort standardmaessig mit), bisher aber nicht ausgelesen - jetzt fuer
    // den neuen Wettervorschau-Screen (radar_screen.cpp, Antippen der 3h-
    // Vorschau-Ecke) gebraucht. -1 = noch keine erfolgreiche Abfrage.
    float currentWindSpeedKmh();

    // Aktuelle Regenintensitaet (siehe RainIntensity oben) - wie current()
    // nur vom NetTask geschrieben, vom UI-Thread gelesen, kein Lock noetig.
    // Liefert RainIntensity::None, solange current() nicht Rain/Thunderstorm
    // ist (bzw. vor der ersten erfolgreichen Abfrage).
    RainIntensity currentRainIntensity();

    // Gleiche Idee wie currentRainIntensity(), nur fuer Schnee (Ruhebildschirm-
    // Schneeeffekt, siehe main.cpp::ScreensaverSnow) - bewusst derselbe
    // RainIntensity-Enum-Typ wiederverwendet statt eines eigenen "SnowIntensity"
    // (rein generische Light/Moderate/Heavy-Abstufung, kein Regen-spezifischer
    // Inhalt). Liefert RainIntensity::None, solange current() nicht Snow ist.
    RainIntensity currentSnowIntensity();

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

    // Niederschlagswahrscheinlichkeit (%) fuer die aktuelle Stunde - neues
    // hourly-Feld (siehe fetchNow()), zusammen mit dem erweiterten
    // Stundenverlauf unten abgefragt (kein zusaetzlicher API-Aufruf, nur
    // ein zusaetzlicher Parameter derselben ohnehin schon laufenden
    // Anfrage). -1 = nicht verfuegbar (z.B. noch keine erfolgreiche
    // Abfrage).
    int8_t currentPrecipitationProbabilityPercent();

    // Kurzer Stundenverlauf (jetzt/+3h/+6h/+9h) fuer den neuen, eigenen
    // Info-Screen beim Antippen der 3h-Wettervorschau-Ecke
    // (radar_screen.cpp) - im GLEICHEN fetchNow()-Aufruf/Intervall wie
    // die einzelne Forecast oben ermittelt (erweitert deren hourly-
    // Abfragefenster lediglich von einem einzelnen Zeitpunkt auf zehn
    // Stunden, aus denen hier vier herausgegriffen werden), keine
    // zusaetzliche Netzwerkanfrage. "localHour" ist eine grobe,
    // auf volle Stunden gerundete lokale Uhrzeit (0-23) - siehe
    // fetchNow()-Kommentar zur UTC/Lokalzeit-Handhabung.
    struct HourlyPoint {
        bool available = false;
        uint8_t hoursAhead = 0; // 0, 3, 6 oder 9
        uint8_t localHour = 0;
        float temperatureC = 0;
        Condition condition = Condition::Unknown;
    };
    constexpr uint8_t HOURLY_TIMELINE_COUNT = 4;
    struct HourlyTimeline {
        HourlyPoint points[HOURLY_TIMELINE_COUNT];
    };

    // Wie current()/currentMetar() nur vom NetTask geschrieben, vom
    // UI-Thread gelesen - gleiche Begruendung, kein Lock noetig.
    HourlyTimeline currentHourlyTimeline();

    // Naechstgelegener Flughafen zum aktuell aktiven Standort - wird im
    // selben fetchNow()-Zyklus wie die METAR-Abfrage ermittelt (siehe
    // weather.cpp), damit die dafuer ohnehin schon noetige
    // AirportLookup::findNearest()-Abfrage nicht noch einmal irgendwo
    // anders wiederholt werden muss. Fuer die dauerhaft sichtbare
    // "Naechster Flughafen"-Anzeige in der oberen linken Radarecke
    // (radar_screen.cpp) UND fuer den Flughafen-Hinweis auf dem
    // Standort-Presets-Screen (location_presets_screen.cpp) gedacht -
    // beide lesen von hier statt jeweils selbst AirportLookup
    // aufzurufen. iata bleibt ein leerer String, wenn der IATA-Code
    // (nicht in der lokalen airports.csv enthalten, siehe
    // AirportLookup::Nearest) nicht per Live-Abfrage (hexdb.io
    // Flughafen-Endpunkt) ermittelt werden konnte - Aufrufer sollen
    // dann auf icao zurueckfallen, siehe SettingsStore::
    // useIataAirportCodes().
    struct NearestAirport {
        bool available = false;
        char icao[5] = {0};
        char iata[4] = {0};
        char name[32] = {0};
        float distanceKm = 0;
        float bearingDeg = 0; // 0-360, 0 = Nord, im Uhrzeigersinn
        // Koordinaten des Flughafens selbst (nicht des Standorts) - fuer
        // die Best-Effort-Anflug-Erkennung (aircraft_table.cpp::
        // postFetchUpdate()), die daraus die Distanz JEDES Flugzeugs zu
        // diesem Flughafen berechnet, ohne selbst AirportLookup/die SD-
        // Karte anzufassen (dieser Cache wird ohnehin schon hier alle
        // WEATHER_FETCH_INTERVAL_MS neu befuellt).
        double lat = 0;
        double lon = 0;
    };

    // Wie current()/currentMetar() nur vom NetTask geschrieben, vom
    // UI-Thread gelesen - gleiche Begruendung, kein Lock noetig.
    NearestAirport currentNearestAirport();
}
