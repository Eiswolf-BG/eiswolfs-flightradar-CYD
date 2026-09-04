#include "weather.h"
#include "config.h"
#include "location_manager.h"
#include "airport_lookup.h"
#include "radar_math.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

namespace Weather {

namespace {
    Condition currentCondition = Condition::Unknown;
    RainIntensity currentRainIntensityVal = RainIntensity::None;
    RainIntensity currentSnowIntensityVal = RainIntensity::None;
    float currentWindDirDeg = -1.0f;
    float currentWindSpeedKmhVal = -1.0f;
    int8_t currentPrecipProbVal = -1;
    uint32_t lastFetchMs = 0;
    double lastLat = 0;
    double lastLon = 0;
    bool hasLastLocation = false;

    Metar currentMetarData;
    Forecast currentForecastData;
    HourlyTimeline currentHourlyTimelineData;
    NearestAirport currentNearestAirportData;

    // Kurzer, eigener Timeout fuer den hexdb.io-Flughafen-Endpunkt (siehe
    // fetchAirportIata() unten) - dieselbe Begruendung wie
    // HEXDB_TIMEOUT_MS in aircraft_details.cpp: hexdb.io war bereits
    // mehrfach komplett unerreichbar (siehe CLAUDE.md "Bekannte
    // Probleme"), ein IATA-Code ist rein dekorativ und die Abfrage darf
    // deshalb keine spuerbare Wartezeit verursachen, falls der Dienst
    // gerade ausfaellt.
    constexpr uint32_t HEXDB_AIRPORT_TIMEOUT_MS = 1200;

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

    // IATA-Code fuer einen ICAO-Flughafencode per hexdb.io-Flughafen-
    // Endpunkt (liefert u.a. "iata"/"icao"/"airport"-Felder, live per
    // curl bestaetigt: https://hexdb.io/api/v1/airport/icao/<ICAO>) - die
    // lokale airports.csv (AirportLookup) enthaelt NUR ICAO-Codes, kein
    // IATA (siehe Kommentar bei NearestAirport in weather.h), daher diese
    // zusaetzliche Live-Abfrage. Kurzer eigener Timeout (siehe
    // HEXDB_AIRPORT_TIMEOUT_MS oben) - schlaegt sie fehl, bleibt outIata
    // einfach leer und Aufrufer fallen auf den ICAO-Code zurueck, kein
    // Fehler/Absturz.
    void fetchAirportIata(WiFiClientSecure& client, const char* icao, char* outIata, size_t outIataSize) {
        outIata[0] = 0;
        HTTPClient http;
        char url[80];
        snprintf(url, sizeof(url), "https://hexdb.io/api/v1/airport/icao/%s", icao);

        http.setTimeout(HEXDB_AIRPORT_TIMEOUT_MS);
        if (!http.begin(client, url)) return;
        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            http.end();
            return;
        }
        String body = http.getString();
        http.end();

        JsonDocument doc;
        if (deserializeJson(doc, body)) return;
        const char* iata = doc["iata"] | "";
        if (iata[0]) {
            strncpy(outIata, iata, outIataSize - 1);
            outIata[outIataSize - 1] = 0;
        }
    }

    // Ordnet den WMO-Wettercode von Open-Meteo (Feld "weathercode", siehe
    // https://open-meteo.com/en/docs) einer der wenigen Icon-Kategorien zu,
    // die main.cpp zeichnen kann.
    Condition conditionFromWmoCode(int code) {
        if (code == 0) return Condition::Clear;
        if (code == 1 || code == 2) return Condition::PartlyCloudy;
        if (code == 3 || code == 45 || code == 48) return Condition::Cloudy;
        if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return Condition::Rain;
        // 85/86 (Schneeschauer leicht/stark) ergaenzt, siehe Alex' Wunsch fuer
        // den Ruhebildschirm-Schneeeffekt - vorher fielen diese beiden Codes
        // durch alle Kategorien und landeten faelschlich bei "Unknown".
        if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Condition::Snow;
        if (code >= 95) return Condition::Thunderstorm;
        return Condition::Unknown;
    }

