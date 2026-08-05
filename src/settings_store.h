#pragma once
#include <Arduino.h>

namespace SettingsStore {
    void load();
    void save();

    uint8_t rangeIndex();
    void setRangeIndex(uint8_t idx);

    bool displayInverted();
    void setDisplayInverted(bool inverted);

    bool emergencyAlertEnabled();
    void setEmergencyAlertEnabled(bool on);

    bool proximityAlertEnabled();
    void setProximityAlertEnabled(bool on);

    bool watchlistAlertEnabled();
    void setWatchlistAlertEnabled(bool on);

    bool flightLogbookEnabled();
    void setFlightLogbookEnabled(bool on);

    bool ledHeartbeatEnabled();
    void setLedHeartbeatEnabled(bool on);

    uint8_t screenTimeoutMinutes();
    void setScreenTimeoutMinutes(uint8_t minutes);

    bool nightDimmingEnabled();
    void setNightDimmingEnabled(bool on);

    bool hideGroundVehicles();
    void setHideGroundVehicles(bool on);

    // Sprache der Benutzeroberflaeche: 0=EN,1=DE,2=FR,3=TR,4=ES,5=IT.
    uint8_t language();
    void setLanguage(uint8_t lang);

    // Einheiten-Modus: 0=Auto (per IP-Standort geschaetzt), 1=Metrisch
    // erzwingen, 2=Imperial (Fuss/Knoten/Meilen) erzwingen.
    uint8_t unitsMode();
    void setUnitsMode(uint8_t mode);
}