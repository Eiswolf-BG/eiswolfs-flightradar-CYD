#pragma once
#include <Arduino.h>

namespace SettingsStore {
    void load();
    void save();

    uint8_t rangeIndex();
    void setRangeIndex(uint8_t idx);

    bool displayInverted();
    void setDisplayInverted(bool inverted);

    // 180-Grad-Drehung fuer Tischmontage (Menue > System > Anzeige) - das
    // TFT-Panel hat eingeschraenkte vertikale Blickwinkel, von oben
    // betrachtet "waschen" die Radarkreise sonst aus (GitHub-Meldung eines
    // Nutzers). Dreht Bild UND Touch-Mapping, siehe main.cpp/
    // touch_input.cpp. AUS per Default (normale Ausrichtung unveraendert).
    bool displayRotated180();
    void setDisplayRotated180(bool rotated);

    // Display-Helligkeit in Prozent (Config::BRIGHTNESS_MIN_PERCENT..MAX_PERCENT).
    uint8_t brightnessPercent();
    void setBrightnessPercent(uint8_t percent);

    // Auto-Helligkeit (Menue > System > Anzeige > Helligkeit, siehe
    // auto_brightness.h) - AUS per Default. Bei AN ersetzt der eingebaute
    // Lichtsensor (LDR an Config::LDR_PIN) die manuelle Prozent-Einstellung
    // (brightnessPercent() oben bleibt dabei unveraendert gespeichert, wird
    // nur voruebergehend nicht angewendet - beim Ausschalten springt die
    // Helligkeit sofort wieder auf den zuletzt manuell eingestellten Wert
    // zurueck, siehe main.cpp::normalBacklightPwm()).
    bool autoBrightnessEnabled();
    void setAutoBrightnessEnabled(bool on);

    bool emergencyAlertEnabled();
    void setEmergencyAlertEnabled(bool on);

    bool proximityAlertEnabled();
    void setProximityAlertEnabled(bool on);

    // AUS per Default = "Einfach" (fester Distanz-Schwellenwert,
    // Config::LED_ALERT_RADIUS_KM, unveraendertes bisheriges Verhalten). AN
    // = "Intelligent" (gestaffelte Gelb/Orange/Rot-Zonen, nur bei
    // Annaeherung, mit Hoehenfilter - siehe radar_screen.cpp::
    // updateProximityAlert()). Wirkt nur, wenn proximityAlertEnabled()
    // ebenfalls an ist - reine Auswahl DER Auswertungslogik, kein
    // zusaetzlicher Ein/Aus-Schalter.
    bool proximityAlertSmartMode();
    void setProximityAlertSmartMode(bool on);

    bool flightLogbookEnabled();
    void setFlightLogbookEnabled(bool on);

    // Unix-Zeitstempel (Sekunden), zu dem das Flugbuch zuletzt eingeschaltet
    // wurde. 0 = unbekannt/nicht gesetzt. FlightLogbook::update() nutzt dies,
    // um die Aufzeichnung nach genau 24 Stunden automatisch wieder
    // auszuschalten (SD-Karten-Schutz, siehe Bestaetigungsdialog im Menue).
    uint32_t flightLogbookEnabledAtEpoch();
    void setFlightLogbookEnabledAtEpoch(uint32_t epoch);

    // Dateiname (ohne ".csv", z.B. "2026-08-06" oder "2026-08-06_2") der
    // aktuell laufenden Flugbuch-Sitzung. "" = keine Sitzungsdatei
    // hinterlegt (Flugbuch aus, oder naechste Aktivierung soll eine neue
    // Datei anlegen). Siehe FlightLogbook::ensureSessionFile().
    String flightLogbookSessionFile();
    void setFlightLogbookSessionFile(const String& label);

    // Merkt sich, ob das Flugbuch zuletzt durch die 24h-Sicherheits-
    // abschaltung (siehe FlightLogbook::checkAutoOff()) ausgeschaltet wurde,
    // statt durch bewusstes manuelles Antippen des Schalters - AUS blieb
    // dadurch bisher voellig unbemerkt (Alex' Meldung: Flugbuch war
    // wochenlang unbemerkt aus). Steuert einen kleinen roten Hinweis-Punkt
    // an "Statistik & Logbuch" bzw. am Flugbuch-Schalter selbst (siehe
    // menu_screen.cpp), NUR wenn dieses Flag gesetzt ist UND das Flugbuch
    // aktuell aus ist - manuelles Aus-/Einschalten setzt das Flag immer
    // zurueck.
    bool flightLogbookAutoOffTriggered();
    void setFlightLogbookAutoOffTriggered(bool on);

