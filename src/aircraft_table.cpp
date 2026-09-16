#include "aircraft_table.h"
#include "radar_math.h"
#include "weather.h"
#include "units.h"
#include "settings_store.h"
#include "session_stats.h"
#include "daily_sightings.h"
#include <algorithm>
#include <atomic>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// WICHTIG: Diese Datei ruft absichtlich KEIN AirlineLookup::resolve() mehr auf!
// postFetchUpdate() wird vom NetTask auf Core 0 aufgerufen. AirlineLookup
// braucht SD-Kartenzugriff, und die SD-Karte wurde in setup() auf Core 1
// initialisiert - Zugriff von Core 0 aus fuehrte zu einem Haenger (Task
// Watchdog auf IDLE0). Die Aufloesung der Airline-Namen passiert deshalb
// jetzt in main.cpp/renderAircraftList() auf Core 1 (demselben Core, der
// die SD-Karte urspruenglich initialisiert hat).

namespace AircraftTable {

namespace {
    Aircraft table[Config::MAX_TRACKED_AIRCRAFT];
    constexpr uint32_t STALE_TIMEOUT_MS = Config::FETCH_INTERVAL_MS * 3; // ~24s
    SemaphoreHandle_t mutex = nullptr;
    uint32_t versionCounter = 0;

    // Offline-/Stale-Data-Modus (siehe aircraft_table.h) - einzelner
    // atomarer Zeitstempel, kein Mutex noetig (gleiches Muster wie
    // LedAlert::heartbeatStartMs): Core 0 schreibt ihn per store(), Core 1
    // liest ihn per load() und leitet "wie lange her?" rein rechnerisch
    // ab, ohne je zurueckzuschreiben.
    std::atomic<uint32_t> lastSuccessfulFetchMs{0};

