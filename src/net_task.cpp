#include "net_task.h"
#include "config.h"
#include "wifi_manager.h"
#include "location_manager.h"
#include "adsb_client.h"
#include "aircraft_table.h"
#include "aircraft.h"
#include "settings_store.h"
#include "aircraft_details.h"
#include "previously_seen.h"
#include "flight_logbook.h"
#include "led_alert.h"
#include "web_export_server.h"
#include "weather.h"
#include "ota_update.h"
#include "iss_tracker.h"
#include "ntfy_push.h"
#include "mqtt_client.h"
#include "aircraft_watchlist.h"
#include "squawk_watchlist.h"
#include "radar_screen.h"
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
    // NetTask NICHT mitten in EINER der Netzwerkoperationen dieser
    // Schleifeniteration steckt (also "sicher" fuer eine Suspendierung) -
    // umspannt seit dem Bugfix (Alex' Meldung: OTA-Download durch spaeter
    // hinzugekommene Hintergrund-Netzwerkaufrufe verlangsamt) die GESAMTE
    // Schleifeniteration (WifiMgr/LocationManager/WebExportServer/
    // AircraftDetails/PreviouslySeen/Weather/IssTracker/OTA-Hintergrund-
    // check/MQTT/ADS-B-Abruf), nicht mehr nur AdsbClient::fetch() -
    // false ganz am Schleifenanfang, true erst unmittelbar vor dem
    // 50ms-Schlaf am Ende. Frueher deckte es NUR den ADS-B-Abruf ab, jeder
    // andere, spaeter hinzugekommene Netzwerkaufruf in dieser Schleife
    // galt faelschlich als "idle" - pause() (siehe unten) konnte dadurch
    // mitten in einem dieser Aufrufe vTaskSuspend() ausloesen, ohne dass
    // die betroffene WiFiClient(Secure)-Verbindung je sauber geschlossen
    // wurde (das Stack-Objekt friert einfach mitten in der Funktion ein) -
    // diese verwaiste, aber technisch weiterhin offene Verbindung wurde
    // von den system­eigenen WLAN-/lwIP-Tasks (von vTaskSuspend() NICHT
    // betroffen) fuer den Rest des Suspendierungs-Zeitraums am Leben
    // gehalten und kostete dabei durchgehend WLAN-Bandbreite, die einem
    // gleichzeitigen OTA-Download fehlte. pause() unten wartet aktiv auf
    // true, bevor es wirklich suspendiert - siehe net_task.h fuer die
    // ausfuehrliche Begruendung.
    std::atomic<bool> netTaskIdle{true};

    Aircraft tempTable[Config::MAX_TRACKED_AIRCRAFT];

    bool webServerStarted = false;

    void taskFunc(void*) {
        MqttClient::init();

        for (;;) {
            // BUGFIX (Alex' Meldung: OTA-Download seit Weather-/MQTT-/ISS-/
            // Update-Hintergrundcheck & Co. spuerbar langsamer, obwohl
            // NetTask::pause() waehrend eines OTA-Downloads doch eigentlich
            // suspendiert werden sollte) - netTaskIdle war bisher NUR waehrend
            // AdsbClient::fetch() false (siehe historischer Kommentar beim
            // vorherigen Deklarationsort), alle spaeter hinzugekommenen
            // Netzwerkaufrufe in dieser Schleife (Weather::update(),
            // IssTracker::update(), OtaUpdate::pollBackground(),
            // MqttClient::loop(), WebExportServer::update(), WifiMgr::
            // update(), LocationManager::update()) meldeten sich nie als
            // "nicht idle". pause() (Core 1) konnte dadurch mitten in einem
            // DIESER Aufrufe vTaskSuspend() ausloesen - der zugehoerige
            // WiFiClient/WiFiClientSecure wird dabei NIE sauber geschlossen
            // (die Funktion friert einfach mitten in ihrer Ausfuehrung ein,
            // ihr Stack-Objekt wird nie destruiert), die offene TCP-/TLS-
            // Verbindung blieb danach fuer die GESAMTE Downloaddauer bestehen
            // und wurde von den systemeigenen WLAN-/lwIP-Tasks (die von
            // vTaskSuspend() NICHT betroffen sind) im Hintergrund weiter am
            // Leben gehalten - das kostete durchgehend WLAN-Bandbreite, die
            // dem eigentlichen OTA-Download fehlte. Jetzt umspannt
            // netTaskIdle=false/true die GESAMTE Schleifeniteration (alle
            // Netzwerkaufrufe), nicht mehr nur den ADS-B-Abruf - pause()
            // wartet dadurch immer auf eine wirklich sichere Luecke
            // (unmittelbar vor dem vTaskDelay(50) am Schleifenende), in der
            // KEINE Verbindung dieser Schleife mehr offen ist.
            netTaskIdle.store(false, std::memory_order_release);

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
            PreviouslySeen::update();
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
            // Optionale ntfy.sh-Push-Benachrichtigung (SettingsStore::
            // ntfyPushEnabled(), AUS per Default, siehe ntfy_push.h) - sendet
            // nur, wenn radar_screen.cpp::updateProximityAlert() (Core 1)
            // gerade eine Nachricht vorgemerkt hat, sonst kehrt update()
            // sofort zurueck. Gleiches "jede Schleife mit aufrufen"-Muster
            // wie MqttClient::loop() oben.
            NtfyPush::update();
            // Unabhaengig vom Erfolg der ADS-B-Abfrage weiter unten pruefen,
            // damit die 24h-Sicherheitsabschaltung des Flugbuchs auch waehrend
            // laengerer WLAN-/ADS-B-Ausfaelle zuverlaessig greift (siehe
            // flight_logbook.cpp::enforceAutoOff() fuer den Hintergrund).
            FlightLogbook::enforceAutoOff();

            // Alterung veralteter Flugzeuge (siehe AircraftTable::
            // ageOutStale()) laeuft bewusst hier, UNABHAENGIG vom Erfolg der
            // ADS-B-Abfrage weiter unten - sonst wuerden bei anhaltendem
            // Verbindungsverlust nie mehr entfernte Eintraege den
            // Naeherungsalarm auf einem eingefrorenen Zustand haengen
            // lassen (Alex' Meldung).
            AircraftTable::ageOutStale(millis());

            if (millis() - lastFetchMs >= currentIntervalMs) {
                lastFetchMs = millis();

                if (WifiMgr::getState() == WifiMgr::State::Connected) {
                    LocationManager::requestIpLookupIfNeeded();

                    double lat = 0, lon = 0;
                    LocationManager::getHomeLocation(lat, lon);

                    float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];

                    // Solange die WebUI-Livekarte gerade aktiv geoeffnet ist
                    // (siehe WebExportServer::isRadarUiActive()), auf einer
                    // groesseren Reichweitenstufe abfragen - sonst kann der
                    // Reichweiten-Waehler dort nie mehr Flugzeuge zeigen als
                    // am Geraet selbst gerade eingestellt ist (Flugzeuge
                    // ausserhalb der Geraete-Reichweite werden ja gar nicht
                    // erst abgefragt/gespeichert). Nur waehrend die WebUI
                    // tatsaechlich genutzt wird, um die zusaetzliche
                    // Netzwerk-/Speicherlast nicht dauerhaft allen Nutzern
                    // aufzuerlegen. Das Geraete-Display selbst filtert beim
                    // Zeichnen weiterhin unabhaengig auf seine eigene
                    // rangeIndex()-Reichweite (siehe radar_screen.cpp), zeigt
                    // also trotzdem nur die eingestellte Reichweite an.
                    //
                    // Historie (siehe CLAUDE.md/Chat): Diese automatische
                    // Eskalation loeste zwei getrennte, inzwischen behobene
                    // Bugs aus. (1) Bei 100km war die ADS-B-JSON-Antwort so
                    // gross (~85-95KB), dass deserializeJson() am dauerhaft
                    // begrenzten groessten zusammenhaengenden Heap-Block
                    // scheiterte ("Keine Verbindung") - behoben durch den
                    // speicherschonenden Streaming-Parser in adsb_client.cpp
                    // (parst Flugzeug-Objekte einzeln statt alle gleichzeitig
                    // im Speicher zu halten). (2) Danach zeigte sich: adsb.lol
                    // liefert Flugzeuge NICHT nach Entfernung sortiert, und
                    // die Tabellen-Befuellung brach beim Erreichen von
                    // Config::MAX_TRACKED_AIRCRAFT (40) einfach ab ("first
                    // come, first served") - bei 100km mit oft >200
                    // Flugzeugen in Reichweite konnten dadurch zufaellig
                    // gerade die NAHEN Flugzeuge fehlen, die die eigene
                    // Geraete-Anzeige eigentlich zeigen sollte. Behoben durch
                    // eine laufende "naechste 40"-Auswahl in adsb_client.cpp
                    // (ersetzt bei voller Tabelle den jeweils am weitesten
                    // entfernten Eintrag durch ein naeheres Flugzeug). Beide
                    // Fixes live verifiziert (100% Erfolgsrate ueber 11
                    // Minuten bei 100km bzw. korrekte Naechste-40-Auswahl
                    // synthetisch UND unter echtem Web-UI-Verkehr) - die
                    // Deckelung auf 50km war fuer BEIDE Probleme nur eine
                    // Notloesung und ist damit nicht mehr noetig.
                    constexpr float WEB_UI_MAX_AUTO_RANGE_KM = 100.0f;
                    if (webServerStarted && WebExportServer::isRadarUiActive() &&
                        rangeKm < WEB_UI_MAX_AUTO_RANGE_KM) {
                        rangeKm = WEB_UI_MAX_AUTO_RANGE_KM;
                    }

                    // netTaskIdle ist bereits seit Schleifenbeginn false
                    // (siehe dortiger Kommentar) - kein erneutes Setzen hier
                    // noetig.
                    uint32_t fetchStartMs = millis();
                    auto result = AdsbClient::fetch(lat, lon, rangeKm,
                                                     tempTable, Config::MAX_TRACKED_AIRCRAFT);
                    uint32_t fetchDurationMs = millis() - fetchStartMs;

                    // Feature 5 "Verbindungsqualitaet" - bei JEDEM
                    // Abrufversuch aktualisiert, unabhaengig vom Ergebnis
                    // (anders als AircraftTable::markFetchSuccess() unten,
                    // das nur bei Erfolg laeuft). fetchDurationMs zusaetzlich
                    // fuer den System-Status-Screen (system_status_screen.cpp).
                    AircraftTable::recordFetchOutcome(result.ok, result.httpCode, fetchDurationMs);

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

                        uint8_t validAircraftCount = AircraftTable::validCount();

                        AircraftTable::unlock();

                        FlightLogbook::update();

                        // "Peak Traffic" (Tages-Hoechstwert gleichzeitig
                        // sichtbarer Flugzeuge, siehe flight_logbook.h) -
                        // bewusst UNABHAENGIG von FlightLogbook::update()
                        // oben, das nur bei eingeschaltetem Flugbuch
                        // ueberhaupt etwas tut (siehe dortiges
                        // checkAutoOff()) - der Hoechstwert soll immer
                        // mitlaufen.
                        FlightLogbook::updatePeakTraffic(validAircraftCount);

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
                            bool militaryOn = SettingsStore::militarySquawkDetectionEnabled();
                            bool emergencyOn = SettingsStore::emergencyAlertEnabled();
                            bool anyWatched = false;
                            bool anyClose = false;

                            // Feature 14 "MQTT/Home Assistant erweitern" -
                            // IM SELBEN Durchlauf wie anyWatched/anyClose
                            // oben mitberechnet (keine zweite, separate
                            // Aggregation ueber die AircraftTable, siehe
                            // MqttClient::TrafficStats). Nutzt dieselben
                            // RadarScreen-Huellfunktionen wie der Live-
                            // Traffic-Screen (radar_screen.h) statt die
                            // Typ-/Squawk-Klassifikation hier ein zweites
                            // Mal zu implementieren.
                            MqttClient::TrafficStats traffic;

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

                                if (!traffic.hasNearest || table[i].distanceKm < traffic.nearestKm) {
                                    traffic.hasNearest = true;
                                    traffic.nearestKm = table[i].distanceKm;
                                }
                                if (!traffic.hasHighest || table[i].altBaroFt > traffic.highestFt) {
                                    traffic.hasHighest = true;
                                    traffic.highestFt = table[i].altBaroFt;
                                }
                                if (!traffic.hasLowest || table[i].altBaroFt < traffic.lowestFt) {
                                    traffic.hasLowest = true;
                                    traffic.lowestFt = table[i].altBaroFt;
                                }
                                if (!traffic.hasFastest || table[i].groundSpeedKt > traffic.fastestKt) {
                                    traffic.hasFastest = true;
                                    traffic.fastestKt = table[i].groundSpeedKt;
                                }

                                if (RadarScreen::isRotorcraftCategory(table[i].category)) traffic.helicopters++;
                                if (RadarScreen::isHeavyAircraftCategory(table[i].category)) traffic.heavy++;
                                if (militaryOn && RadarScreen::isMilitaryGovSquawkCode(table[i].squawk)) {
                                    traffic.militaryDetected = true;
                                }
                                if (emergencyOn && RadarScreen::isEmergencySquawkCode(table[i].squawk)) {
                                    traffic.emergencyDetected = true;
                                }
                            }
                            AircraftTable::unlock();
                            MqttClient::publishStatus(aircraftCount, anyWatched, anyClose, traffic);
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
                }
            }

            // Erst hier, unmittelbar vor dem Schlafen, wirklich idle - siehe
            // ausfuehrlichen Kommentar am Schleifenanfang: deckt jetzt ALLE
            // Netzwerkaufrufe dieser Iteration ab, nicht mehr nur den
            // ADS-B-Abruf.
            netTaskIdle.store(true, std::memory_order_release);
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