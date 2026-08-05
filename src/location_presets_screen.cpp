#include "location_presets_screen.h"
#include "location_presets.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace LocationPresetsScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label,
                     bool active = false, bool danger = false) {
        uint16_t accent = danger ? TFT_RED : TFT_GREEN;
        uint16_t bg = active ? accent : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : accent;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    String runNumericKeypad(TFT_eSPI& tft, const String& title) {
        MenuStars::reset();
        char buf[16] = {0};
        uint8_t len = 0;

        constexpr int16_t KEY_W = 74;
        constexpr int16_t KEY_H = 34;
        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t GRID_LEFT = (Config::SCREEN_WIDTH - 3 * KEY_W - 2 * KEY_GAP) / 2;
        constexpr int16_t GRID_TOP = 84;

        const char* keys[12] = {"1","2","3","4","5","6","7","8","9","-","0","."};
        Rect keyRects[12];
        for (uint8_t i = 0; i < 12; i++) {
            int16_t col = i % 3;
            int16_t row = i / 3;
            keyRects[i] = {(int16_t)(GRID_LEFT + col * (KEY_W + KEY_GAP)),
                           (int16_t)(GRID_TOP + row * (KEY_H + KEY_GAP)),
                           KEY_W, KEY_H};
        }

        Rect backspaceBtn = {(int16_t)GRID_LEFT, (int16_t)(GRID_TOP + 4 * (KEY_H + KEY_GAP)),
                              (int16_t)(3 * KEY_W + 2 * KEY_GAP), 30};
        Rect cancelBtn  = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), 110, 40};
        Rect confirmBtn = {(int16_t)(Config::SCREEN_WIDTH - 120), (int16_t)(Config::SCREEN_HEIGHT - 50), 110, 40};

        bool done = false;
        bool confirmed = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 10);
            tft.println(title);

            tft.fillRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_BLACK);
            tft.drawRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_GREEN);
            tft.setTextSize(2);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(14, 66);
            tft.print(buf);
            tft.setTextSize(1);

            for (uint8_t i = 0; i < 12; i++) drawButton(tft, keyRects[i], keys[i]);
            drawButton(tft, backspaceBtn, "<- Backspace");
            drawButton(tft, cancelBtn, I18n::t(StringId::CANCEL), false, true);
            drawButton(tft, confirmBtn, I18n::t(StringId::OK));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            for (uint8_t i = 0; i < 12 && !handled; i++) {
                if (keyRects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) {
                    buf[len++] = keys[i][0];
                    buf[len] = 0;
                    handled = true;
                }
            }
            if (!handled && backspaceBtn.contains(tap.x, tap.y)) {
                if (len > 0) { len--; buf[len] = 0; }
                handled = true;
            }
            if (!handled && cancelBtn.contains(tap.x, tap.y)) {
                done = true;
                confirmed = false;
                handled = true;
            }
            if (!handled && confirmBtn.contains(tap.x, tap.y) && len > 0) {
                done = true;
                confirmed = true;
                handled = true;
            }

            if (handled) redraw();
        }

        return confirmed ? String(buf) : String();
    }

    bool addPresetFlow(TFT_eSPI& tft) {
        String latStr = runNumericKeypad(tft, I18n::t(StringId::LOCATION_LAT_PROMPT));
        if (latStr.length() == 0) return false;

        String lonStr = runNumericKeypad(tft, I18n::t(StringId::LOCATION_LON_PROMPT));
        if (lonStr.length() == 0) return false;

        double lat = latStr.toDouble();
        double lon = lonStr.toDouble();
        if (lat == 0.0 && lon == 0.0) return false;

        return LocationPresets::addPreset(lat, lon);
    }
}

void run(TFT_eSPI& tft) {
    constexpr int16_t ROW_H = 32;
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t REMOVE_BTN_W = 60;

    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 8);
        tft.println(I18n::t(StringId::LOCATION_TITLE));

        int8_t active = LocationPresets::activeIndex();
        uint8_t count = LocationPresets::count();
        int16_t y = 36;

        Rect autoRect = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
        String autoLabel = I18n::t(StringId::LOCATION_AUTO);
        drawButton(tft, autoRect, active < 0 ? ("> " + autoLabel) : autoLabel, active < 0);
        y += ROW_H + ROW_GAP;

        Rect rowRects[LocationPresets::MAX_PRESETS];
        Rect removeRects[LocationPresets::MAX_PRESETS];
        String presetWord = I18n::t(StringId::LOCATION_PRESET);

        for (uint8_t i = 0; i < LocationPresets::MAX_PRESETS; i++) {
            Rect rowRect = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20 - REMOVE_BTN_W - 6), ROW_H};
            Rect removeRect = {(int16_t)(Config::SCREEN_WIDTH - 10 - REMOVE_BTN_W), y, REMOVE_BTN_W, ROW_H};
            rowRects[i] = rowRect;
            removeRects[i] = removeRect;

            if (i < count) {
                double lat = 0, lon = 0;
                LocationPresets::getLatLon(i, lat, lon);
                char coords[24];
                snprintf(coords, sizeof(coords), "%d: %.3f, %.3f", i + 1, lat, lon);
                String label = (active == (int8_t)i ? "> " : "") + presetWord + " " + coords;
                drawButton(tft, rowRect, label, active == (int8_t)i);
                drawButton(tft, removeRect, "X", false, true);
            } else {
                tft.fillRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_BLACK);
                tft.drawRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_DARKGREEN);
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
                String label = presetWord + " " + String(i + 1) + " " + I18n::t(StringId::LOCATION_PRESET_EMPTY);
                tft.drawString(label, rowRect.x + rowRect.w / 2, rowRect.y + rowRect.h / 2);
                tft.setTextDatum(TL_DATUM);
            }
            y += ROW_H + ROW_GAP;
        }

        Rect addBtn = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), 36};
        bool canAdd = count < LocationPresets::MAX_PRESETS;
        if (canAdd) {
            drawButton(tft, addBtn, I18n::t(StringId::LOCATION_ADD));
        }

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            MenuStars::update(tft);
            delay(20);
        }

        bool handled = false;
        if (autoRect.contains(tap.x, tap.y)) {
            LocationPresets::setActiveIndex(-1);
            handled = true;
        }
        for (uint8_t i = 0; i < count && !handled; i++) {
            if (removeRects[i].contains(tap.x, tap.y)) {
                LocationPresets::removePreset(i);
                handled = true;
            } else if (rowRects[i].contains(tap.x, tap.y)) {
                LocationPresets::setActiveIndex((int8_t)i);
                handled = true;
            }
        }
        if (!handled && canAdd && addBtn.contains(tap.x, tap.y)) {
            addPresetFlow(tft);
            handled = true;
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}