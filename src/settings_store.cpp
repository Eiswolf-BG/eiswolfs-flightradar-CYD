#include "settings_store.h"
#include "config.h"
#include "sd_mutex.h"
#include <SD.h>
#include <cstring>

namespace SettingsStore {

namespace {
    uint8_t rangeIdx = Config::DEFAULT_RANGE_INDEX;
    bool inverted = true; // Dieses Board braucht invertDisplay(true) fuer korrekte Farben (siehe main.cpp)
    // AUS per Default - Tischmontage (180 Grad) ist ein bewusstes Opt-in
    // ueber Menue > System > Anzeige, siehe settings_store.h.
    bool rotated180 = false;
    uint8_t brightnessPct = Config::BRIGHTNESS_MAX_PERCENT;
    bool emergencyAlertOn = true;
    bool proximityAlertOn = true;
    bool watchlistAlertOn = true;
    // MUSS bei einer frischen Installation aus sein - sonst schreibt sich
    // die SD-Karte unbemerkt voll (siehe Bestaetigungsdialog beim
    // Einschalten in menu_screen.cpp + 24h-Auto-Aus in flight_logbook.cpp).
    bool flightLogbookOn = false;
    uint32_t logbookEnabledAtEpoch = 0;
    char logbookSessionFile[16] = {0};
    bool ledHeartbeatOn = true;
    uint8_t screenTimeoutMin = 0;
    bool nightDimmingOn = true;
    bool screensaverOn = false;
    bool hideGroundVehiclesOn = true;
    bool onlyHelicoptersOn = false;
    bool onlyLowAltitudeOn = false;
    uint8_t languageIdx = 0;
    uint8_t unitsModeVal = 0;
    // AUS per Default (ICAO) - IATA-Anzeige ist ein bewusstes Opt-in ueber
    // Menue > Land/Region > Einheiten, siehe settings_store.h.
    bool iataAirportCodesOn = false;
    uint8_t radarThemeIdx = 0;
    // Zwei unabhaengige, ankreuzbare Radar-Extras (radar_theme_screen.cpp) -
    // AUS per Default, siehe Kommentar in settings_store.h.
    bool crtPhosphorOn = false;
    // TESTWEISE - Default auf AN geaendert (siehe Absprache mit Karl).
    // Betrifft nur frische/zurueckgesetzte Geraete ohne gespeicherten Wert -
    // bestehende Geraete behalten ihren in den Preferences gespeicherten
    // Wert, der in applyKeyValue() unten weiterhin Vorrang hat.
    bool radarPulseOn = true;
    bool issMarkerOn = true;
    bool nostalgicModeOn = false;
    bool trailOn = false;
    char lastSeenVersionBuf[16] = {0};

    // MUSS auf SD persistiert werden (nicht nur im RAM halten): wird kurz
    // vor ESP.restart() gesetzt und erst im NAECHSTEN Boot-Zyklus gelesen -
    // ein Neustart loescht den RAM komplett, siehe main.cpp::
    // showWhatsNewIfNeeded().
    bool otaJustInstalledFlag = false;

