#pragma once
#include <Arduino.h>

namespace LocationPresets {
    constexpr uint8_t MAX_PRESETS = 3;

    void init();

    uint8_t count();
    void getLatLon(uint8_t index, double& lat, double& lon);
    // Vom Nutzer vergebener Name (z.B. "Zuhause") - leerer String, wenn
    // keiner gesetzt wurde (dann zeigt der Screen stattdessen die
    // Koordinaten an).
    String getName(uint8_t index);

    bool addPreset(double lat, double lon, const String& name = String());
    void removePreset(uint8_t index);

    int8_t activeIndex();
    void setActiveIndex(int8_t index);
}
