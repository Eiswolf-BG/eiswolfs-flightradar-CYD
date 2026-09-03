#include "aircraft_table.h"
#include "radar_math.h"
#include "weather.h"
#include "units.h"
#include <algorithm>
#include <atomic>
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

Aircraft* raw() { return table; }
uint8_t capacity() { return Config::MAX_TRACKED_AIRCRAFT; }

uint8_t validCount() {
    uint8_t n = 0;
    for (auto& a : table) if (a.valid) n++;
    return n;
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
        a.distanceKm = polar.distanceKm;
        a.bearingDeg = polar.bearingDeg;

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