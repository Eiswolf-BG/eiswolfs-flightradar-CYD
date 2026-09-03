#include "net_task.h"
#include "config.h"
#include "wifi_manager.h"
#include "location_manager.h"
#include "adsb_client.h"
#include "aircraft_table.h"
#include "aircraft.h"
#include "settings_store.h"
#include "aircraft_details.h"
#include "flight_logbook.h"
#include "led_alert.h"
#include "web_export_server.h"
#include "weather.h"
#include "ota_update.h"
#include "iss_tracker.h"
#include "mqtt_client.h"
#include "aircraft_watchlist.h"
#include "squawk_watchlist.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <atomic>

namespace NetTask {

namespace {
    TaskHandle_t taskHandle = nullptr;
    uint32_t lastFetchMs = 0;

    // TESTWEISE - adaptives ADS-B-Abfrageintervall (siehe Absprache mit
    // Karl, Reaktion auf vereinzelte HTTP 429 von adsb.lol): startet bei
    // Config::FETCH_INTERVAL_MS, verdoppelt sich nach einem 429 (oder
    // uebernimmt dessen Retry-After-Wert), gedeckelt bei
    // Config::FETCH_BACKOFF_MAX_MS, und kehrt nach jeder erfolgreichen
    // Abfrage schrittweise (nicht abrupt) zum Grundintervall zurueck. Bei
    // einem einzelnen sonstigen Fehlschlag (Timeout/SSL) wird stattdessen
    // fest Config::FETCH_RETRY_DELAY_MS gewartet, um bei kurzen WLAN-
    // Aussetzern keine Anfragen-Flut auszuloesen.
    uint32_t currentIntervalMs = Config::FETCH_INTERVAL_MS;

    // Von NetTask (Core 0) geschrieben, von pause() (Core 1, siehe unten)
    // gelesen - std::atomic statt eines ungeschuetzten bool, gleiches
    // Muster wie beim Heartbeat-Race-Fix in led_alert.cpp. true, solange
    // NetTask NICHT mitten in einer ADS-B-Netzwerkoperation steckt (also
    // "sicher" fuer eine Suspendierung) - wird NUR um AdsbClient::fetch()
    // herum kurzzeitig auf false gesetzt, sonst bleibt es true (inkl.
    // WifiMgr/Weather/OTA-Hintergrundcheck/50ms-Delay). pause() unten
    // wartet aktiv auf true, bevor es wirklich suspendiert - siehe
    // net_task.h fuer die ausfuehrliche Begruendung.
    std::atomic<bool> netTaskIdle{true};

    Aircraft tempTable[Config::MAX_TRACKED_AIRCRAFT];

    bool webServerStarted = false;

