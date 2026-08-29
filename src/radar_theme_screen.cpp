#include "radar_theme_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"

// Einstell-Screen fuer die Radar-Darstellung (Menue > System > Radar-
// Darstellung). Oben drei EXKLUSIVE Farbschema-Buttons (Gruen/Amber/Blau,
// SettingsStore::radarThemeIndex(), gleiches 3-Wege-Auswahl-Muster wie
// units_screen.cpp), darunter vier UNABHAENGIGE, ankreuzbare Extras
// (CRT-Phosphor, Radar-Puls, Nostalgisch, Flugbahn-Trail) - lassen sich
// mit JEDEM der drei Farbschemata kombinieren, deshalb eigene bool-
// Einstellungen statt weiterer Werte fuer radarThemeIndex(). Betrifft NUR
// den Radar-Screen (siehe radar_screen.cpp), alle anderen Bildschirme
// bleiben unveraendert gruen.
namespace RadarThemeScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool active = false) {
        uint16_t bg = active ? TFT_GREEN : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : TFT_GREEN;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Echtes ankreuzbares Kaestchen links neben dem Label statt eines
    // Text-Buttons mit "An/Aus"-Suffix (Alex' ausdruecklicher Wunsch) - die
    // ganze Zeile bleibt trotzdem antippbar (nicht nur das Kaestchen
    // selbst), gleiche Rect.contains()-Flaeche wie bei den Theme-Buttons.
    void drawCheckboxRow(TFT_eSPI& tft, const Rect& r, const String& label, bool checked) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);

        constexpr int16_t BOX_SIZE = 22;
        int16_t boxX = r.x + 10;
        int16_t boxY = (int16_t)(r.y + (r.h - BOX_SIZE) / 2);
        if (checked) {
            tft.fillRoundRect(boxX, boxY, BOX_SIZE, BOX_SIZE, 3, TFT_GREEN);
        } else {
            tft.drawRoundRect(boxX, boxY, BOX_SIZE, BOX_SIZE, 3, TFT_GREEN);
        }

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(label, (int16_t)(boxX + BOX_SIZE + 10), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    // Zeilenhoehe aus dem verfuegbaren Platz errechnet (gleiches Muster wie
    // SYSTEM_ROW_H in menu_screen.cpp) statt fest verdrahtet - 8 Zeilen
    // (3 Farbschemata + 4 Kaestchen + Zurueck) muessen mit ca. 10px Reserve
    // zum unteren Rand aufs Display passen.
    constexpr uint8_t ROW_COUNT = 8;
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t START_Y = 40;
    constexpr int16_t END_Y = Config::SCREEN_HEIGHT - 10;
    constexpr int16_t ROW_H =
        (END_Y - START_Y - (ROW_COUNT - 1) * ROW_GAP) / ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
    }
}

void run(TFT_eSPI& tft) {
    constexpr uint8_t THEME_COUNT = 3;
    StringId themeLabels[THEME_COUNT] = {StringId::RADAR_THEME_GREEN, StringId::RADAR_THEME_AMBER,
                                          StringId::RADAR_THEME_BLUE};

    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::RADAR_THEME_TITLE));

        uint8_t currentTheme = SettingsStore::radarThemeIndex();
        Rect themeRects[THEME_COUNT];

        for (uint8_t i = 0; i < THEME_COUNT; i++) {
            themeRects[i] = rowRect(i);
            drawButton(tft, themeRects[i], I18n::t(themeLabels[i]), i == currentTheme);
        }

        Rect crtRow = rowRect(THEME_COUNT);
        drawCheckboxRow(tft, crtRow, I18n::t(StringId::RADAR_THEME_CRT), SettingsStore::crtPhosphorEnabled());

        Rect pulseRow = rowRect(THEME_COUNT + 1);
        drawCheckboxRow(tft, pulseRow, I18n::t(StringId::RADAR_PULSE_TOGGLE), SettingsStore::radarPulseEnabled());

        Rect nostalgicRow = rowRect(THEME_COUNT + 2);
        drawCheckboxRow(tft, nostalgicRow, I18n::t(StringId::MENU_NOSTALGIC_MODE), SettingsStore::nostalgicModeEnabled());

        Rect trailRow = rowRect(THEME_COUNT + 3);
        drawCheckboxRow(tft, trailRow, I18n::t(StringId::MENU_FLIGHT_TRAIL), SettingsStore::trailEnabled());

        Rect backBtn = rowRect(THEME_COUNT + 4);
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }

        bool handled = false;
        for (uint8_t i = 0; i < THEME_COUNT && !handled; i++) {
            if (themeRects[i].contains(tap.x, tap.y)) {
                SettingsStore::setRadarThemeIndex(i);
                handled = true;
            }
        }
        if (!handled && crtRow.contains(tap.x, tap.y)) {
            SettingsStore::setCrtPhosphorEnabled(!SettingsStore::crtPhosphorEnabled());
            handled = true;
        }
        if (!handled && pulseRow.contains(tap.x, tap.y)) {
            SettingsStore::setRadarPulseEnabled(!SettingsStore::radarPulseEnabled());
            handled = true;
        }
        if (!handled && nostalgicRow.contains(tap.x, tap.y)) {
            SettingsStore::setNostalgicModeEnabled(!SettingsStore::nostalgicModeEnabled());
            handled = true;
        }
        if (!handled && trailRow.contains(tap.x, tap.y)) {
            SettingsStore::setTrailEnabled(!SettingsStore::trailEnabled());
            handled = true;
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
