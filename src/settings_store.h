#pragma once
#include <Arduino.h>

namespace SettingsStore {
    void load();
    void save();

    uint8_t rangeIndex();
    void setRangeIndex(uint8_t idx);

    bool displayInverted();
    void setDisplayInverted(bool inverted);

    // Display-Helligkeit in Prozent (Config::BRIGHTNESS_MIN_PERCENT..MAX_PERCENT).
    uint8_t brightnessPercent();
    void setBrightnessPercent(uint8_t percent);

    bool emergencyAlertEnabled();
    void setEmergencyAlertEnabled(bool on);

    bool proximityAlertEnabled();
    void setProximityAlertEnabled(bool on);

    bool watchlistAlertEnabled();
    void setWatchlistAlertEnabled(bool on);

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

    // Sprache der Benutzeroberflaeche: 0=EN,1=DE,2=FR,3=TR,4=ES,5=IT.
    uint8_t language();
    void setLanguage(uint8_t lang);

    // Einheiten-Modus: 0=Auto (per IP-Standort geschaetzt), 1=Metrisch
    // erzwingen, 2=Imperial (Fuss/Knoten/Meilen) erzwingen.
    uint8_t unitsMode();
    void setUnitsMode(uint8_t mode);

    // Radar-Farbschema (Menue > System > Radar-Farbschema): 0=Gruen
    // (Standard), 1=Amber, 2=Blau - betrifft nur den Radar-Screen (Sweep-
    // Linie, Panel-Rahmen/Text, niedrig fliegende Flugzeuge), siehe
    // radar_screen.cpp::themeBaseColor().
    uint8_t radarThemeIndex();
    void setRadarThemeIndex(uint8_t idx);

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
}