    // Feinere Abstufung DESSELBEN weathercode-Feldes fuer den animierten
    // Regen-Effekt (Radarscreen/Ruhebildschirm/WebUI) - conditionFromWmoCode()
    // oben wirft die Intensitaet innerhalb von "Rain"/"Thunderstorm" bewusst
    // weg (fuer das einfache Header-Icon reicht die grobe Kategorie), hier
    // wird sie zusaetzlich ausgewertet. Offizielle WMO-Code-Bedeutungen
    // (siehe https://open-meteo.com/en/docs):
    //   51/53/55 Nieselregen leicht/maessig/dicht, 56/57 gefrierender
    //     Nieselregen leicht/dicht, 61 Regen leicht -> alle als "leicht"
    //     eingestuft (Niesel-Codes und leichter Regen sind vom sichtbaren
    //     Niederschlag her vergleichbar schwach).
    //   63 Regen maessig, 66 gefrierender Regen leicht, 80 Regenschauer
    //     leicht -> "mittel" (Schauer wirken durch ihre Boeen-Natur trotz
    //     "leicht"-Bezeichnung optisch praesenter als gleichmaessiger
    //     Nieselregen, daher hier statt bei "leicht" eingeordnet).
    //   65 Regen stark, 67 gefrierender Regen stark, 81/82 Regenschauer
    //     maessig/heftig, 95/96/99 Gewitter (mit/ohne Hagel) -> "stark".
    RainIntensity intensityFromWmoCode(int code) {
        if (code == 65 || code == 67 || code == 81 || code == 82 || code >= 95) {
            return RainIntensity::Heavy;
        }
        if (code == 63 || code == 66 || code == 80) {
            return RainIntensity::Moderate;
        }
        if ((code >= 51 && code <= 57) || code == 61) {
            return RainIntensity::Light;
        }
        return RainIntensity::None;
    }