    // "Peak Traffic" (siehe FlightLogbook::updatePeakTraffic()/
    // todayPeakTraffic(), Anzeige in stats_history_screen.cpp) - persistiert
    // ueber Geraete-Neustarts hinweg, damit ein Tages-Hoechstwert nicht
    // durch einen zwischenzeitlichen Neustart verloren geht. BEWUSST
    // unabhaengig vom Flugbuch-Ein/Aus-Schalter (anders als
    // flightLogbookSessionFile() oben) - der Hoechstwert soll unabhaengig
    // davon mitlaufen, ob gerade tatsaechlich in eine CSV-Datei geloggt
    // wird.
    uint16_t peakTrafficCount();
    void setPeakTrafficCount(uint16_t count);

    // Kalendertag ("YYYY-MM-DD"), zu dem peakTrafficCount() gehoert - dient
    // NUR der Tageswechsel-Erkennung (weicht das aktuelle Datum davon ab,
    // wird der Hoechstwert auf 0 zurueckgesetzt), wird nicht direkt
    // angezeigt.
    String peakTrafficDate();
    void setPeakTrafficDate(const String& date);

    // Unix-Zeitstempel des Moments, in dem der aktuelle Hoechstwert erreicht
    // wurde - 0 bedeutet "Uhrzeit war zu diesem Zeitpunkt noch nicht NTP-
    // synchronisiert" (gleiches Fallback-Prinzip wie Aircraft::
    // firstSeenEpoch) - die Anzeige laesst die Uhrzeit dann einfach weg,
    // statt eine falsche zu zeigen.
    uint32_t peakTrafficEpoch();
    void setPeakTrafficEpoch(uint32_t epoch);

    bool ledHeartbeatEnabled();
    void setLedHeartbeatEnabled(bool on);

    // AN per Default (gleiches Verhalten wie die anderen Alarm-Toggles auf
    // dem LED-Alerts-Screen) - schaltet den Web-Alarmton bei Watchlist-/
    // Notfall-Treffern auf der Live-Radar-Webseite komplett ein/aus (fuer
    // ALLE Betrachter). Der CYD selbst hat keinen brauchbaren Lautsprecher
    // (bereits getestet/verworfen) - der Ton laeuft stattdessen per Web
    // Audio API im Browser jedes Geraets, das die Webseite gerade offen
    // hat, siehe web_export_server.cpp.
    bool webAudioAlertEnabled();
    void setWebAudioAlertEnabled(bool on);

    uint8_t screenTimeoutMinutes();
    void setScreenTimeoutMinutes(uint8_t minutes);

    // Inaktivitaets-Timeout INNERHALB von Vollbild-Menues/Einstellungs-
    // Screens (siehe Config::MENU_IDLE_TIMEOUT_MIN/MAX_SECONDS in config.h
    // und menu_timeout_screen.cpp) - Sekunden, 0 = "Nie" (kein automatischer
    // Ruecksprung zum Radarscreen). Default 120s entspricht dem bisherigen
    // fest einprogrammierten Verhalten. menuIdleTimeoutMs() ist der
    // Convenience-Helfer, den alle Timeout-Check-Stellen im Projekt
    // verwenden (rechnet in Millisekunden um, 0 -> UINT32_MAX).
    uint16_t menuIdleTimeoutSeconds();
    void setMenuIdleTimeoutSeconds(uint16_t seconds);
    uint32_t menuIdleTimeoutMs();

    bool nightDimmingEnabled();
    void setNightDimmingEnabled(bool on);

    // Ruhebildschirm bei Inaktivitaets-Timeout (siehe main.cpp) - AUS per
    // Default, damit sich am bisherigen Verhalten (Backlight komplett aus)
    // nichts aendert, wer es nicht explizit einschaltet.
    bool screensaverEnabled();
    void setScreensaverEnabled(bool on);

    bool hideGroundVehicles();
    void setHideGroundVehicles(bool on);

    bool onlyHelicopters();
    void setOnlyHelicopters(bool on);

