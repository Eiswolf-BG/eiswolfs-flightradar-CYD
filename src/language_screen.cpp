#include "language_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"
#include "ui_theme.h"

namespace LanguageScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool active = false) {
        uint16_t bg = active ? UiTheme::accentColor(tft) : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : UiTheme::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // ROW_H dynamisch aus dem verfuegbaren Platz berechnet (gleiches
    // Prinzip wie ROW_H in radar_theme_screen.cpp) statt fest 36px - mit
    // der siebten Sprache (Portugiesisch) plus Zurueck-Button waeren das 8
    // Zeilen, die mit fest 36px+6px Abstand ueber den unteren
    // Bildschirmrand hinausgereicht haetten.
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t START_Y = 30;
    constexpr int16_t BOTTOM_MARGIN = 10;
    constexpr uint8_t ROW_COUNT = I18n::LANG_COUNT + 1; // Sprachen + Zurueck
    constexpr int16_t ROW_H =
        (Config::SCREEN_HEIGHT - START_Y - BOTTOM_MARGIN - (ROW_COUNT - 1) * ROW_GAP) / ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
    }
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::LANGUAGE_TITLE));

        uint8_t current = SettingsStore::language();
        Rect langRects[I18n::LANG_COUNT];

        for (uint8_t i = 0; i < I18n::LANG_COUNT; i++) {
            langRects[i] = rowRect(i);
            drawButton(tft, langRects[i], I18n::languageName(i), i == current);
        }

        Rect backBtn = rowRect(I18n::LANG_COUNT);
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
        for (uint8_t i = 0; i < I18n::LANG_COUNT && !handled; i++) {
            if (langRects[i].contains(tap.x, tap.y)) {
                SettingsStore::setLanguage(i);
                handled = true;
            }
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}