    // Ergebnis des letzten Abrufversuchs (Feature 5 "Verbindungsqualitaet",
    // connection_status_screen.cpp) - unabhaengig vom reinen Zeitstempel
    // oben, der nur ERFOLGREICHE Abrufe merkt. Drei separate Atomics statt
    // eines Mutex-geschuetzten Structs, da jedes Feld unabhaengig gelesen/
    // geschrieben wird (gleiches einfaches Cross-Core-Muster wie
    // lastSuccessfulFetchMs) - ein torn read (z.B. ok=true mit noch altem
    // httpCode) waere hier unkritisch, das ist reine Diagnose-Anzeige.
    std::atomic<bool> hasFetchOutcome{false};
    std::atomic<bool> lastFetchOk{false};
    std::atomic<int>  lastFetchHttpCode{0};
    std::atomic<uint32_t> lastFetchDurationMs{0};
}

void lock() { xSemaphoreTake(mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGive(mutex); }

uint32_t version() { return versionCounter; }

void init() {
    if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    for (auto& a : table) a = Aircraft{};
    // Verhindert einen faelschlichen "offline seit dem Boot"-Zustand,
    // bevor ueberhaupt der allererste ADS-B-Abrufversuch gelaufen ist -
    // init() laeuft beim Setup, deutlich vor dem ersten NetTask-Zyklus.
    lastSuccessfulFetchMs.store(millis(), std::memory_order_relaxed);
}

void markFetchSuccess(uint32_t nowMs) {
    lastSuccessfulFetchMs.store(nowMs, std::memory_order_relaxed);
}

uint32_t msSinceLastSuccessfulFetch(uint32_t nowMs) {
    return nowMs - lastSuccessfulFetchMs.load(std::memory_order_relaxed);
}

void recordFetchOutcome(bool ok, int httpCode, uint32_t durationMs) {
    lastFetchOk.store(ok, std::memory_order_relaxed);
    lastFetchHttpCode.store(httpCode, std::memory_order_relaxed);
    lastFetchDurationMs.store(durationMs, std::memory_order_relaxed);
    hasFetchOutcome.store(true, std::memory_order_relaxed);
}

FetchOutcome lastFetchOutcome() {
    FetchOutcome out;
    out.hasResult = hasFetchOutcome.load(std::memory_order_relaxed);
    out.ok = lastFetchOk.load(std::memory_order_relaxed);
    out.httpCode = lastFetchHttpCode.load(std::memory_order_relaxed);
    out.durationMs = lastFetchDurationMs.load(std::memory_order_relaxed);
    return out;
}

Aircraft* raw() { return table; }
uint8_t capacity() { return Config::MAX_TRACKED_AIRCRAFT; }

uint8_t validCount() {
    uint8_t n = 0;
    for (auto& a : table) if (a.valid) n++;
    return n;
}

void ageOutStale(uint32_t nowMs) {
    lock();
    bool anyEvicted = false;
    for (auto& a : table) {
        if (!a.valid) continue;
        if (nowMs - a.lastSeenMs > STALE_TIMEOUT_MS) {
            a = Aircraft{};
            anyEvicted = true;
        }
    }
    // Bewusst nicht mit der (weiterhin vorhandenen, siehe dort) Alterung in
    // postFetchUpdate() zusammengelegt, um deren bestehende, laengst
    // getestete Struktur nicht anzufassen - ein doppelter Eviction-Check
    // ist unschaedlich, da a.valid ohnehin zuerst geprueft wird. Sortierung/
    // versionCounter++ nur bei tatsaechlicher Aenderung, damit ein Aufruf
    // ohne veraltete Eintraege (der Normalfall) keine unnoetigen Redraws
    // ausloest.
    if (anyEvicted) {
        std::sort(table, table + Config::MAX_TRACKED_AIRCRAFT,
                  [](const Aircraft& a, const Aircraft& b) {
                      if (a.valid != b.valid) return a.valid > b.valid;
                      if (!a.valid) return false;
                      return a.distanceKm < b.distanceKm;
                  });
        versionCounter++;
    }
    unlock();
}

void postFetchUpdate(double homeLat, double homeLon) {
    uint32_t now = millis();

    // Best-Effort-Anflug-Erkennung (siehe aircraft.h::approachLikely) -
    // dieselbe Flughafen-Referenz wie die "Naechster Flughafen"-
    // Eckanzeige, aus dem ohnehin schon alle WEATHER_FETCH_INTERVAL_MS
    // aktualisierten Cache gelesen (kein zusaetzlicher SD-/API-Zugriff
    // hier). Einmal pro Zyklus ausserhalb der Schleife geholt, nicht pro
    // Flugzeug.
    Weather::NearestAirport nearestAirport = Weather::currentNearestAirport();

    for (auto& a : table) {
        if (!a.valid) continue;

        if (now - a.lastSeenMs > STALE_TIMEOUT_MS) {
            a = Aircraft{}; // evict
            continue;
        }

        auto polar = RadarMath::toPolar(homeLat, homeLon, a.lat, a.lon);

        // Naeherungs-/Entfernungs-Trend (aircraft.h::DistanceTrend) - exakt
        // dasselbe "vorherige Distanz merken und vergleichen"-Muster wie
        // die Anflug-Erkennung unten (prevAirportDistKm), nur bezogen auf
        // die Distanz zum eigenen Standort statt zum naechsten Flughafen.
        // Schwellenwert siehe Config::DISTANCE_TREND_THRESHOLD_KM.
        if (a.prevDistanceKm >= 0) {
            float delta = polar.distanceKm - a.prevDistanceKm;
            if (delta <= -Config::DISTANCE_TREND_THRESHOLD_KM) {
                a.distanceTrend = Aircraft::DistanceTrend::Approaching;
            } else if (delta >= Config::DISTANCE_TREND_THRESHOLD_KM) {
                a.distanceTrend = Aircraft::DistanceTrend::Departing;
            } else {
                a.distanceTrend = Aircraft::DistanceTrend::Passing;
            }
        } else {
            a.distanceTrend = Aircraft::DistanceTrend::Unknown;
        }

        // Circle-Crossing-Puls (radar_screen.cpp, siehe Aircraft::
        // ringCrossedAtMs) - dieselbe "alten Wert merken und mit aktuellem
        // vergleichen"-Idee wie beim intelligenten Proximity-Alarm
        // (radar_screen.cpp::updateProximityAlert()), hier aber bezogen auf
        // die tatsaechlich angezeigten Ring-Distanzen (1/3, 2/3, Aussenrand
        // der AKTUELLEN Anzeige-Reichweite - kann sich durch manuelle Wahl
        // aendern) statt auf feste Alarm-Zonen. Bewusst als direkter
        // Vergleich der tatsaechlichen ALTEN (a.prevDistanceKm, noch nicht
        // ueberschrieben) und NEUEN (polar.distanceKm) Distanz gegen die
        // Schwellen - NICHT als gespeicherter Zonen-INDEX wie
        // proximityZone: ein reiner Reichweiten-Wechsel aendert nur, WO die
        // Schwellen gerade liegen, kann aber niemals einen Puls ausloesen,
        // wenn sich die tatsaechliche Distanz zwischen den beiden Werten
        // gar nicht veraendert hat (Alex' Vorgabe: kein Fehlalarm durch
        // reinen manuellen Reichweiten-Wechsel).
        // "In beide Richtungen": lo/hi bilden das Intervall unabhaengig von
        // der Bewegungsrichtung, ein Ueberschreiten wird so unabhaengig
        // davon erkannt, ob sich das Flugzeug naehert oder entfernt.
        a.ringCrossedAtMs = 0;
        if (a.prevDistanceKm >= 0) {
            float rangeKmNow = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];
            float thresholds[3] = { rangeKmNow / 3.0f, rangeKmNow * 2.0f / 3.0f, rangeKmNow };
            float lo = (a.prevDistanceKm < polar.distanceKm) ? a.prevDistanceKm : polar.distanceKm;
            float hi = (a.prevDistanceKm < polar.distanceKm) ? polar.distanceKm : a.prevDistanceKm;
            for (float threshold : thresholds) {
                if (threshold > lo && threshold <= hi) {
                    a.ringCrossedAtMs = now;
                    break;
                }
            }
        }

        a.prevDistanceKm = polar.distanceKm;

        a.distanceKm = polar.distanceKm;
        a.bearingDeg = polar.bearingDeg;

        // Flugzeug-Steckbrief fuers Flugbuch (aircraft.h::
        // sessionMinDistanceKm/sessionMaxSpeedKt, siehe flight_logbook.cpp)
        // - laufend die bisher kleinste Distanz bzw. hoechste Geschwindigkeit
        // SEIT dem ersten Sichten in dieser Sitzung festhalten. -1 (Default,
        // noch keine Messung) ist immer kleiner als jede echte Distanz bzw.
        // -1 < jede echte Geschwindigkeit >= 0, daher reicht ein einfacher
        // Vergleich ohne Sonderfall fuer den allerersten Zyklus.
        if (a.sessionMinDistanceKm < 0 || polar.distanceKm < a.sessionMinDistanceKm) {
            a.sessionMinDistanceKm = polar.distanceKm;
        }
        if (a.groundSpeedKt > a.sessionMaxSpeedKt) {
            a.sessionMaxSpeedKt = a.groundSpeedKt;
        }

        // Reine In-RAM-Sitzungsstatistik (Alex' Wunsch, siehe
        // session_stats.h) - GLOBAL ueber ALLE Flugzeuge dieser Sitzung
        // hinweg, im Unterschied zu sessionMinDistanceKm/sessionMaxSpeedKt
        // oben (die nur PRO Flugzeug gelten). Bewusst unabhaengig vom
        // Flugbuch-Schalter (SettingsStore::flightLogbookEnabled()), laeuft
        // also immer mit.
        SessionStats::record(a);

        // Taeglicher (lokale Kalenderzeit) Sichtungszaehler fuers Detail-
        // Panel (Alex' Wunsch, siehe daily_sightings.h) - eigenstaendig
        // von SessionStats oben, da dort ausschliesslich seit dem letzten
        // Neustart gezaehlt wird, hier dagegen taeglich zurueckgesetzt.
        DailySightings::record(a);

        // "Ueberflug"-CPA (Closest Point of Approach, siehe aircraft.h::
        // cpaRelevant/cpaEtaMin und Config::CPA_*) - reine Momentaufnahme
        // aus Position, Kurs und Geschwindigkeit DIESES Zyklus, kein
        // "vorherige Messung merken"-Muster noetig. Nur ueberhaupt relevant,
        // wenn sich das Flugzeug gerade annaehert (s.o.), es kein Boden-
        // fahrzeug ist (Kategorie "C" ueberfliegt nichts) und schnell genug
        // fuer eine numerisch stabile Berechnung ist.
        a.cpaRelevant = false;
        a.cpaEtaMin = 0;
        if (a.distanceTrend == Aircraft::DistanceTrend::Approaching &&
            a.category[0] != 'C' &&
            a.groundSpeedKt >= Config::CPA_MIN_SPEED_KT) {
            constexpr double DEG2RAD = M_PI / 180.0;
            // r = Position des Flugzeugs relativ zum eigenen Standort
            // (Ost-/Nord-Komponenten aus der bereits berechneten Distanz/
            // Peilung) - Standard-CPA-Konvention "Position des Ziels
            // relativ zum eigenen Standort".
            double bearingRad = polar.bearingDeg * DEG2RAD;
            double rx = polar.distanceKm * sin(bearingRad);
            double ry = polar.distanceKm * cos(bearingRad);

            // v = Geschwindigkeitsvektor des Flugzeugs aus Kurs+Ground-
            // speed, in km/min (passend zur gewuenschten Zeiteinheit).
            double headingRad = a.headingDeg * DEG2RAD;
            double speedKmPerMin = Units::ktToKmh(a.groundSpeedKt) / 60.0;
            double vx = speedKmPerMin * sin(headingRad);
            double vy = speedKmPerMin * cos(headingRad);

            double rv = rx * vx + ry * vy;
            double vv = vx * vx + vy * vy;
            if (vv > 0.0001) {
                // Zeit bis zum naechsten Punkt auf der aktuellen Flugbahn,
                // siehe Config::CPA_MAX_TIME_MIN-Kommentar fuer die Formel-
                // Herleitung.
                double tStar = -rv / vv;
                if (tStar > 0.0 && tStar <= (double)Config::CPA_MAX_TIME_MIN) {
                    double cpaX = rx + vx * tStar;
                    double cpaY = ry + vy * tStar;
                    double cpaDistKm = sqrt(cpaX * cpaX + cpaY * cpaY);
                    if (cpaDistKm <= Config::CPA_MAX_DISTANCE_KM) {
                        a.cpaRelevant = true;
                        a.cpaEtaMin = (float)tStar;
                    }
                }
            }
        }

        if (nearestAirport.available) {
            float airportDistKm = RadarMath::toPolar(a.lat, a.lon, nearestAirport.lat, nearestAirport.lon).distanceKm;

            // "Distanz sinkt ueber die letzten Zyklen" - nur pruefbar, wenn
            // bereits eine vorherige Messung vorliegt (siehe Kommentar bei
            // prevAirportDistKm in aircraft.h). Alle uebrigen Kriterien
            // sind rein Momentaufnahmen des aktuellen Zyklus.
            bool distanceDecreasing = a.prevAirportDistKm >= 0 && airportDistKm < a.prevAirportDistKm;
            bool closeEnough = airportDistKm <= Config::APPROACH_MAX_DISTANCE_KM;
            bool descending = a.vertRateFtMin < 0;
            bool speedPlausible = a.groundSpeedKt > 0 && a.groundSpeedKt < Config::APPROACH_MAX_SPEED_KT;
            bool altPlausible = a.altBaroFt < Config::APPROACH_MAX_ALT_FT;

            a.approachLikely = distanceDecreasing && closeEnough && descending && speedPlausible && altPlausible;
            if (a.approachLikely) {
                float etaMin = airportDistKm / Units::ktToKmh(a.groundSpeedKt) * 60.0f;
                if (etaMin >= Config::APPROACH_ETA_MIN_PLAUSIBLE_MIN && etaMin <= Config::APPROACH_ETA_MAX_PLAUSIBLE_MIN) {
                    a.approachEtaMin = (uint16_t)(etaMin + 0.5f);
                } else {
                    // Kriterien erfuellt, aber ETA nicht plausibel darstellbar -
                    // Zeile bleibt dann in drawDetailPanel() trotzdem weg (dort
                    // wird approachEtaMin > 0 mitgeprueft), statt eine unsinnige
                    // Zahl zu zeigen (siehe Alex' Vorgabe).
                    a.approachEtaMin = 0;
                }
            } else {
                a.approachEtaMin = 0;
            }
            a.prevAirportDistKm = airportDistKm;
        } else {
            a.approachLikely = false;
            a.approachEtaMin = 0;
            a.prevAirportDistKm = -1;
        }
    }
    std::sort(table, table + Config::MAX_TRACKED_AIRCRAFT,
              [](const Aircraft& a, const Aircraft& b) {
                  if (a.valid != b.valid) return a.valid > b.valid;
                  if (!a.valid) return false;
                  return a.distanceKm < b.distanceKm;
              });
    versionCounter++;
}

}