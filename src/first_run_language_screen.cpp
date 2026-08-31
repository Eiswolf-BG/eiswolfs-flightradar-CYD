#include "first_run_language_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"
#include "ui_theme.h"

namespace FirstRunLanguageScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // ROW_H dynamisch aus dem verfuegbaren Platz berechnet (gleiches
    // Prinzip wie in language_screen.cpp) statt fest 36px - mit der
    // siebten Sprache (Portugiesisch) waeren das 7 Zeilen, die mit fest
    // 36px+6px Abstand ab START_Y=60 ueber den unteren Bildschirmrand
    // hinausgereicht haetten.
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t START_Y = 60;
    constexpr int16_t BOTTOM_MARGIN = 10;
    constexpr uint8_t ROW_COUNT = I18n::LANG_COUNT;
    constexpr int16_t ROW_H =
        (Config::SCREEN_HEIGHT - START_Y - BOTTOM_MARGIN - (ROW_COUNT - 1) * ROW_GAP) / ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {20, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 40), ROW_H};
    }

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Language / Sprache", Config::SCREEN_WIDTH / 2, 24);
    tft.setTextDatum(TL_DATUM);

    Rect langRects[I18n::LANG_COUNT];
    for (uint8_t i = 0; i < I18n::LANG_COUNT; i++) {
        langRects[i] = rowRect(i);
        drawButton(tft, langRects[i], I18n::languageName(i));
    }

    MenuStars::reset();
    while (true) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

        for (uint8_t i = 0; i < I18n::LANG_COUNT; i++) {
            if (langRects[i].contains(tap.x, tap.y)) {
                SettingsStore::setLanguage(i);
                return;
            }
        }
    }
}

}