    void applyKeyValue(const String& key, const String& value) {
        if (key == "range_index") {
            int v = value.toInt();
            if (v >= 0 && v < Config::RANGE_STEP_COUNT) {
                rangeIdx = (uint8_t)v;
            }
        } else if (key == "invert") {
            inverted = (value.toInt() != 0);
        } else if (key == "rotate_180") {
            rotated180 = (value.toInt() != 0);
        } else if (key == "brightness_percent") {
            int v = value.toInt();
            if (v >= Config::BRIGHTNESS_MIN_PERCENT && v <= Config::BRIGHTNESS_MAX_PERCENT) {
                brightnessPct = (uint8_t)v;
            }
        } else if (key == "emergency_alert") {
            emergencyAlertOn = (value.toInt() != 0);
        } else if (key == "proximity_alert") {
            proximityAlertOn = (value.toInt() != 0);
        } else if (key == "watchlist_alert") {
            watchlistAlertOn = (value.toInt() != 0);
        } else if (key == "flight_logbook") {
            flightLogbookOn = (value.toInt() != 0);
        } else if (key == "logbook_enabled_at") {
            logbookEnabledAtEpoch = (uint32_t)value.toInt();
        } else if (key == "logbook_session_file") {
            strncpy(logbookSessionFile, value.c_str(), sizeof(logbookSessionFile) - 1);
            logbookSessionFile[sizeof(logbookSessionFile) - 1] = 0;
        } else if (key == "led_heartbeat") {
            ledHeartbeatOn = (value.toInt() != 0);
        } else if (key == "screen_timeout_min") {
            int v = value.toInt();
            if (v >= 0 && v <= Config::SCREEN_TIMEOUT_MAX_MINUTES) screenTimeoutMin = (uint8_t)v;
        } else if (key == "night_dimming") {
            nightDimmingOn = (value.toInt() != 0);
        } else if (key == "screensaver") {
            screensaverOn = (value.toInt() != 0);
        } else if (key == "hide_ground_vehicles") {
            hideGroundVehiclesOn = (value.toInt() != 0);
        } else if (key == "only_helicopters") {
            onlyHelicoptersOn = (value.toInt() != 0);
        } else if (key == "only_low_altitude") {
            onlyLowAltitudeOn = (value.toInt() != 0);
        } else if (key == "language") {
            int v = value.toInt();
            if (v >= 0 && v <= 5) languageIdx = (uint8_t)v;
        } else if (key == "units_mode") {
            int v = value.toInt();
            if (v >= 0 && v <= 2) unitsModeVal = (uint8_t)v;
        } else if (key == "airport_code_iata") {
            iataAirportCodesOn = (value.toInt() != 0);
        } else if (key == "radar_theme") {
            int v = value.toInt();
            if (v >= 0 && v <= 2) radarThemeIdx = (uint8_t)v;
        } else if (key == "crt_phosphor") {
            crtPhosphorOn = (value.toInt() != 0);
        } else if (key == "radar_pulse") {
            radarPulseOn = (value.toInt() != 0);
        } else if (key == "iss_marker") {
            issMarkerOn = (value.toInt() != 0);
        } else if (key == "nostalgic_mode") {
            nostalgicModeOn = (value.toInt() != 0);
        } else if (key == "trail_enabled") {
            trailOn = (value.toInt() != 0);
        } else if (key == "last_seen_version") {
            strncpy(lastSeenVersionBuf, value.c_str(), sizeof(lastSeenVersionBuf) - 1);
            lastSeenVersionBuf[sizeof(lastSeenVersionBuf) - 1] = 0;
        } else if (key == "ota_just_installed") {
            otaJustInstalledFlag = (value.toInt() != 0);
        }
    }
}

void load() {
    SdMutex::Guard guard;

    if (!SD.exists(Config::SD_SETTINGS_FILE)) return;

    File f = SD.open(Config::SD_SETTINGS_FILE, FILE_READ);
    if (!f) return;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        int eq = line.indexOf('=');
        if (eq < 0) continue;

        String key = line.substring(0, eq);
        String value = line.substring(eq + 1);
        key.trim();
        value.trim();
        applyKeyValue(key, value);
    }
    f.close();

    // Sicherheitsregel: das Flugbuch darf nach einem Neustart nur dann
    // aktiv bleiben, wenn auch ein gueltiger Einschalt-Zeitstempel
    // vorhanden ist - kein Zeitstempel bedeutet garantiert AUS, egal was in
    // "flight_logbook" steht (verhindert unbemerktes Weiterlaufen z.B. nach
    // einem Firmware-Update oder einer manuell bearbeiteten Datei).
    if (flightLogbookOn && logbookEnabledAtEpoch == 0) {
        flightLogbookOn = false;
    }
}