    // Filter "nur niedrig fliegende Flugzeuge" (Menue > Flugoptionen >
    // Tools) - zeigt nur Flugzeuge unterhalb der gruenen Hoehenschwelle
    // (Config::COLOR_LOW_ALT_THRESHOLD_FT), Bodenfahrzeuge ausgenommen
    // (siehe radar_screen.cpp). Gleiches Speicher-/Getter-/Setter-Muster
    // wie onlyHelicopters() oben.
    bool onlyLowAltitude();
    void setOnlyLowAltitude(bool on);

    // Filter "Nur Interessantes" (Alex' Wunsch, Menue > Flugoptionen >
    // Anzeigefilter, gleiche Stelle wie onlyHelicopters()/onlyLowAltitude()
    // oben) - zeigt nur Flugzeuge, auf die mindestens eine bereits
    // bestehende Erkennung zutrifft (Militaer-/Behoerdenflug, Notfall-
    // Squawk, Watchlist-Treffer, Heavy), siehe radar_screen.cpp::
    // isInterestingAircraft(). Gleiches Speicher-/Getter-/Setter-Muster wie
    // die beiden Geschwister oben, AUS per Default.
    bool onlyInteresting();
    void setOnlyInteresting(bool on);

    // Modus des Airline-Filters (airline_filter.h/.cpp) - false (Default)
    // = "Ausblenden" (bisheriges Verhalten: eingetragene Airlines werden
    // versteckt, alle anderen bleiben sichtbar), true = "Nur anzeigen"
    // (nur eingetragene Airlines sichtbar, alle anderen inkl. nicht
    // erkennbarer Airline ausgeblendet). Nutzt dieselbe Airline-Liste in
    // beiden Modi, keine zweite Liste.
    bool airlineFilterShowOnlyMode();
    void setAirlineFilterShowOnlyMode(bool showOnly);

    // Sprache der Benutzeroberflaeche: 0=EN,1=DE,2=FR,3=TR,4=ES,5=IT.
    uint8_t language();
    void setLanguage(uint8_t lang);

    // Einheiten-Modus: 0=Auto (per IP-Standort geschaetzt), 1=Metrisch
    // erzwingen, 2=Imperial (Fuss/Knoten/Meilen) erzwingen.
    uint8_t unitsMode();
    void setUnitsMode(uint8_t mode);

    // Flughafencode-Format fuer die Routenanzeige im Detail-Panel (Menue >
    // Land/Region > Einheiten) - AN (IATA, z.B. "FRA") per Default, bei
    // Aviation-Enthusiasten gelaeufiger als ICAO; AUS zeigt stattdessen den
    // 4-stelligen ICAO-Code (z.B. "EDDF"). Rein kosmetisch: nutzt
    // IATA-Codes, die die bestehende Routen-Lookup-Kette
    // (aircraft_details.cpp) ohnehin schon mitliefert, kein zusaetzlicher
    // API-Call. Faellt sauber auf ICAO zurueck, wenn fuer einen Flughafen
    // kein IATA-Code bekannt ist.
    bool useIataAirportCodes();
    void setUseIataAirportCodes(bool on);

    // Radar-Farbschema (Menue > System > Radar-Darstellung): 0=Gruen
    // (Standard), 1=Amber, 2=Blau - betrifft nur den Radar-Screen (Sweep-
    // Linie, Panel-Rahmen/Text, niedrig fliegende Flugzeuge), siehe
    // radar_screen.cpp::themeBaseColor().
    uint8_t radarThemeIndex();
    void setRadarThemeIndex(uint8_t idx);

    // Zwei unabhaengige, ankreuzbare Extras im selben Menue (radar_theme_
    // screen.cpp) - lassen sich mit JEDEM der drei Farbschemata oben
    // kombinieren, deshalb eigene Einstellungen statt weiterer Werte fuer
    // radarThemeIndex(). Beide AUS per Default (bewusste Zusatz-Optik, die
    // man selbst aktiviert). Siehe radar_screen.cpp::crtModeActive()/
    // Radar-Puls-Logik in render()/tick().
    bool crtPhosphorEnabled();
    void setCrtPhosphorEnabled(bool on);

    bool radarPulseEnabled();
    void setRadarPulseEnabled(bool on);

