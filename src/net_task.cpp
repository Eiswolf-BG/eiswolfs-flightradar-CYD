#include "net_task.h"
#include "config.h"
#include "wifi_manager.h"
#include "location_manager.h"
#include "adsb_client.h"
#include "aircraft_table.h"
#include "aircraft.h"
#include "settings_store.h"
#include "aircraft_details.h"
#include "route_watchlist.h"
#include "previously_seen.h"
#include "flight_logbook.h"
#include "led_alert.h"
#include "web_export_server.h"
#include "weather.h"
#include "ota_update.h"
#include "sd_storage.h"
#include "ntfy_push.h"
#include "mqtt_client.h"
#include "aircraft_watchlist.h"
#include "squawk_watchlist.h"
#include "type_watchlist.h"
#include "watchlist_alert.h"
#include "radar_screen.h"
#include "live_traffic_screen.h"
#include "airline_filter.h"
#include "i18n.h"
#include <Arduino.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <atomic>

namespace NetTask {

namespace {
    TaskHandle_t taskHandle = nullptr;
    uint32_t lastFetchMs = 0;

    // Drei neue, taeglich zuruecksetzende ntfy-Event-Merker (Alex' Auftrag,
    // "Grok"-Ideenliste) - gleiches Tageswechsel-Muster wie DailySightings/
    // FlightLogbook::updatePeakTraffic() (String-Datumsvergleich statt
    // eigener Kalenderarithmetik).
    char dailyFirstsDay[11] = {0};
    bool militaryNotifiedToday = false;
    bool heavyNotifiedToday = false;

    // "Peak Traffic erreicht" - letzter BEKANNTER Hoechststand, um einen
    // NEUEN Rekord zu erkennen (FlightLogbook::todayPeakTraffic() selbst
    // liefert kein "neu"-Signal).
    uint16_t lastKnownPeakTraffic = 0;

    // "Logbook-Auto-Off in X Stunden" - verhindert Mehrfachversand,
    // solange die verbleibende Zeit unter der Warnschwelle bleibt.
    bool logbookAutoOffNotified = false;

    // TESTWEISE - adaptives ADS-B-Abfrageintervall (siehe Absprache mit
    // Karl, Reaktion auf vereinzelte HTTP 429 von adsb.lol): startet bei
    // Config::FETCH_INTERVAL_MS, verdoppelt sich nach einem 429 (oder
    // uebernimmt dessen Retry-After-Wert), gedeckelt bei
    // Config::FETCH_BACKOFF_MAX_MS, und kehrt nach jeder erfolgreichen
    // Abfrage schrittweise (nicht abrupt) zum Grundintervall zurueck. Bei
    // einem einzelnen sonstigen Fehlschlag (Timeout/SSL) wird stattdessen
    // IMMER die feste Config::FETCH_RETRY_DELAY_MS gewartet (ein
    // zwischenzeitlich eingefuehrtes, bei MEHREREN Fehlschlaegen in Folge
    // eskalierendes Backoff wurde als Ursache fuer Alex' "Verbindung sofort
    // weg"-Meldung identifiziert und wieder auf dieses einfache, in v6.5.5
    // bewaehrte Verhalten zurueckgesetzt).
    uint32_t currentIntervalMs = Config::FETCH_INTERVAL_MS;

