#include "ui_theme.h"
#include "settings_store.h"

namespace UiTheme {

uint16_t accentColor(TFT_eSPI& gfx) {
    switch (SettingsStore::radarThemeIndex()) {
        case 1: return gfx.color565(255, 176, 0);  // Amber
        case 2: return gfx.color565(0, 200, 255);  // Blau
        // Iteriert nach mehreren Rueckmeldungen: (255,60,60)/(255,20,20)
        // zu hell/rosa, (255,5,5)/(255,2,2) auf dem Display zwar besser,
        // wirkte in der Web-UI (echtes 24-Bit-Hex statt RGB565-
        // Quantisierung) aber weiterhin zu rosastichig - zurueck auf reines
        // TFT_RED (255,0,0), das in RGB565 ohnehin identisch zu (255,2,2)
        // gerastert wird (G/B-Kanal faellt beide Male unter die
        // Rundungsschwelle), in der Web-UI aber sauber neutral Rot bleibt.
        case 3: return gfx.color565(255, 0, 0);     // Rot
        case 4: return gfx.color565(180, 0, 255);  // Lila
        default: return TFT_GREEN;                  // Gruen (Standard)
    }
}

uint16_t accentColorDimmed(TFT_eSPI& gfx, float fraction) {
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    uint16_t c = accentColor(gfx);
    uint16_t r5 = (c >> 11) & 0x1F;
    uint16_t g6 = (c >> 5) & 0x3F;
    uint16_t b5 = c & 0x1F;
    r5 = (uint16_t)(r5 * fraction + 0.5f);
    g6 = (uint16_t)(g6 * fraction + 0.5f);
    b5 = (uint16_t)(b5 * fraction + 0.5f);
    return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}

}