    // "Klassik-Radar" (System > Radar-Darstellung) - AUS per Default. Bei
    // AN: Kometenschweif hinter der Sweep-Linie (mehrere ausfadende
    // Segmente statt einer einzelnen Linie) sowie zusaetzliche, dezente
    // Rasterspeichen alle 30 Grad (die bestehenden N/S/E/W-Kreuzlinien
    // bleiben unveraendert). Rein kosmetisch, siehe radar_screen.cpp.
    bool classicRadarEnabled();
    void setClassicRadarEnabled(bool on);

    // "Militaer-/Behoerdenflug-Erkennung" (System > Radar-Darstellung) - AUS
    // per Default. Bei AN: Flugzeuge, deren aktueller Squawk-Code in einen
    // der bekannten, oeffentlich dokumentierten Militaer-/Behoerden-/
    // Sonderflug-Bereiche faellt (siehe MILITARY_SQUAWK_RANGES in
    // radar_screen.cpp), bekommen einen oranger Ring um den Marker - rein
    // visuell, kein Alarm/Ton. AUSDRUECKLICH Best-Effort ohne Garantie auf
    // Vollstaendigkeit/Korrektheit, siehe Hilfetext (StringId::
    // MILITARY_SQUAWK_INFO_BODY).
    bool militarySquawkDetectionEnabled();
    void setMilitarySquawkDetectionEnabled(bool on);

    // "Follow-Me Modus" (System > Radar-Darstellung) - AUS per Default,
    // gedacht fuer mobile Nutzung mit angeschlossenem GPS-Modul. Bei AN
    // dreht sich der Radarkreis zusaetzlich zur ohnehin schon GPS-
    // gestuetzten Zentrierung (siehe LocationManager::getHomeLocation(),
    // nutzt bei aktivem GPS-Fix und "Automatisch"-Standort bereits von
    // sich aus die Live-Position) an der aktuellen Fahrtrichtung aus
    // ("Heading-up" statt Norden oben) und zoomt bei hoeherer
    // Geschwindigkeit automatisch weiter heraus - siehe
    // RadarScreen::followMeRotationOffsetDeg()/followMeEffectiveRangeKm()
    // in radar_screen.cpp. Ohne gueltigen GPS-Kurs/-Fix faellt die Anzeige
    // sauber auf die normale, unrotierte Nordausrichtung zurueck.
    bool followMeModeEnabled();
    void setFollowMeModeEnabled(bool on);

    // "Performance-Auto-Tuning" (System > Radar-Darstellung) - AN per
    // Default. Bei AN reduziert PerfTuner (perf_tuner.h/.cpp) automatisch
    // und stufenweise die Render-Last (Wetter-Effekte -> Sweep-Animation ->
    // Flugzeug-Silhouetten-Detailgrad), sobald die ohnehin schon erfasste
    // Frame-Zeit/freeHeap/maxAlloc auf eine Ueberlastung hindeuten, und
    // normalisiert sich mit etwas Hysterese automatisch wieder. Bei AUS
    // bleibt PerfTuner dauerhaft auf Stufe 0 (kein Eingriff), unabhaengig
    // von den gemessenen Werten.
    bool perfAutoTuningEnabled();
    void setPerfAutoTuningEnabled(bool on);

    // Animierter Regen-Effekt (System > Radar-Darstellung) - AN per Default
    // (wird ohnehin nur sichtbar, wenn die Wetterdaten tatsaechlich Regen/
    // Gewitter zeigen, siehe radar_screen.cpp). Kurze, schraege Linien
    // ("Tropfen"), die als parallele Sehnen ueber den Radarkreis wandern -
    // Neigungswinkel folgt der tatsaechlichen Windrichtung (Weather::
    // currentWindDirectionDeg()), auch wenn das je nach Windrichtung wie
    // "nach oben regnen" aussehen kann (physikalisch korrekt, siehe
    // Hilfetext StringId::RAIN_EFFECT_INFO_BODY).
    bool rainEffectEnabled();
    void setRainEffectEnabled(bool on);

    // ISS-Marker-Bonusfeature (siehe iss_tracker.h) - AN per Default. Bei
    // AUS unterbleibt sowohl die periodische Positionsabfrage (kein
    // Netzwerk-Traffic) als auch das Zeichnen des Markers (siehe
    // IssTracker::update()/radar_screen.cpp).
    bool issMarkerEnabled();
    void setIssMarkerEnabled(bool on);