    // Von NetTask (Core 0) geschrieben, von pause() (Core 1, siehe unten)
    // gelesen - std::atomic statt eines ungeschuetzten bool, gleiches
    // Muster wie beim Heartbeat-Race-Fix in led_alert.cpp. true, solange
    // NetTask NICHT mitten in EINER der Netzwerkoperationen dieser
    // Schleifeniteration steckt (also "sicher" fuer eine Suspendierung) -
    // umspannt seit dem Bugfix (Alex' Meldung: OTA-Download durch spaeter
    // hinzugekommene Hintergrund-Netzwerkaufrufe verlangsamt) die GESAMTE
    // Schleifeniteration (WifiMgr/LocationManager/WebExportServer/
    // AircraftDetails/PreviouslySeen/Weather/OTA-Hintergrund-
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
            // BUGFIX (Alex' Meldung: OTA-Download seit Weather-/MQTT-/
            // Update-Hintergrundcheck & Co. spuerbar langsamer, obwohl
            // NetTask::pause() waehrend eines OTA-Downloads doch eigentlich
            // suspendiert werden sollte) - netTaskIdle war bisher NUR waehrend
            // AdsbClient::fetch() false (siehe historischer Kommentar beim
            // vorherigen Deklarationsort), alle spaeter hinzugekommenen
            // Netzwerkaufrufe in dieser Schleife (Weather::update(),
            // OtaUpdate::pollBackground(),
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
            // Route-Watchlist (SettingsStore::routeWatchlistAlertEnabled(),
            // AUS per Default, siehe route_watchlist.h) - kuemmert sich
            // intern selbst darum, hoechstens EINE Route pro Aufruf zu
            // ermitteln, nur fuer aktuell sichtbare Flugzeuge ohne bereits
            // abgeschlossenen Lookup, gleiches "jede Schleife mit
            // aufrufen"-Muster wie AircraftDetails::update() oben.
            RouteWatchlist::pollBackground();
            PreviouslySeen::update();
            Weather::update();
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
                    // Notloesung und war damit eine Zeit lang nicht mehr
                    // noetig.
                    //
                    // ERNEUT gedeckelt (01.10., siehe CLAUDE.md "Bekannte
                    // Probleme" - echter ESP32-Task-Watchdog-Absturz bei
                    // 100km+Web-Livekarte): Live reproduziert, dass bei
                    // 100km + vielen Flugzeugen + gleichzeitiger
                    // Hintergrundlast ein echter Watchdog-Reset auftreten
                    // kann, weil die Streaming-Parse-Schleife in
                    // AdsbClient::fetch() an keiner Stelle yieldet (dort
                    // bislang unveraendert, ein gezielter vTaskDelay()-Fix
                    // dort wurde bewusst NICHT versucht, siehe Begruendung
                    // im IncompleteInput-Eintrag in CLAUDE.md). Auf Alex'
                    // Wunsch 75km statt 50km als Kompromiss zwischen
                    // Stabilitaet und Web-UI-Reichweite - noch nicht
                    // abschliessend live verifiziert, ob 75km bereits
                    // ausreichend kleiner ist als die problematischen
                    // 100km. Pragmatische Notloesung, bis die eigentliche
                    // Parse-Schleife robust gemacht ist - KEIN
                    // vollstaendiger Fix der Ursache.
                    constexpr float WEB_UI_MAX_AUTO_RANGE_KM = 75.0f;
                    // ACHTUNG: Nicht nur nach OBEN eskalieren (rangeKm <
                    // Cap), sondern bei aktiver WebUI auch nach UNTEN
                    // deckeln - sonst bleibt bei manuell am Geraet
                    // eingestelltem 100km (rangeKm bereits >= Cap) die
                    // Abfrage trotz Deckelung bei 100km, genau der
                    // Praxisfall, der den Watchdog-Crash ausloest (Alex'
                    // Live-Test 01.10.: "kann immernoch auf 100km, sowohl
                    // live radar als auch Geraet" - die reine
                    // "< Cap"-Bedingung von vorher griff dabei nicht).
                    if (webServerStarted && WebExportServer::isRadarUiActive()) {
                        rangeKm = WEB_UI_MAX_AUTO_RANGE_KM;
                    }

                    // BUGFIX (Alex' Meldung: "Flight Stories" schickte trotz
                    // 10-Minuten-Wiederholungssperre alle ~8s erneut dieselbe
                    // Meldung): tempTable wurde bisher nur EINMAL direkt nach
                    // einem erfolgreichen Abruf aus AircraftTable::raw()
                    // resynchronisiert (siehe memcpy() weiter unten,
                    // Kommentar bei "proximityZone 'ueberlebt' das..."). Mit
                    // lastFlightStoryMs schreibt radar_screen.cpp::
                    // updateProximityAlert() (Core 1) aber oft ERST NACH
                    // diesem Zeitpunkt in die Live-Tabelle - also NACH dem
                    // Resync, aber VOR dem naechsten Abruf. Der naechste
                    // AdsbClient::fetch()-Aufruf schnappschoss dadurch
                    // tempTable (siehe PrevFlightStory dort) IMMER im alten,
                    // noch-nicht-gesetzten Zustand. Frische Resync direkt vor
                    // JEDEM Abruf behebt das - erfasst garantiert auch
                    // Core-1-Schreibzugriffe aus der Luecke seit dem letzten
                    // Zyklus.
                    {
                        AircraftTable::lock();
                        memcpy(tempTable, AircraftTable::raw(),
                               sizeof(Aircraft) * Config::MAX_TRACKED_AIRCRAFT);
                        AircraftTable::unlock();
                    }