    void taskFunc(void*) {
        MqttClient::init();

        for (;;) {
            WifiMgr::update();
            LocationManager::update();

            if (!webServerStarted && WifiMgr::getState() == WifiMgr::State::Connected) {
                WebExportServer::begin();
                webServerStarted = true;
            }
            if (webServerStarted) {
                WebExportServer::update();
            }

            AircraftDetails::update();
            Weather::update();
            // Bonus-Feature (siehe iss_tracker.h) - kuemmert sich intern
            // um ihr eigenes, deutlich selteneres Intervall
            // (Config::ISS_FETCH_INTERVAL_MS), gleiches Muster wie
            // Weather::update() oben. Nur HTTP (kein TLS), daher unkritisch
            // fuer die ADS-B-Speicher-/Verbindungsproblematik.
            IssTracker::update();
            // Kuemmert sich intern um ihr eigenes, deutlich selteneres
            // Intervall (Config::OTA_BACKGROUND_CHECK_INTERVAL_MS, siehe
            // ota_update.cpp) - hier einfach jede Schleife mit aufrufen,
            // genau wie Weather::update() oben.
            OtaUpdate::pollBackground();
            // Optionale MQTT-Anbindung (SettingsStore::mqttEnabled(), AUS
            // per Default, siehe mqtt_client.h) - kuemmert sich selbst um
            // (Wieder-)Verbinden mit eigenem Mindestabstand zwischen
            // Versuchen, hier einfach jede Schleife mit aufrufen, gleiches
            // Muster wie Weather::update()/OtaUpdate::pollBackground() oben.
            MqttClient::loop();
            // Unabhaengig vom Erfolg der ADS-B-Abfrage weiter unten pruefen,
            // damit die 24h-Sicherheitsabschaltung des Flugbuchs auch waehrend
            // laengerer WLAN-/ADS-B-Ausfaelle zuverlaessig greift (siehe
            // flight_logbook.cpp::enforceAutoOff() fuer den Hintergrund).
            FlightLogbook::enforceAutoOff();

            if (millis() - lastFetchMs >= currentIntervalMs) {
                lastFetchMs = millis();

                if (WifiMgr::getState() == WifiMgr::State::Connected) {
                    LocationManager::requestIpLookupIfNeeded();

                    double lat = 0, lon = 0;
                    LocationManager::getHomeLocation(lat, lon);

                    float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];

                    // Solange die WebUI-Livekarte gerade aktiv geoeffnet ist
                    // (siehe WebExportServer::isRadarUiActive()), auf der
                    // groessten konfigurierten Reichweitenstufe abfragen -
                    // sonst kann der Reichweiten-Waehler dort nie mehr
                    // Flugzeuge zeigen als am Geraet selbst gerade
                    // eingestellt ist (Flugzeuge ausserhalb der Geraete-
                    // Reichweite werden ja gar nicht erst abgefragt/
                    // gespeichert). Nur waehrend die WebUI tatsaechlich
                    // genutzt wird, um die zusaetzliche Netzwerk-/Speicher-
                    // last nicht dauerhaft allen Nutzern aufzuerlegen. Das
                    // Geraete-Display selbst filtert beim Zeichnen weiterhin
                    // unabhaengig auf seine eigene rangeIndex()-Reichweite
                    // (siehe radar_screen.cpp), zeigt also trotzdem nur die
                    // eingestellte Reichweite an.
                    if (webServerStarted && WebExportServer::isRadarUiActive()) {
                        rangeKm = Config::RANGE_STEPS_KM[Config::RANGE_STEP_COUNT - 1];
                    }

                    // Ab hier bis zum Ende der Ergebnisverarbeitung unten
                    // NICHT idle - siehe Kommentar bei netTaskIdle oben.
                    netTaskIdle.store(false, std::memory_order_release);

                    auto result = AdsbClient::fetch(lat, lon, rangeKm,
                                                     tempTable, Config::MAX_TRACKED_AIRCRAFT);

                    if (result.ok) {
                        // Offline-/Stale-Data-Modus (radar_screen.cpp) - haelt
                        // fest, WANN zuletzt ein ADS-B-Abruf erfolgreich war,
                        // unabhaengig vom "letzten bekannten Wert"-Tracking
                        // einzelner Flugzeuge weiter unten.
                        AircraftTable::markFetchSuccess(millis());

                        AircraftTable::lock();
                        memcpy(AircraftTable::raw(), tempTable,
                               sizeof(Aircraft) * Config::MAX_TRACKED_AIRCRAFT);
                        AircraftTable::postFetchUpdate(lat, lon);
                        // postFetchUpdate() schreibt seine "letzter bekannter
                        // Wert"-Felder (proximityZone, NEU auch
                        // prevAirportDistKm/approachLikely/approachEtaMin fuer
                        // die Best-Effort-Anflug-Erkennung, siehe aircraft.h)
                        // NUR in AircraftTable::raw() - tempTable bekommt das
                        // ohne diese Rueckkopie nie zu sehen und wuerde beim
                        // naechsten Zyklus wieder mit dem alten (bzw. fuer
                        // prevAirportDistKm: dem Default-)Wert ueberschrieben,
                        // sobald memcpy() oben erneut komplett von tempTable
                        // nach raw() kopiert. proximityZone "ueberlebt" das in
                        // der Praxis nur, weil radar_screen.cpp::
                        // updateProximityAlert() denselben raw()-Speicher
                        // zusaetzlich viel haeufiger (jeden UI-Tick auf Core 1)
                        // direkt liest/schreibt, ausserhalb dieses Zyklus -
                        // prevAirportDistKm hat kein solches Aequivalent
                        // (postFetchUpdate() ist die einzige Stelle, die es
                        // liest/schreibt) und muss deshalb explizit
                        // zurueckkopiert werden, sonst wuerde "Distanz zum
                        // Flughafen sinkt ueber die letzten Zyklen" nie
                        // erkannt (mit einer TESTWEISE simulierten, stetig
                        // sinkenden Distanz bestaetigt und behoben).
                        memcpy(tempTable, AircraftTable::raw(),
                               sizeof(Aircraft) * Config::MAX_TRACKED_AIRCRAFT);

                        AircraftTable::unlock();

                        FlightLogbook::update();

                        // MQTT-Statuswerte (SettingsStore::mqttEnabled(),
                        // siehe mqtt_client.h) - dieselben drei Kennzahlen,
                        // die auch die LED-Alarme steuern (Naeherungs-/
                        // Watchlist-Alarm, siehe radar_screen.cpp::
                        // updateProximityAlert()), hier unabhaengig auf
                        // Core 0 aus der frisch aktualisierten
                        // AircraftTable neu berechnet - MqttClient::
                        // publishStatus() selbst prueft mqttEnabled() und
                        // den Verbindungsstatus, ist also auch bei
                        // ausgeschaltetem MQTT gefahrlos aufrufbar (reiner
                        // No-Op).
                        {
                            bool proximityOn = SettingsStore::proximityAlertEnabled();
                            bool anyWatched = false;
                            bool anyClose = false;
                            AircraftTable::lock();
                            Aircraft* table = AircraftTable::raw();
                            uint8_t aircraftCount = AircraftTable::validCount();
                            for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
                                if (!table[i].valid) continue;
                                if (AircraftWatchlist::isWatched(table[i].callsign) ||
                                    SquawkWatchlist::isWatched(table[i].squawk)) {
                                    anyWatched = true;
                                }
                                if (proximityOn && table[i].distanceKm <= Config::LED_ALERT_RADIUS_KM) {
                                    anyClose = true;
                                }
                            }
                            AircraftTable::unlock();
                            MqttClient::publishStatus(aircraftCount, anyWatched, anyClose);
                        }

                        // Kurzer gruener LED-Blitz als "Herzschlag" - zeigt,
                        // dass gerade eine Abfrage gelaufen ist. Wird von
                        // LedAlert::update() automatisch ignoriert, solange
                        // ein Naeherungs-/Notfall-Alarm aktiv ist.
                        if (SettingsStore::ledHeartbeatEnabled()) {
                            LedAlert::pulseHeartbeat(millis());
                        }

                        // TESTWEISE - nach Erfolg schrittweise (nicht
                        // abrupt) zum Grundintervall zurueckkehren, falls
                        // zuvor wegen 429/Fehlern hochskaliert wurde -
                        // halbiert bei jedem weiteren Erfolg die
                        // verbleibende Differenz zum Grundintervall.
                        if (currentIntervalMs > Config::FETCH_INTERVAL_MS) {
                            uint32_t excess = currentIntervalMs - Config::FETCH_INTERVAL_MS;
                            currentIntervalMs = (excess < 1000)
                                ? Config::FETCH_INTERVAL_MS
                                : Config::FETCH_INTERVAL_MS + excess / 2;
                        }
                    } else if (result.httpCode == 429) {
                        // TESTWEISE - exponentielles Backoff nach HTTP 429:
                        // Retry-After-Header bevorzugen, falls vorhanden,
                        // sonst Intervall verdoppeln - jeweils gedeckelt bei
                        // FETCH_BACKOFF_MAX_MS.
                        uint32_t suggested = (result.retryAfterSec >= 0)
                            ? (uint32_t)result.retryAfterSec * 1000UL
                            : currentIntervalMs * 2;
                        currentIntervalMs = min(max(suggested, Config::FETCH_INTERVAL_MS),
                                                 Config::FETCH_BACKOFF_MAX_MS);
                        Serial.printf("[NetTask] HTTP 429 - naechste Abfrage in %lums (retryAfterSec=%d)\n",
                                      (unsigned long)currentIntervalMs, result.retryAfterSec);
                    } else {
                        // TESTWEISE - einzelner sonstiger Fehlschlag
                        // (Timeout/SSL/...): feste moderate Wartezeit statt
                        // sofort wieder im Grundintervall weiterzumachen.
                        currentIntervalMs = max(currentIntervalMs, Config::FETCH_RETRY_DELAY_MS);
                        Serial.printf("[NetTask] Abfrage fehlgeschlagen (HTTP %d), naechster Versuch in %lums\n",
                                      result.httpCode, (unsigned long)currentIntervalMs);
                    }

                    netTaskIdle.store(true, std::memory_order_release);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void begin() {
    xTaskCreatePinnedToCore(
        taskFunc,
        "NetTask",
        20480,
        nullptr,
        1,
        &taskHandle,
        0
    );
}

bool pause(uint32_t timeoutMs) {
    if (!taskHandle) return true;

    // Aktiv warten, bis NetTask sich selbst als idle meldet (siehe
    // netTaskIdle oben), STATT sofort zu suspendieren - ein vTaskSuspend()
    // mitten in einer laufenden ADS-B-Netzwerkoperation wuerde erst am
    // naechsten Yield-/Blockierpunkt greifen (bis zu
    // Config::ADSB_HTTP_TIMEOUT_MS = 15s spaeter), waehrenddessen liefe ein
    // gleichzeitiger OTA-Download auf Core 1 tatsaechlich parallel dazu und
    // koennte um Heap-/TLS-Ressourcen konkurrieren. Laeuft im Aufrufer-
    // Kontext (Core 1, z.B. menu_screen.cpp), daher normales delay() statt
    // vTaskDelay - blockiert absichtlich die UI, da der OTA-Screen ohnehin
    // "Suche nach Update..." anzeigt und auf eine Antwort wartet.
    uint32_t start = millis();
    while (!netTaskIdle.load(std::memory_order_acquire)) {
        if (millis() - start >= timeoutMs) {
            return false;
        }
        delay(20);
    }

    vTaskSuspend(taskHandle);
    return true;
}

void resume() {
    if (taskHandle) vTaskResume(taskHandle);
}

}