    // Steuert NUR das LED-Blinken bei verfuegbarem Update (dreimal kurz
    // Magenta, siehe radar_screen.cpp) - AN per Default (bisheriges
    // Verhalten). Der rote Punkt am "Nach Update suchen"-Button bleibt bei
    // AUS unveraendert bestehen, betrifft ausschliesslich das LED-Signal.
    bool updateLedSignalEnabled();
    void setUpdateLedSignalEnabled(bool on);

    // Steuert die Ereignis-Ecke unten rechts auf dem Radarschirm (Militaer-
    // /Behoerdenflug, Squawk-Wachposten, Rufzeichen-Watchlist, Airline-
    // Filter-Treffer - siehe radar_screen.cpp::drawEventCorner()), AN per
    // Default. Der Update-Indikator (Ausrufezeichen-Kreis, andere Ecke)
    // haengt NICHT an diesem Schalter.
    bool eventCornerOverlayEnabled();
    void setEventCornerOverlayEnabled(bool on);

    // Steuert den Weltkarten-Punktraster-Hintergrund unter dem Radarkreis
    // (radar_screen.cpp::drawWorldMap()), AN per Default (bisher
    // unbedingt gezeichnet - fuer bestehende Nutzer aendert sich optisch
    // nichts, bis jemand aktiv abschaltet).
    bool worldMapBackgroundEnabled();
    void setWorldMapBackgroundEnabled(bool on);

    // Optionale MQTT-Schnittstelle (siehe mqtt_client.h/mqtt_screen.cpp) -
    // AUS per Default. mqttBroker() liefert "host:port" als ein Feld (so
    // wie im Eingabe-Screen erfasst, siehe MqttScreen::run()) statt
    // getrennter Host-/Port-Felder - MqttClient::loop() zerlegt die
    // Zeichenkette selbst (letzter ":"), das spart ein zweites
    // Eingabefeld. Nutzername/Passwort duerfen leer bleiben (z.B. fuer
    // oeffentliche Test-Broker ohne Authentifizierung).
    bool mqttEnabled();
    void setMqttEnabled(bool on);
    String mqttBroker();
    void setMqttBroker(const String& hostPort);
    String mqttUsername();
    void setMqttUsername(const String& user);
    String mqttPassword();
    void setMqttPassword(const String& pass);

    // Optionale Push-Benachrichtigung ueber den kostenlosen, anmeldefreien
    // Dienst ntfy.sh (siehe ntfy_push.h/ntfy_push_screen.cpp) bei Notfall-
    // Squawk oder Watchlist-Treffer - AUS per Default. ntfyPushTopic() ist
    // der frei gewaehlte, eindeutige Topic-Name (kein Passwort - siehe
    // Info-Text im Screen, der oeffentliche ntfy.sh-Server ist nur so
    // "geheim" wie der Topic-Name selbst).
    bool ntfyPushEnabled();
    void setNtfyPushEnabled(bool on);
    String ntfyPushTopic();
    void setNtfyPushTopic(const String& topic);

    // "Flight Stories" - automatische, kurze Ereignis-Meldungen (Militaer-/
    // Hubschrauber-Sichtung, Tiefflug) per ntfy-Push und/oder MQTT, siehe
    // radar_screen.cpp::updateProximityAlert(). Eigener Schalter, unabhaengig
    // von ntfyPushEnabled() oben (der nur Notfall/Watchlist steuert) - AUS
    // per Default.
    bool ntfyFlightStoriesEnabled();
    void setNtfyFlightStoriesEnabled(bool on);

    // "Anflug-Alarm" - eigene ntfy-Push-Benachrichtigung, sobald ein bereits
    // per Watchlist (Rufzeichen/Squawk/Typ) erkanntes Flugzeug neu in die
    // Landeanflugphase wechselt, siehe radar_screen.cpp::
    // updateProximityAlert(). Eigener Schalter, unabhaengig von
    // ntfyPushEnabled()/ntfyFlightStoriesEnabled() oben - AUS per Default.
    bool ntfyApproachAlertEnabled();
    void setNtfyApproachAlertEnabled(bool on);

