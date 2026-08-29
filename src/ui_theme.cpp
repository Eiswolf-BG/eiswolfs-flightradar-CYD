#include "ui_theme.h"
#include "settings_store.h"

namespace UiTheme {

uint16_t accentColor(TFT_eSPI& gfx) {
    switch (SettingsStore::radarThemeIndex()) {
        case 1: return gfx.color565(255, 176, 0);  // Amber
        case 2: return gfx.color565(0, 200, 255);  // Blau
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