                    // netTaskIdle ist bereits seit Schleifenbeginn false
                    // (siehe dortiger Kommentar) - kein erneutes Setzen hier
                    // noetig.
                    Serial.printf("[DIAG] fetch rangeKm=%.0f webUiActive=%d\n", rangeKm, WebExportServer::isRadarUiActive() ? 1 : 0);
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

                    // Kurzer Yield-Punkt (Alex' Meldung: Verbindungsverlust/
                    // Watchdog-Absturz bei 100km + geoeffneter Web-Livekarte) -
                    // Alex' Meldung: Watchdog-Absturz (ESP32 Task-Watchdog,
                    // echter abort()+Auto-Reboot) bei 100km Reichweite
                    // waehrend die Web-Livekarte offen ist - siehe
                    // ausfuehrlicher Eintrag in CLAUDE.md "Bekannte
                    // Probleme". Dieser vTaskDelay(1) hier war ein erster
                    // Versuch, dem Scheduler zwischen Abruf und der neuen
                    // Nachbearbeitung dieses Batches eine Gelegenheit zu
                    // geben - hat den Absturz in einem WIEDERHOLTEN Testlauf
                    // NICHT verhindert (trat erneut auf, diesmal mit
                    // "NetTask" statt "wifi" als blockiertem Task). Der
                    // eigentliche Sitz des Problems liegt hoechstwahrschein-
                    // lich in der Streaming-Parse-Schleife in
                    // adsb_client.cpp::fetch() (dort KEIN einziger Yield-
                    // Punkt, unveraendert seit vor diesem Batch) - bewusst
                    // trotzdem hier stehen gelassen (kann nicht schaden),
                    // aber NICHT als verifizierter Fix zu verstehen.
                    vTaskDelay(1);

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

                        // Verkehrstrend-Sample (Alex' Auftrag, siehe
                        // live_traffic_screen.cpp::recordTrendSample()) -
                        // bewusst HIER nach jedem erfolgreichen ADS-B-Abruf
                        // auf Core 0 aufgenommen, UNABHAENGIG davon, ob der
                        // Live-Traffic-Screen gerade geoeffnet ist, damit
                        // der ~15-30-Minuten-Trend auch im Hintergrund
                        // weiterlaeuft. Zaehlt mit DENSELBEN Filtern
                        // (RadarScreen::isAircraftCurrentlyVisible(), noch
                        // unter dem oben gehaltenen Lock) wie die Gesamtzahl
                        // auf dem Live-Traffic-Screen selbst, statt einfach
                        // validAircraftCount zu uebernehmen - sonst wuerde
                        // der Trend z.B. bei aktivem "Nur Helikopter"-Filter
                        // von einer anderen Grundmenge ausgehen als die
                        // Anzeige, die er beschreibt.
                        {
                            uint16_t visibleNow = 0;
                            Aircraft* rawTable = AircraftTable::raw();
                            for (uint8_t ti = 0; ti < AircraftTable::capacity(); ti++) {
                                if (!rawTable[ti].valid) continue;
                                if (RadarScreen::isAircraftCurrentlyVisible(rawTable[ti])) visibleNow++;
                            }
                            LiveTrafficScreen::recordTrendSample(visibleNow);
                        }

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
                                if (WatchlistAlert::isHit(table[i])) {
                                    anyWatched = true;
                                }
                                if (proximityOn && table[i].distanceKm <= Config::LED_ALERT_RADIUS_KM) {
                                    anyClose = true;
                                }