void save() {
    SdMutex::Guard guard;

    File f = SD.open(Config::SD_SETTINGS_FILE, FILE_WRITE);
    if (!f) return;
    f.printf("range_index=%d\n", rangeIdx);
    f.printf("invert=%d\n", inverted ? 1 : 0);
    f.printf("rotate_180=%d\n", rotated180 ? 1 : 0);
    f.printf("brightness_percent=%d\n", brightnessPct);
    f.printf("emergency_alert=%d\n", emergencyAlertOn ? 1 : 0);
    f.printf("proximity_alert=%d\n", proximityAlertOn ? 1 : 0);
    f.printf("watchlist_alert=%d\n", watchlistAlertOn ? 1 : 0);
    f.printf("flight_logbook=%d\n", flightLogbookOn ? 1 : 0);
    f.printf("logbook_enabled_at=%lu\n", (unsigned long)logbookEnabledAtEpoch);
    f.printf("logbook_session_file=%s\n", logbookSessionFile);
    f.printf("led_heartbeat=%d\n", ledHeartbeatOn ? 1 : 0);
    f.printf("screen_timeout_min=%d\n", screenTimeoutMin);
    f.printf("night_dimming=%d\n", nightDimmingOn ? 1 : 0);
    f.printf("screensaver=%d\n", screensaverOn ? 1 : 0);
    f.printf("hide_ground_vehicles=%d\n", hideGroundVehiclesOn ? 1 : 0);
    f.printf("only_helicopters=%d\n", onlyHelicoptersOn ? 1 : 0);
    f.printf("only_low_altitude=%d\n", onlyLowAltitudeOn ? 1 : 0);
    f.printf("language=%d\n", languageIdx);
    f.printf("units_mode=%d\n", unitsModeVal);
    f.printf("airport_code_iata=%d\n", iataAirportCodesOn ? 1 : 0);
    f.printf("radar_theme=%d\n", radarThemeIdx);
    f.printf("crt_phosphor=%d\n", crtPhosphorOn ? 1 : 0);
    f.printf("radar_pulse=%d\n", radarPulseOn ? 1 : 0);
    f.printf("iss_marker=%d\n", issMarkerOn ? 1 : 0);
    f.printf("nostalgic_mode=%d\n", nostalgicModeOn ? 1 : 0);
    f.printf("trail_enabled=%d\n", trailOn ? 1 : 0);
    f.printf("last_seen_version=%s\n", lastSeenVersionBuf);
    f.printf("ota_just_installed=%d\n", otaJustInstalledFlag ? 1 : 0);
    f.close();
}

uint8_t rangeIndex() { return rangeIdx; }

void setRangeIndex(uint8_t idx) {
    if (idx < Config::RANGE_STEP_COUNT) {
        rangeIdx = idx;
        save();
    }
}

bool displayInverted() { return inverted; }

void setDisplayInverted(bool inv) {
    inverted = inv;
    save();
}

bool displayRotated180() { return rotated180; }

void setDisplayRotated180(bool rot) {
    rotated180 = rot;
    save();
}

uint8_t brightnessPercent() { return brightnessPct; }

void setBrightnessPercent(uint8_t percent) {
    if (percent >= Config::BRIGHTNESS_MIN_PERCENT && percent <= Config::BRIGHTNESS_MAX_PERCENT) {
        brightnessPct = percent;
        save();
    }
}

bool emergencyAlertEnabled() { return emergencyAlertOn; }

void setEmergencyAlertEnabled(bool on) {
    emergencyAlertOn = on;
    save();
}

bool proximityAlertEnabled() { return proximityAlertOn; }

void setProximityAlertEnabled(bool on) {
    proximityAlertOn = on;
    save();
}

bool watchlistAlertEnabled() { return watchlistAlertOn; }

void setWatchlistAlertEnabled(bool on) {
    watchlistAlertOn = on;
    save();
}

bool flightLogbookEnabled() { return flightLogbookOn; }

void setFlightLogbookEnabled(bool on) {
    flightLogbookOn = on;
    save();
}

