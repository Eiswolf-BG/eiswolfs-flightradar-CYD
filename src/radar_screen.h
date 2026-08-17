#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace RadarScreen {
    void render(TFT_eSPI& tft, int16_t top);
    void tick(TFT_eSPI& tft, int16_t top, uint32_t deltaMs);
    bool handleTap(TFT_eSPI& tft, int16_t x, int16_t y, int16_t top);
    void updateProximityAlert(uint32_t nowMs);

    // Waehlt ein Flugzeug programmgesteuert aus (z.B. von der Flugzeugliste
    // aus, nicht per Antippen auf dem Radar) - damit beim naechsten render()
    // sofort das Detail-Panel fuer dieses Flugzeug erscheint, so als haette
    // man es direkt im Radar angetippt.
    void selectAircraft(const char* hex, const char* callsign);

    struct EmergencyInfo {
        bool active = false;
        char callsign[9] = {0};
        char squawk[5] = {0};
    };

    EmergencyInfo checkEmergency();
}