                                if (!traffic.hasNearest || table[i].distanceKm < traffic.nearestKm) {
                                    traffic.hasNearest = true;
                                    traffic.nearestKm = table[i].distanceKm;
                                    // Flugphase des naechstgelegenen Flugzeugs
                                    // (Alex' Auftrag) - genau an diesem Punkt
                                    // erfasst, da table[i] hier gerade DAS
                                    // naechstgelegene Flugzeug ist.
                                    const char* phase = RadarScreen::flightPhaseLabelFor(table[i]);
                                    strncpy(traffic.nearestPhase, phase, sizeof(traffic.nearestPhase) - 1);
                                    traffic.nearestPhase[sizeof(traffic.nearestPhase) - 1] = 0;
                                }
                                // Naechste Ueberflug-ETA (Alex' Auftrag) - das
                                // insgesamt kleinste cpaEtaMin ueber ALLE
                                // gerade relevanten Flugzeuge (nicht
                                // notwendigerweise das naechstgelegene), siehe
                                // aircraft_table.cpp::postFetchUpdate() fuer
                                // cpaRelevant/cpaEtaMin.
                                if (table[i].cpaRelevant &&
                                    (!traffic.hasNextOverflight || table[i].cpaEtaMin < traffic.nextOverflightEtaMin)) {
                                    traffic.hasNextOverflight = true;
                                    traffic.nextOverflightEtaMin = table[i].cpaEtaMin;
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

                            // Wetter-Code/GPS-Fix/gefilterte-Airlines (Alex'
                            // Auftrag) - alle drei brauchen KEINEN eigenen
                            // AircraftTable-Zugriff, deshalb bewusst erst
                            // HIER nach dem unlock() oben ermittelt.
                            switch (Weather::current()) {
                                case Weather::Condition::Clear:        strncpy(traffic.weatherCode, "clear", sizeof(traffic.weatherCode) - 1); break;
                                case Weather::Condition::PartlyCloudy: strncpy(traffic.weatherCode, "partly_cloudy", sizeof(traffic.weatherCode) - 1); break;
                                case Weather::Condition::Cloudy:       strncpy(traffic.weatherCode, "cloudy", sizeof(traffic.weatherCode) - 1); break;
                                case Weather::Condition::Rain:         strncpy(traffic.weatherCode, "rain", sizeof(traffic.weatherCode) - 1); break;
                                case Weather::Condition::Snow:         strncpy(traffic.weatherCode, "snow", sizeof(traffic.weatherCode) - 1); break;
                                case Weather::Condition::Thunderstorm: strncpy(traffic.weatherCode, "thunderstorm", sizeof(traffic.weatherCode) - 1); break;
                                case Weather::Condition::Unknown:
                                default:                                strncpy(traffic.weatherCode, "unknown", sizeof(traffic.weatherCode) - 1); break;
                            }
                            // "GPS-Fix-Qualitaet" (Alex' Auftrag) - im Projekt
                            // ist aktuell nur ein einfaches Ja/Nein verfuegbar
                            // (LocationManager::hasGpsFix()), keine feinere
                            // HDOP-/Satelliten-Anzahl-Erfassung - daher als
                            // einfacher Binaer-Sensor umgesetzt statt einer
                            // erfundenen Detailstufe.
                            traffic.gpsFixAvailable = LocationManager::hasGpsFix();
                            traffic.filteredAirlineCount = AirlineFilter::count();

                            MqttClient::publishStatus(aircraftCount, anyWatched, anyClose, traffic);
                            vTaskDelay(1); // zweiter Yield-Punkt, siehe Kommentar oben

                            // Drei neue ntfy-Events (Alex' Auftrag) - jeweils
                            // einmalig pro Tag ausgeloest (eigene, lokal
                            // taeglich zuruecksetzende Merker, gleiches
                            // Tageswechsel-Muster wie FlightLogbook::
                            // updatePeakTraffic()/DailySightings), gated
                            // durch denselben SettingsStore::ntfyPushEnabled()/
                            // isQuietHoursActive()-Check wie alle bestehenden
                            // ntfy-Versandstellen in radar_screen.cpp.
                            if (SettingsStore::ntfyPushEnabled() && !NtfyPush::isQuietHoursActive()) {
                                time_t nowEpoch = time(nullptr);
                                if (nowEpoch > 8 * 3600 * 2) {
                                    struct tm tmNow;
                                    localtime_r(&nowEpoch, &tmNow);
                                    char today[11];
                                    snprintf(today, sizeof(today), "%04d-%02d-%02d",
                                             tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);
                                    if (strcmp(today, dailyFirstsDay) != 0) {
                                        strncpy(dailyFirstsDay, today, sizeof(dailyFirstsDay) - 1);
                                        dailyFirstsDay[sizeof(dailyFirstsDay) - 1] = 0;
                                        militaryNotifiedToday = false;
                                        heavyNotifiedToday = false;
                                    }
                                    if (traffic.militaryDetected && !militaryNotifiedToday) {
                                        militaryNotifiedToday = true;
                                        NtfyPush::request(I18n::t(StringId::NTFY_PUSH_MSG_FIRST_MILITARY));
                                    }
                                    if (traffic.heavy > 0 && !heavyNotifiedToday) {
                                        heavyNotifiedToday = true;
                                        NtfyPush::request(I18n::t(StringId::NTFY_PUSH_MSG_FIRST_HEAVY));
                                    }
                                }

                                // "Peak Traffic erreicht" - vergleicht gegen
                                // den zuletzt BEKANNTEN Hoechststand (lokaler
                                // Merker, da FlightLogbook::updatePeakTraffic()
                                // selbst kein "neuer Rekord"-Signal liefert,
                                // siehe dortiger Kommentar) - erkennt so auch
                                // einen Tageswechsel automatisch (der neue Tag
                                // startet bei 0, der naechste Zyklus mit
                                // >0 Flugzeugen ist dann automatisch wieder
                                // ein "neuer" Peak).
                                FlightLogbook::PeakTraffic peak = FlightLogbook::todayPeakTraffic();
                                if (peak.count > lastKnownPeakTraffic) {
                                    lastKnownPeakTraffic = peak.count;
                                    if (lastKnownPeakTraffic > 0) {
                                        char msg[64];
                                        snprintf(msg, sizeof(msg), "%s%u", I18n::t(StringId::NTFY_PUSH_MSG_PEAK_TRAFFIC_PREFIX),
                                                 (unsigned)peak.count);
                                        NtfyPush::request(msg);
                                    }
                                } else if (peak.count < lastKnownPeakTraffic) {
                                    // Tageswechsel (Peak wurde zurueckgesetzt) -
                                    // Merker mitziehen, OHNE eine Meldung zu
                                    // senden (0 ist kein neuer Rekord).
                                    lastKnownPeakTraffic = peak.count;
                                }

                                // "Logbook-Auto-Off in X Stunden" - einmalig
                                // ausgeloest, wenn die verbleibende Zeit zum
                                // ersten Mal unter die Schwelle faellt (siehe
                                // FlightLogbook::secondsUntilAutoOff()).
                                // logbookAutoOffNotified wird zurueckgesetzt,
                                // sobald wieder MEHR Zeit uebrig ist als die
                                // Schwelle (z.B. Flugbuch wurde zwischenzeitlich
                                // erneut eingeschaltet) - sonst wuerde nach
                                // einem Wiedereinschalten nie erneut gewarnt.
                                constexpr int32_t AUTO_OFF_WARN_SECONDS = 3600; // 1 Stunde
                                int32_t remaining = FlightLogbook::secondsUntilAutoOff();
                                if (remaining < 0 || remaining > AUTO_OFF_WARN_SECONDS) {
                                    logbookAutoOffNotified = false;
                                } else if (!logbookAutoOffNotified) {
                                    logbookAutoOffNotified = true;
                                    NtfyPush::request(I18n::t(StringId::NTFY_PUSH_MSG_LOGBOOK_AUTO_OFF));
                                }
                            }
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
                        // Einzelner sonstiger Fehlschlag (Timeout/SSL/...):
                        // IMMER feste Config::FETCH_RETRY_DELAY_MS, keine
                        // Eskalation - ein zwischenzeitlich eingefuehrtes
                        // exponentielles Backoff fuer generische Fehlschlaege
                        // wurde per Bisektion als Ursache fuer Alex'
                        // "Verbindung/Heartbeat sofort weg"-Meldung
                        // identifiziert (kurze Serien von 2-3 harmlosen
                        // Fehlschlagen liessen die naechste Chance auf einen
                        // Neuversuch immer weiter in die Zukunft rutschen)
                        // und wieder auf dieses einfache, in v6.5.5 bewaehrte
                        // Verhalten zurueckgesetzt. Der 429-Zweig oben
                        // (echtes Rate-Limiting durch adsb.lol, wo eine
                        // Eskalation tatsaechlich sinnvoll ist) bleibt davon
                        // unberuehrt.
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