    // Gleiches Prinzip wie intensityFromWmoCode() oben, nur fuer die
    // Schnee-Codes (siehe main.cpp::ScreensaverSnow). WMO-Code-Bedeutungen:
    //   71 Schneefall leicht, 77 Schneegriesel -> "leicht" (Griesel ist
    //     feiner/duenner Niederschlag, vergleichbar mit leichtem Schneefall).
    //   73 Schneefall maessig, 85 Schneeschauer leicht -> "mittel" (gleiche
    //     Boeen-Logik wie bei Regenschauern oben).
    //   75 Schneefall stark, 86 Schneeschauer stark -> "stark".
    RainIntensity snowIntensityFromWmoCode(int code) {
        if (code == 75 || code == 86) {
            return RainIntensity::Heavy;
        }
        if (code == 73 || code == 85) {
            return RainIntensity::Moderate;
        }
        if (code == 71 || code == 77) {
            return RainIntensity::Light;
        }
        return RainIntensity::None;
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

        // Stundenverlauf (siehe Weather::HourlyTimeline) fuer den neuen "3h-
        // Wettervorschau antippen"-Screen (radar_screen.cpp) - EIN
        // Abfragefenster von "jetzt" (aktuelle volle Stunde) bis +9h liefert
        // sowohl die vier Zeitpunkte des neuen Verlaufs (jetzt/+3h/+6h/+9h)
        // als auch weiterhin den einzelnen Punkt fuer die bestehende
        // Kurzvorhersage (Forecast, FORECAST_HOURS_AHEAD=3) - keine zweite
        // Anfrage noetig, nur ein breiteres Zeitfenster derselben Anfrage.
        // precipitation_probability ist neu ergaenzt (fuer die
        // "Zusatzdetails"-Zeile im neuen Screen), war vorher nicht Teil der
        // Abfrage. Ohne "timezone"-Parameter interpretiert Open-Meteo
        // start_hour/end_hour als UTC, exakt wie gmtime_r() hier rechnet -
        // kein zusaetzlicher Standort-Zeitzonen-Versatz noetig. Bleibt leer
        // (alle Werte damit unverfuegbar), solange die Uhrzeit noch nicht
        // per NTP synchronisiert ist (gleiche Pruefung wie bei
        // isNightDimHours()) - ohne verlaessliche Uhrzeit liessen sich die
        // Zielstunden gar nicht berechnen.
        char forecastParams[150] = "";
        time_t nowEpoch = time(nullptr);
        if (nowEpoch > 8 * 3600 * 2) {
            time_t endTarget = nowEpoch + 9 * 3600;
            struct tm tmStart, tmEnd;
            gmtime_r(&nowEpoch, &tmStart);
            gmtime_r(&endTarget, &tmEnd);
            snprintf(forecastParams, sizeof(forecastParams),
                     "&hourly=temperature_2m,weathercode,precipitation_probability"
                     "&start_hour=%04d-%02d-%02dT%02d:00&end_hour=%04d-%02d-%02dT%02d:00",
                     tmStart.tm_year + 1900, tmStart.tm_mon + 1, tmStart.tm_mday, tmStart.tm_hour,
                     tmEnd.tm_year + 1900, tmEnd.tm_mon + 1, tmEnd.tm_mday, tmEnd.tm_hour);
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
        currentRainIntensityVal = intensityFromWmoCode(wmoCode);
        currentSnowIntensityVal = snowIntensityFromWmoCode(wmoCode);
        // "winddirection" ist Teil derselben current_weather-Antwort - siehe
        // currentWindDirectionDeg()-Kommentar in weather.h. | -1.0f als
        // Default, falls das Feld ausnahmsweise fehlen sollte (aendert dann
        // nichts an einem vorherigen gueltigen Wert).
        float windDir = doc["current_weather"]["winddirection"] | -1.0f;
        if (windDir >= 0.0f) currentWindDirDeg = windDir;
        // "windspeed" ist ebenfalls Teil derselben current_weather-Antwort
        // (Open-Meteo liefert es standardmaessig mit, in km/h) - bisher
        // nicht ausgelesen, jetzt fuer den neuen Wettervorschau-Screen
        // gebraucht (siehe currentWindSpeedKmh()-Kommentar in weather.h).
        float windSpeed = doc["current_weather"]["windspeed"] | -1.0f;
        if (windSpeed >= 0.0f) currentWindSpeedKmhVal = windSpeed;

        // Stundenverlauf aus dem "hourly"-Teil derselben Antwort - dank des
        // jetzt breiteren start_hour/end_hour-Fensters (siehe oben)
        // enthaelt jedes Array bis zu 10 Eintraege (Index 0 = aktuelle
        // Stunde, Index 9 = +9h). Fehlt der "hourly"-Teil (z.B. weil oben
        // mangels NTP-Zeit gar nicht erst danach gefragt wurde), liefert
        // ArduinoJson fuer .as<JsonArray>() auf einem fehlenden Feld einfach
        // ein leeres Array zurueck - alle folgenden Werte werden dann
        // korrekt auf "nicht verfuegbar" gesetzt statt mit einem veralteten
        // Wert stehen zu bleiben.
        JsonArray hourlyTemp = doc["hourly"]["temperature_2m"].as<JsonArray>();
        JsonArray hourlyCode = doc["hourly"]["weathercode"].as<JsonArray>();
        JsonArray hourlyPrecip = doc["hourly"]["precipitation_probability"].as<JsonArray>();

        // Bestehende Einzelpunkt-Kurzvorhersage (main.cpp::showWeatherInfo())
        // - unveraendert der Punkt bei FORECAST_HOURS_AHEAD (=3), jetzt aus
        // demselben breiteren Array statt einer eigenen Anfrage entnommen.
        Forecast forecast;
        if (hourlyTemp.size() > FORECAST_HOURS_AHEAD && hourlyCode.size() > FORECAST_HOURS_AHEAD) {
            forecast.available = true;
            forecast.temperatureC = hourlyTemp[FORECAST_HOURS_AHEAD].as<float>();
            forecast.condition = conditionFromWmoCode(hourlyCode[FORECAST_HOURS_AHEAD].as<int>());
            forecast.hoursAhead = FORECAST_HOURS_AHEAD;
        }
        currentForecastData = forecast;

        // Niederschlagswahrscheinlichkeit fuer die aktuelle Stunde (Index 0)
        // - fuer die "Zusatzdetails"-Zeile im neuen Wettervorschau-Screen.
        if (hourlyPrecip.size() > 0) {
            currentPrecipProbVal = (int8_t)hourlyPrecip[0].as<int>();
        }

        // Vier-Punkte-Stundenverlauf (jetzt/+3h/+6h/+9h) fuer denselben
        // Screen - "localHour" wird direkt aus nowEpoch+Offset berechnet
        // (nicht aus dem Array-Index abgeleitet), exakt dieselbe Rechnung,
        // die vorher schon fuer den einzelnen Forecast-Punkt oben genutzt
        // wurde, nur fuer vier Offsets statt einem.
        HourlyTimeline timeline;
        constexpr uint8_t TIMELINE_OFFSETS[HOURLY_TIMELINE_COUNT] = {0, 3, 6, 9};
        for (uint8_t i = 0; i < HOURLY_TIMELINE_COUNT; i++) {
            uint8_t off = TIMELINE_OFFSETS[i];
            if (hourlyTemp.size() <= off || hourlyCode.size() <= off) continue;
            HourlyPoint& p = timeline.points[i];
            p.available = true;
            p.hoursAhead = off;
            p.temperatureC = hourlyTemp[off].as<float>();
            p.condition = conditionFromWmoCode(hourlyCode[off].as<int>());
            time_t pointEpoch = nowEpoch + (time_t)off * 3600;
            struct tm tmPoint;
            gmtime_r(&pointEpoch, &tmPoint);
            p.localHour = (uint8_t)tmPoint.tm_hour;
        }
        currentHourlyTimelineData = timeline;

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

            // NearestAirport-Cache (siehe weather.h) - Peilung per
            // RadarMath::toPolar() (dieselbe Funktion, die AirportLookup
            // intern schon fuer die Distanz nutzt) NEU berechnet statt in
            // AirportLookup::Nearest mitgespeichert, um dieses bestehende,
            // an mehreren Stellen genutzte Struct nicht aendern zu
            // muessen. IATA per zusaetzlicher hexdb.io-Abfrage, siehe
            // fetchAirportIata() oben - bleibt bei einem Fehlschlag leer.
            NearestAirport na;
            na.available = true;
            strncpy(na.icao, nearest.icao, sizeof(na.icao) - 1);
            strncpy(na.name, nearest.name, sizeof(na.name) - 1);
            na.distanceKm = nearest.distanceKm;
            na.bearingDeg = RadarMath::toPolar(lat, lon, nearest.lat, nearest.lon).bearingDeg;
            na.lat = nearest.lat;
            na.lon = nearest.lon;
            fetchAirportIata(client, nearest.icao, na.iata, sizeof(na.iata));
            currentNearestAirportData = na;
        } else {
            Serial.println("[Weather] METAR uebersprungen: kein Flughafen in airports.csv auf der SD-Karte gefunden.");
            currentMetarData = Metar{};
            currentNearestAirportData = NearestAirport{};
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

RainIntensity currentRainIntensity() { return currentRainIntensityVal; }

RainIntensity currentSnowIntensity() { return currentSnowIntensityVal; }

float currentWindDirectionDeg() { return currentWindDirDeg; }

float currentWindSpeedKmh() { return currentWindSpeedKmhVal; }

int8_t currentPrecipitationProbabilityPercent() { return currentPrecipProbVal; }

Metar currentMetar() { return currentMetarData; }

Forecast currentForecast() { return currentForecastData; }

HourlyTimeline currentHourlyTimeline() { return currentHourlyTimelineData; }

NearestAirport currentNearestAirport() { return currentNearestAirportData; }

}
