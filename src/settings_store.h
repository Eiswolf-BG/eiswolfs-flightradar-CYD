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

    bool ledHeartbeatEnabled();
    void setLedHeartbeatEnabled(bool on);

    uint8_t screenTimeoutMinutes();
    void setScreenTimeoutMinutes(uint8_t minutes);

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
}
