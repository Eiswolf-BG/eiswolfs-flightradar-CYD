#include "settings_store.h"
#include "config.h"
#include "sd_mutex.h"
#include <SD.h>

namespace SettingsStore {

namespace {
    uint8_t rangeIdx = Config::DEFAULT_RANGE_INDEX;
    bool inverted = true; // Dieses Board braucht invertDisplay(true) fuer korrekte Farben (siehe main.cpp)
    bool emergencyAlertOn = true;
    bool proximityAlertOn = true;
    bool watchlistAlertOn = true;
    bool flightLogbookOn = true;
    bool ledHeartbeatOn = true;
    uint8_t screenTimeoutMin = 0;
    bool nightDimmingOn = true;
    bool hideGroundVehiclesOn = true;
    uint8_t languageIdx = 0;
    uint8_t unitsModeVal = 0;

    void applyKeyValue(const String& key, const String& value) {
        if (key == "range_index") {
            int v = value.toInt();
            if (v >= 0 && v < Config::RANGE_STEP_COUNT) {
                rangeIdx = (uint8_t)v;
            }
        } else if (key == "invert") {
            inverted = (value.toInt() != 0);
        } else if (key == "emergency_alert") {
            emergencyAlertOn = (value.toInt() != 0);
        } else if (key == "proximity_alert") {
            proximityAlertOn = (value.toInt() != 0);
        } else if (key == "watchlist_alert") {
            watchlistAlertOn = (value.toInt() != 0);
        } else if (key == "flight_logbook") {
            flightLogbookOn = (value.toInt() != 0);
        } else if (key == "led_heartbeat") {
            ledHeartbeatOn = (value.toInt() != 0);
        } else if (key == "screen_timeout_min") {
            int v = value.toInt();
            if (v >= 0 && v <= 10) screenTimeoutMin = (uint8_t)v;
        } else if (key == "night_dimming") {
            nightDimmingOn = (value.toInt() != 0);
        } else if (key == "hide_ground_vehicles") {
            hideGroundVehiclesOn = (value.toInt() != 0);
        } else if (key == "language") {
            int v = value.toInt();
            if (v >= 0 && v <= 5) languageIdx = (uint8_t)v;
        } else if (key == "units_mode") {
            int v = value.toInt();
            if (v >= 0 && v <= 2) unitsModeVal = (uint8_t)v;
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
}

void save() {
    SdMutex::Guard guard;

    File f = SD.open(Config::SD_SETTINGS_FILE, FILE_WRITE);
    if (!f) return;
    f.printf("range_index=%d\n", rangeIdx);
    f.printf("invert=%d\n", inverted ? 1 : 0);
    f.printf("emergency_alert=%d\n", emergencyAlertOn ? 1 : 0);
    f.printf("proximity_alert=%d\n", proximityAlertOn ? 1 : 0);
    f.printf("watchlist_alert=%d\n", watchlistAlertOn ? 1 : 0);
    f.printf("flight_logbook=%d\n", flightLogbookOn ? 1 : 0);
    f.printf("led_heartbeat=%d\n", ledHeartbeatOn ? 1 : 0);
    f.printf("screen_timeout_min=%d\n", screenTimeoutMin);
    f.printf("night_dimming=%d\n", nightDimmingOn ? 1 : 0);
    f.printf("hide_ground_vehicles=%d\n", hideGroundVehiclesOn ? 1 : 0);
    f.printf("language=%d\n", languageIdx);
    f.printf("units_mode=%d\n", unitsModeVal);
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

bool ledHeartbeatEnabled() { return ledHeartbeatOn; }

void setLedHeartbeatEnabled(bool on) {
    ledHeartbeatOn = on;
    save();
}

uint8_t screenTimeoutMinutes() { return screenTimeoutMin; }

void setScreenTimeoutMinutes(uint8_t minutes) {
    if (minutes <= 10) {
        screenTimeoutMin = minutes;
        save();
    }
}

bool nightDimmingEnabled() { return nightDimmingOn; }

void setNightDimmingEnabled(bool on) {
    nightDimmingOn = on;
    save();
}

bool hideGroundVehicles() { return hideGroundVehiclesOn; }

void setHideGroundVehicles(bool on) {
    hideGroundVehiclesOn = on;
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

}