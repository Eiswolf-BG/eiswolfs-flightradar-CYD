#include "units_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"

namespace UnitsScreen {

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

    constexpr int16_t ROW_H = 40;
    constexpr int16_t ROW_GAP = 8;
    constexpr int16_t START_Y = 40;

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
    }
}

void run(TFT_eSPI& tft) {
    constexpr uint8_t MODE_COUNT = 3;
    StringId labels[MODE_COUNT] = {StringId::UNITS_AUTO, StringId::UNITS_METRIC, StringId::UNITS_IMPERIAL};

    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::UNITS_TITLE));

        uint8_t current = SettingsStore::unitsMode();
        Rect modeRects[MODE_COUNT];

        for (uint8_t i = 0; i < MODE_COUNT; i++) {
            modeRects[i] = rowRect(i);
            drawButton(tft, modeRects[i], I18n::t(labels[i]), i == current);
        }

        Rect backBtn = rowRect(MODE_COUNT);
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            MenuStars::update(tft);
            delay(20);
        }

        bool handled = false;
        for (uint8_t i = 0; i < MODE_COUNT && !handled; i++) {
            if (modeRects[i].contains(tap.x, tap.y)) {
                SettingsStore::setUnitsMode(i);
                handled = true;
            }
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}