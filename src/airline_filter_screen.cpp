#include "airline_filter_screen.h"
#include "airline_filter.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace AirlineFilterScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool danger = false) {
        uint16_t accent = danger ? TFT_RED : TFT_GREEN;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    String runLetterKeypad(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr const char* ROW1 = "QWERTYUIOP";
        constexpr const char* ROW2 = "ASDFGHJKL";
        constexpr const char* ROW3 = "ZXCVBNM";

        char buf[4] = {0};
        uint8_t len = 0;

        constexpr int16_t KEY_H = 32;
        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t ROW0_Y = 90;

        auto layoutRow = [&](const char* row, int16_t y, Rect* outRects, uint8_t n) {
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect row1Rects[10], row2Rects[9], row3Rects[7];
        layoutRow(ROW1, ROW0_Y, row1Rects, 10);
        layoutRow(ROW2, ROW0_Y + KEY_H + KEY_GAP, row2Rects, 9);
        layoutRow(ROW3, ROW0_Y + 2 * (KEY_H + KEY_GAP), row3Rects, 7);

        Rect backspaceBtn = {4, (int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), 100, KEY_H};
        Rect cancelBtn     = {(int16_t)(Config::SCREEN_WIDTH - 100), (int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), 96, KEY_H};
        Rect confirmBtn    = {4, (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8), KEY_H};

        bool done = false;
        bool confirmed = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::AIRLINE_ADD_TITLE));

            tft.fillRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_BLACK);
            tft.drawRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_GREEN);
            tft.setTextSize(2);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(14, 66);
            tft.print(buf);
            tft.setTextSize(1);

            for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(ROW1[i]));
            for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(ROW2[i]));
            for (uint8_t i = 0; i < 7; i++) drawButton(tft, row3Rects[i], String(ROW3[i]));

            drawButton(tft, backspaceBtn, "<-");
            drawButton(tft, cancelBtn, I18n::t(StringId::CANCEL), true);
            drawButton(tft, confirmBtn, I18n::t(StringId::ADD));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (row1Rects[i].contains(tap.x, tap.y) && len < 3) { buf[len++] = ROW1[i]; handled = true; }
            }
            for (uint8_t i = 0; i < 9 && !handled; i++) {
                if (row2Rects[i].contains(tap.x, tap.y) && len < 3) { buf[len++] = ROW2[i]; handled = true; }
            }
            for (uint8_t i = 0; i < 7 && !handled; i++) {
                if (row3Rects[i].contains(tap.x, tap.y) && len < 3) { buf[len++] = ROW3[i]; handled = true; }
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
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::AIRLINE_FILTER_TITLE));
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 26);
        tft.println(I18n::t(StringId::AIRLINE_FILTER_DESC1));
        tft.setCursor(10, 38);
        tft.println(I18n::t(StringId::AIRLINE_FILTER_DESC2));

        uint8_t count = AirlineFilter::count();
        int16_t y = 56;

        Rect rowRects[AirlineFilter::MAX_HIDDEN];
        Rect removeRects[AirlineFilter::MAX_HIDDEN];

        for (uint8_t i = 0; i < count; i++) {
            Rect rowRect = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20 - REMOVE_BTN_W - 6), ROW_H};
            Rect removeRect = {(int16_t)(Config::SCREEN_WIDTH - 10 - REMOVE_BTN_W), y, REMOVE_BTN_W, ROW_H};
            rowRects[i] = rowRect;
            removeRects[i] = removeRect;

            tft.fillRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_BLACK);
            tft.drawRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_GREEN);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString(AirlineFilter::icaoAt(i), rowRect.x + rowRect.w / 2, rowRect.y + rowRect.h / 2);
            tft.setTextDatum(TL_DATUM);
            drawButton(tft, removeRect, "X", true);

            y += ROW_H + ROW_GAP;
        }

        Rect addBtn = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        bool canAdd = count < AirlineFilter::MAX_HIDDEN;
        if (canAdd) {
            drawButton(tft, addBtn, I18n::t(StringId::AIRLINE_FILTER_ADD));
            y += 40 + 10;
        }

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
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
        for (uint8_t i = 0; i < count && !handled; i++) {
            if (removeRects[i].contains(tap.x, tap.y)) {
                AirlineFilter::removeHidden(i);
                handled = true;
            }
        }
        if (!handled && canAdd && addBtn.contains(tap.x, tap.y)) {
            String code = runLetterKeypad(tft);
            if (code.length() > 0) {
                AirlineFilter::addHidden(code.c_str());
            }
            handled = true;
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}