    // Ruhezeiten fuer ntfy-Push (Alex' Wunsch, "alles kann, nichts muss") -
    // AUS per Default. Wenn eingeschaltet, werden ALLE ntfy-Push-Typen
    // (Notfall/Watchlist, Flight Stories, Anflug-Alarm) innerhalb des
    // eingestellten Stunden-Fensters unterdrueckt (siehe NtfyPush::
    // isQuietHoursActive() in ntfy_push.h/.cpp) - betrifft NUR die Push-
    // Benachrichtigung aufs Handy, die normale Anzeige/Alarme auf dem
    // Geraete-Display selbst bleiben unveraendert. Start-/Endstunde sind
    // volle Stunden (0-23, siehe ntfy_push_screen.cpp fuer den Editor) -
    // ein ueber Mitternacht laufendes Fenster (z.B. 22 bis 7) ist
    // ausdruecklich vorgesehen, siehe isQuietHoursActive().
    bool ntfyQuietHoursEnabled();
    void setNtfyQuietHoursEnabled(bool on);
    uint8_t ntfyQuietHoursStartHour();
    void setNtfyQuietHoursStartHour(uint8_t hour);
    uint8_t ntfyQuietHoursEndHour();
    void setNtfyQuietHoursEndHour(uint8_t hour);

    // Ein/Aus-Schalter fuer die Route-Watchlist (route_watchlist.h) - AUS
    // per Default, anders als die drei bestehenden Watchlists (Rufzeichen/
    // Squawk/Typ), die kein eigenes Ein/Aus haben: die Route steckt NICHT
    // im ADS-B-Signal, sondern erfordert einen eigenen Hintergrund-HTTPS-
    // Lookup pro Flugzeug (RouteWatchlist::pollBackground(), siehe
    // net_task.cpp) - dieser Schalter kontrolliert daher sowohl den Alarm
    // ALS AUCH, ob ueberhaupt jemals ein solcher Lookup ausgeloest wird
    // (Alex' Wunsch: API-Last im Blick behalten).
    bool routeWatchlistAlertEnabled();
    void setRouteWatchlistAlertEnabled(bool on);

    // Zuletzt vom Geraet GEBOOTETE Firmware-Version (Config::APP_VERSION zum
    // Zeitpunkt des letzten Speicherns) - main.cpp::setup() vergleicht dies
    // beim Start gegen die AKTUELLE Config::APP_VERSION, um genau EINMAL
    // pro neuer Version den "Was ist neu?"-Changelog-Screen zu zeigen. "" =
    // noch nie gespeichert (z.B. Geraete, die dieses Feature noch nicht
    // kannten). WICHTIG, warum das erst NACH dem naechsten Boot passiert
    // und nicht direkt auf dem OTA-Erfolgs-Screen: dort laeuft noch die
    // ALTE (gerade zu ersetzende) Firmware, die den Changelog-Text der NEUEN
    // Version noch gar nicht kennen kann - der neu heruntergeladene Code
    // wird ja erst nach ESP.restart() tatsaechlich ausgefuehrt.
    String lastSeenVersion();
    void setLastSeenVersion(const String& version);

    // Wird NUR im Erfolgsfall eines OTA-Updates gesetzt (siehe
    // menu_screen.cpp::runOtaUpdateScreen(), direkt vor ESP.restart()) - der
    // naechste Boot liest dieses Flag einmalig aus (main.cpp::
    // showWhatsNewIfNeeded()) und setzt es dabei sofort wieder zurueck.
    // Zusaetzlich zu lastSeenVersion noetig, damit der "Was ist neu"-
    // Changelog-Screen WIRKLICH nur nach einem echten OTA-Update erscheint -
    // nicht nach jedem simplen Neuflashen per USB mit einer anderen
    // Versionsnummer (Alex' ausdruecklicher Wunsch).
    bool otaJustInstalled();
    void setOtaJustInstalled(bool value);

    // Persistenter "Update-Installation aussteht"-Zustand (Alex' Wunsch,
    // siehe main.cpp::setup()/menu_screen.cpp::runPendingOtaInstall()) -
    // wird vom "Update installieren"-Screen gesetzt, BEVOR der eigentliche
    // Download beginnt, gefolgt von einem gezielten Neustart. Der naechste
    // Boot liest das Flag ganz frueh aus (vor WLAN-Manager/NetTask/
    // Radarscreen) und konsumiert es sofort (setzt es zurueck), damit ein
    // fehlgeschlagener Versuch NIE in eine Neustart-Schleife fuehren kann -
    // unabhaengig davon, ob der eigentliche Download/das Update danach
    // gelingt oder fehlschlaegt.
    bool otaPendingInstall();
    const char* otaPendingInstallUrl();
    void setOtaPendingInstall(const char* url);
    void clearOtaPendingInstall();
}
