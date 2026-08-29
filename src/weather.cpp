#include "weather.h"
#include "config.h"
#include "location_manager.h"
#include "airport_lookup.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

namespace Weather {

namespace {
    Condition currentCondition = Condition::Unknown;
    float currentWindDirDeg = -1.0f;
    uint32_t lastFetchMs = 0;
    double lastLat = 0;
    double lastLon = 0;
    bool hasLastLocation = false;

    Metar currentMetarData;
    Forecast currentForecastData;

    // Wie lange im Voraus die Kurzvorhersage gelten soll (siehe
    // Weather::Forecast in weather.h) - eine einzelne konstante Stundenzahl,
    // kein einstellbarer Wert, um die Sache bewusst einfach zu halten.
    constexpr uint8_t FORECAST_HOURS_AHEAD = 3;

    // aviationweather.gov liefert bei format=json ein JSON-ARRAY (auch fuer
    // eine einzelne angefragte Station) - Feld "rawOb" enthaelt den
    // kompletten rohen METAR-Text (inkl. dem Wort "METAR" am Anfang, so wie
    // von der API geliefert). Kein API-Key noetig, dieselbe kostenlose
    // Daten-API, die z.B. auch ForeFlight/SkyVector nutzen.
    //
    // Bekommt den client von fetchNow() uebergeben (siehe dort) statt einen
    // eigenen/globalen zu halten - siehe Kommentar bei fetchNow() zum
    // "Bad file number"-Absturz, den ein dauerhaft gehaltener Client mit
    // zwei verschiedenen Hosts verursacht hat.
    void fetchMetarFor(WiFiClientSecure& client, const char* icao) {
        HTTPClient http;
        char url[128];
        snprintf(url, sizeof(url), "https://aviationweather.gov/api/data/metar?ids=%s&format=json", icao);

        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) {
            Serial.println("[Weather] METAR-Abfrage fehlgeschlagen: http.begin() lieferte false.");
            return;
        }

        // Manche Server (aviationweather.gov eingeschlossen) lehnen
        // Anfragen ohne User-Agent-Header eher ab bzw. liefern damit
        // zuverlaessiger - der ESP32-HTTPClient sendet standardmaessig
        // keinen. Rein defensiv, kostet nichts, falls nicht noetig.
        http.addHeader("User-Agent", "EiswolfsFlightradarCYD/1.0");

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.printf("[Weather] METAR-Abfrage fuer %s fehlgeschlagen: HTTP %d\n", icao, code);
            http.end();
            return;
        }

