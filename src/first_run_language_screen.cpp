#include "first_run_language_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "i18n.h"
#include "config.h"

namespace FirstRunLanguageScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr int16_t ROW_H = 36;
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t START_Y = 60;

    Rect rowRect(uint8_t index) {
        return {20, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 40), ROW_H};
    }

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Language / Sprache", Config::SCREEN_WIDTH / 2, 24);
    tft.setTextDatum(TL_DATUM);

    Rect langRects[I18n::LANG_COUNT];
    for (uint8_t i = 0; i < I18n::LANG_COUNT; i++) {
        langRects[i] = rowRect(i);
        drawButton(tft, langRects[i], I18n::languageName(i));
    }

    while (true) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) { delay(20); continue; }

        for (uint8_t i = 0; i < I18n::LANG_COUNT; i++) {
            if (langRects[i].contains(tap.x, tap.y)) {
                SettingsStore::setLanguage(i);
                return;
            }
        }
    }
}

}