uint32_t flightLogbookEnabledAtEpoch() { return logbookEnabledAtEpoch; }

void setFlightLogbookEnabledAtEpoch(uint32_t epoch) {
    logbookEnabledAtEpoch = epoch;
    save();
}

String flightLogbookSessionFile() { return String(logbookSessionFile); }

void setFlightLogbookSessionFile(const String& label) {
    strncpy(logbookSessionFile, label.c_str(), sizeof(logbookSessionFile) - 1);
    logbookSessionFile[sizeof(logbookSessionFile) - 1] = 0;
    save();
}

bool ledHeartbeatEnabled() { return ledHeartbeatOn; }

void setLedHeartbeatEnabled(bool on) {
    ledHeartbeatOn = on;
    save();
}

uint8_t screenTimeoutMinutes() { return screenTimeoutMin; }

void setScreenTimeoutMinutes(uint8_t minutes) {
    if (minutes <= Config::SCREEN_TIMEOUT_MAX_MINUTES) {
        screenTimeoutMin = minutes;
        save();
    }
}

bool nightDimmingEnabled() { return nightDimmingOn; }

void setNightDimmingEnabled(bool on) {
    nightDimmingOn = on;
    save();
}

bool screensaverEnabled() { return screensaverOn; }

void setScreensaverEnabled(bool on) {
    screensaverOn = on;
    save();
}

bool hideGroundVehicles() { return hideGroundVehiclesOn; }

void setHideGroundVehicles(bool on) {
    hideGroundVehiclesOn = on;
    save();
}

bool onlyHelicopters() { return onlyHelicoptersOn; }

void setOnlyHelicopters(bool on) {
    onlyHelicoptersOn = on;
    save();
}

bool onlyLowAltitude() { return onlyLowAltitudeOn; }

void setOnlyLowAltitude(bool on) {
    onlyLowAltitudeOn = on;
    save();
}

uint8_t language() { return languageIdx; }

void setLanguage(uint8_t lang) {
    if (lang <= 5) {
        languageIdx = lang;
        save();
    }
}

uint8_t unitsMode() { return unitsModeVal; }

void setUnitsMode(uint8_t mode) {
    if (mode <= 2) {
        unitsModeVal = mode;
        save();
    }
}

bool useIataAirportCodes() { return iataAirportCodesOn; }

void setUseIataAirportCodes(bool on) {
    iataAirportCodesOn = on;
    save();
}

uint8_t radarThemeIndex() { return radarThemeIdx; }

void setRadarThemeIndex(uint8_t idx) {
    if (idx <= 2) {
        radarThemeIdx = idx;
        save();
    }
}

bool crtPhosphorEnabled() { return crtPhosphorOn; }

void setCrtPhosphorEnabled(bool on) {
    crtPhosphorOn = on;
    save();
}

bool radarPulseEnabled() { return radarPulseOn; }

void setRadarPulseEnabled(bool on) {
    radarPulseOn = on;
    save();
}

bool issMarkerEnabled() { return issMarkerOn; }

void setIssMarkerEnabled(bool on) {
    issMarkerOn = on;
    save();
}

bool nostalgicModeEnabled() { return nostalgicModeOn; }

void setNostalgicModeEnabled(bool on) {
    nostalgicModeOn = on;
    save();
}

bool trailEnabled() { return trailOn; }

void setTrailEnabled(bool on) {
    trailOn = on;
    save();
}

String lastSeenVersion() { return String(lastSeenVersionBuf); }

void setLastSeenVersion(const String& version) {
    strncpy(lastSeenVersionBuf, version.c_str(), sizeof(lastSeenVersionBuf) - 1);
    lastSeenVersionBuf[sizeof(lastSeenVersionBuf) - 1] = 0;
    save();
}

bool otaJustInstalled() { return otaJustInstalledFlag; }

void setOtaJustInstalled(bool value) {
    otaJustInstalledFlag = value;
    save();
}

}