        // Body erst komplett als String einsammeln statt direkt aus
        // http.getStream() zu parsen - gleicher Grund/Fix wie bei der
        // Open-Meteo-Abfrage oben (siehe dortiger Kommentar).
        String body = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            Serial.printf("[Weather] METAR-JSON fuer %s nicht lesbar: %s\n", icao, err.c_str());
            return;
        }
        if (!doc.is<JsonArray>() || doc.size() == 0) {
            Serial.printf("[Weather] METAR fuer %s: leere/unerwartete Antwort (evtl. keine aktuelle Meldung fuer diese Station).\n", icao);
            return;
        }

        const char* raw = doc[0]["rawOb"] | "";
        if (!raw[0]) {
            Serial.printf("[Weather] METAR fuer %s: kein rawOb-Feld in der Antwort.\n", icao);
            return;
        }

        currentMetarData.available = true;
        strncpy(currentMetarData.icao, icao, sizeof(currentMetarData.icao) - 1);
        currentMetarData.icao[sizeof(currentMetarData.icao) - 1] = 0;
        strncpy(currentMetarData.raw, raw, sizeof(currentMetarData.raw) - 1);
        currentMetarData.raw[sizeof(currentMetarData.raw) - 1] = 0;

        // Erfolgs-Log ergaenzt - vorher gab es bei Erfolg GAR KEINE
        // Ausgabe, wodurch sich "Abfrage lief nie" nicht von "Abfrage lief
        // still und erfolgreich durch" unterscheiden liess.
        Serial.printf("[Weather] METAR fuer %s erfolgreich empfangen: %s\n", icao, currentMetarData.raw);
    }

    // Ordnet den WMO-Wettercode von Open-Meteo (Feld "weathercode", siehe
    // https://open-meteo.com/en/docs) einer der wenigen Icon-Kategorien zu,
    // die main.cpp zeichnen kann.
    Condition conditionFromWmoCode(int code) {
        if (code == 0) return Condition::Clear;
        if (code == 1 || code == 2) return Condition::PartlyCloudy;
        if (code == 3 || code == 45 || code == 48) return Condition::Cloudy;
        if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return Condition::Rain;
        if (code >= 71 && code <= 77) return Condition::Snow;
        if (code >= 95) return Condition::Thunderstorm;
        return Condition::Unknown;
    }

    void fetchNow(double lat, double lon) {
        if (WiFi.status() != WL_CONNECTED) return;

        // Frisches, nur fuer diesen einen Aufruf lebendes Client-Objekt -
        // vorher war das ein dauerhaft gehaltener globaler Client, der ueber
        // die gesamte Laufzeit des Geraets (alle 10 Minuten) hinweg
        // wiederverwendet wurde. Seit der METAR-Ergaenzung wurde DERSELBE
        // Client dabei zusaetzlich fuer einen ZWEITEN, komplett anderen Host
        // (aviationweather.gov statt api.open-meteo.com) genutzt - genau
        // diese Kombination (dauerhaft gehalten + mehrere Hosts ueber sehr
        // viele Zyklen) fuehrte zu einem "Bad file number"-Socket-Fehler und
        // einem haengenden NetTask (siehe Testbericht). Ein frisches, rein
        // lokales Client-Objekt pro Aufruf - dasselbe bewaehrte Muster wie
        // AircraftDetails::update(), das ebenfalls einen einzigen lokalen
        // Client fuer mehrere verschiedene Hosts innerhalb eines Aufrufs
        // wiederverwendet, ihn danach aber komplett verwirft - behebt das.
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(Config::HTTP_TIMEOUT_MS);

        // Kurzvorhersage (siehe Weather::Forecast) fuer genau EINEN
        // stuendlichen Zeitpunkt ("jetzt + FORECAST_HOURS_AHEAD Stunden")
        // ueber Open-Meteo's start_hour/end_hour-Parameter angefragt (beide
        // auf dieselbe volle Stunde gesetzt), statt ueber den einfacheren
        // forecast_hours-Parameter, der ab der aktuellen Tagesstunde 00:00
        // zaehlt und damit mehrere Datenpunkte liefern wuerde, aus denen der
        // richtige erst noch herausgesucht werden muesste. So liefert die
        // Antwort direkt genau einen Eintrag - weniger Speicher, kein
        // Zeitstempel-Abgleich noetig. Ohne "timezone"-Parameter interpretiert
        // Open-Meteo start_hour/end_hour als UTC, exakt wie gmtime_r() hier
        // rechnet - kein zusaetzlicher Standort-Zeitzonen-Versatz noetig.
        // Bleibt leer (und die Vorhersage damit unverfuegbar), solange die
        // Uhrzeit noch nicht per NTP synchronisiert ist (gleiche Pruefung
        // wie bei isNightDimHours()) - ohne verlässliche Uhrzeit liesse sich
        // "in 3 Stunden" gar nicht berechnen.
        char forecastParams[100] = "";
        time_t nowEpoch = time(nullptr);
        if (nowEpoch > 8 * 3600 * 2) {
            time_t target = nowEpoch + (time_t)FORECAST_HOURS_AHEAD * 3600;
            struct tm tmTarget;
            gmtime_r(&target, &tmTarget);
            snprintf(forecastParams, sizeof(forecastParams),
                     "&hourly=temperature_2m,weathercode"
                     "&start_hour=%04d-%02d-%02dT%02d:00&end_hour=%04d-%02d-%02dT%02d:00",
                     tmTarget.tm_year + 1900, tmTarget.tm_mon + 1, tmTarget.tm_mday, tmTarget.tm_hour,
                     tmTarget.tm_year + 1900, tmTarget.tm_mon + 1, tmTarget.tm_mday, tmTarget.tm_hour);
        }

        HTTPClient http;
        char url[256];
        snprintf(url, sizeof(url),
                 "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true%s",
                 lat, lon, forecastParams);

        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return;

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            http.end();
            return;
        }

        // Body erst komplett als String einsammeln (getString() kuemmert
        // sich zuverlaessig um Chunked-Transfer-Encoding), statt direkt aus
        // http.getStream() zu parsen - Letzteres scheiterte bei Open-Meteo
        // zuverlaessig mit einem ArduinoJson-"InvalidInput"-Fehler.
        String body = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) return;

        int wmoCode = doc["current_weather"]["weathercode"] | -1;
        if (wmoCode < 0) return;

        currentCondition = conditionFromWmoCode(wmoCode);
        // "winddirection" ist Teil derselben current_weather-Antwort - siehe
        // currentWindDirectionDeg()-Kommentar in weather.h. | -1.0f als
        // Default, falls das Feld ausnahmsweise fehlen sollte (aendert dann
        // nichts an einem vorherigen gueltigen Wert).
        float windDir = doc["current_weather"]["winddirection"] | -1.0f;
        if (windDir >= 0.0f) currentWindDirDeg = windDir;

        // Kurzvorhersage aus dem "hourly"-Teil derselben Antwort - dank
        // start_hour/end_hour oben enthaelt jedes der beiden Arrays hoechstens
        // einen einzigen Eintrag. Fehlt der "hourly"-Teil (z.B. weil oben
        // mangels NTP-Zeit gar nicht erst danach gefragt wurde), liefert
        // ArduinoJson fuer .as<JsonArray>() auf einem fehlenden Feld einfach
        // ein leeres Array zurueck - currentForecastData wird dann korrekt
        // auf "nicht verfuegbar" gesetzt statt mit einem veralteten Wert
        // stehen zu bleiben.
        Forecast forecast;
        JsonArray hourlyTemp = doc["hourly"]["temperature_2m"].as<JsonArray>();
        JsonArray hourlyCode = doc["hourly"]["weathercode"].as<JsonArray>();
        if (hourlyTemp.size() > 0 && hourlyCode.size() > 0) {
            forecast.available = true;
            forecast.temperatureC = hourlyTemp[0].as<float>();
            forecast.condition = conditionFromWmoCode(hourlyCode[0].as<int>());
            forecast.hoursAhead = FORECAST_HOURS_AHEAD;
        }
        currentForecastData = forecast;

        // METAR-Flugwetterbericht fuer den naechstgelegenen Flughafen - im
        // selben Aufruf/Intervall wie das Icon-Wetter oben, damit dafuer
        // keine zusaetzliche Netzwerklast/kein eigener Timer noetig ist. Der
        // naechste Flughafen wird bei JEDEM Aufruf neu bestimmt (billige,
        // rein lokale SD-Abfrage ueber AirportLookup, kein Netzwerkzugriff)
        // - so bleibt er auch nach einem Standortwechsel (anderes Preset)
        // automatisch aktuell, ohne eigene Aenderungserkennung wie beim
        // Icon-Wetter oben.
        AirportLookup::Nearest nearest = AirportLookup::findNearest(lat, lon);
        if (nearest.found) {
            Serial.printf("[Weather] Naechster Flughafen: %s (%s), %.0f km entfernt - frage METAR ab...\n",
                          nearest.icao, nearest.name, nearest.distanceKm);
            fetchMetarFor(client, nearest.icao);
        } else {
            Serial.println("[Weather] METAR uebersprungen: kein Flughafen in airports.csv auf der SD-Karte gefunden.");
            currentMetarData = Metar{};
        }
    }
}

void update() {
    double lat = 0, lon = 0;
    LocationManager::getHomeLocation(lat, lon);
    if (lat == 0 && lon == 0) return;

    // Deutliche Standort-Aenderung (z.B. anderes Standort-Preset aktiviert)
    // - sofort neu abfragen statt bis zum naechsten regulaeren Intervall zu
    // warten, damit das Icon nicht minutenlang das Wetter des alten
    // Standorts zeigt.
    bool locationChanged = !hasLastLocation ||
                            fabs(lat - lastLat) > 0.01 || fabs(lon - lastLon) > 0.01;

    uint32_t now = millis();
    if (!locationChanged && (now - lastFetchMs < Config::WEATHER_FETCH_INTERVAL_MS)) {
        return;
    }

    lastFetchMs = now;
    lastLat = lat;
    lastLon = lon;
    hasLastLocation = true;
    fetchNow(lat, lon);
}

Condition current() { return currentCondition; }

float currentWindDirectionDeg() { return currentWindDirDeg; }

Metar currentMetar() { return currentMetarData; }

Forecast currentForecast() { return currentForecastData; }

}
