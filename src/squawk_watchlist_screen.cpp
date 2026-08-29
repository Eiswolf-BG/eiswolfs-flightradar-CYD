#include "squawk_watchlist_screen.h"
#include "squawk_watchlist.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "ui_theme.h"

namespace SquawkWatchlistScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool danger = false) {
        uint16_t accent = danger ? TFT_RED : UiTheme::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Oktale Zifferntastatur (nur 0-7, kein 8/9) fuer die 4-stellige
    // Squawk-Eingabe - eigenes, bewusst schlankeres Tastenfeld statt des
    // allgemeinen runNumericKeypad() aus location_presets_screen.cpp
    // (das zusaetzlich "-"/"." fuer Koordinaten anbietet und lokal in
    // dieser Datei bleibt, siehe CLAUDE.md "kein gemeinsames Rect/Button-
    // Modul") - hier reichen 8 Tasten in einem 4x2-Raster.
    String runSquawkKeypad(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr const char* DIGITS = "01234567";
        char buf[5] = {0};
        uint8_t len = 0;

        constexpr int16_t KEY_W = 54;
        constexpr int16_t KEY_H = 44;
        constexpr int16_t KEY_GAP = 6;
        constexpr int16_t GRID_LEFT = (Config::SCREEN_WIDTH - 4 * KEY_W - 3 * KEY_GAP) / 2;
        constexpr int16_t GRID_TOP = 90;

        Rect keyRects[8];
        for (uint8_t i = 0; i < 8; i++) {
            int16_t col = i % 4;
            int16_t row = i / 4;
            keyRects[i] = {(int16_t)(GRID_LEFT + col * (KEY_W + KEY_GAP)),
                           (int16_t)(GRID_TOP + row * (KEY_H + KEY_GAP)),
                           KEY_W, KEY_H};
        }

        Rect backspaceBtn = {(int16_t)GRID_LEFT, (int16_t)(GRID_TOP + 2 * (KEY_H + KEY_GAP)),
                              (int16_t)(4 * KEY_W + 3 * KEY_GAP), 34};
        Rect cancelBtn  = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), 110, 40};
        Rect confirmBtn = {(int16_t)(Config::SCREEN_WIDTH - 120), (int16_t)(Config::SCREEN_HEIGHT - 50), 110, 40};

        bool done = false;
        bool confirmed = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::SQUAWK_WATCH_ADD_TITLE));

            tft.fillRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_BLACK);
            tft.drawRect(8, 40, Config::SCREEN_WIDTH - 16, 34, UiTheme::accentColor(tft));
            tft.setTextSize(2);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(14, 66);
            tft.print(buf);
            tft.setTextSize(1);

            for (uint8_t i = 0; i < 8; i++) drawButton(tft, keyRects[i], String(DIGITS[i]));
            drawButton(tft, backspaceBtn, "<- Backspace");
            drawButton(tft, cancelBtn, I18n::t(StringId::CANCEL), true);
            drawButton(tft, confirmBtn, I18n::t(StringId::OK));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            for (uint8_t i = 0; i < 8 && !handled; i++) {
                if (keyRects[i].contains(tap.x, tap.y) && len < 4) {
                    buf[len++] = DIGITS[i];
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
            if (!handled && confirmBtn.contains(tap.x, tap.y) && len == 4) {
                done = true;
                confirmed = true;
                handled = true;
            }

            if (handled) redraw();
        }

        return confirmed ? String(buf) : String();
    }

    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                          int16_t lineHeight, const String& text, int16_t scrollY,
                          int16_t viewTop, int16_t viewBottom, bool draw) {
        int16_t y = startY;
        int32_t start = 0;
        int32_t len = text.length();
        while (start < len) {
            while (start < len && text[start] == ' ') start++;
            if (start >= len) break;

            String line = text.substring(start, len);
            while (tft.textWidth(line) > maxWidth) {
                int32_t lastSpace = line.lastIndexOf(' ');
                if (lastSpace <= 0) break;
                line = line.substring(0, lastSpace);
            }

            if (draw) {
                int16_t screenY = y - scrollY;
                if (screenY >= viewTop && screenY <= viewBottom) {
                    tft.setCursor(x, screenY);
                    tft.print(line);
                }
            }
            y += lineHeight;
            start += line.length();
        }
        return y;
    }

    void runInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();

        constexpr int16_t textMaxWidth = Config::SCREEN_WIDTH - 20;
        constexpr int16_t LINE_H = 16;
        int16_t titleEndY = layoutWrapped(tft, 10, 14, textMaxWidth, LINE_H, I18n::t(StringId::SQUAWK_WATCH_INFO_TITLE), 0, 0, 0, false);
        const int16_t VIEW_TOP = titleEndY + 4;
        constexpr int16_t VIEW_BOTTOM = Config::SCREEN_HEIGHT - 60;

        int16_t totalH = VIEW_TOP;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::SQUAWK_WATCH_INFO_PARA1), 0, 0, 0, false);
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::SQUAWK_WATCH_INFO_PARA2), 0, 0, 0, false);

        int16_t maxScroll = totalH - VIEW_BOTTOM;
        if (maxScroll < 0) maxScroll = 0;
        bool scrollable = maxScroll > 0;
        int16_t scrollY = 0;

        Rect backBtn = scrollable
            ? Rect{10, (int16_t)(Config::SCREEN_HEIGHT - 50), 130, 40}
            : Rect{10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect upBtn   = {146, (int16_t)(Config::SCREEN_HEIGHT - 50), 38, 40};
        Rect downBtn = {190, (int16_t)(Config::SCREEN_HEIGHT - 50), 38, 40};
        constexpr int16_t SCROLL_STEP = 48;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            layoutWrapped(tft, 10, 14, textMaxWidth, LINE_H, I18n::t(StringId::SQUAWK_WATCH_INFO_TITLE), 0, 0, Config::SCREEN_HEIGHT, true);

            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            int16_t y = VIEW_TOP;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::SQUAWK_WATCH_INFO_PARA1), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::SQUAWK_WATCH_INFO_PARA2), scrollY, VIEW_TOP, VIEW_BOTTOM, true);

            drawButton(tft, backBtn, I18n::t(StringId::BACK));
            if (scrollable) {
                drawButton(tft, upBtn, "^");
                drawButton(tft, downBtn, "v");
            }
        };

        redraw();

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (backBtn.contains(tap.x, tap.y)) return;
                if (scrollable && upBtn.contains(tap.x, tap.y) && scrollY > 0) {
                    scrollY -= SCROLL_STEP;
                    if (scrollY < 0) scrollY = 0;
                    redraw();
                } else if (scrollable && downBtn.contains(tap.x, tap.y) && scrollY < maxScroll) {
                    scrollY += SCROLL_STEP;
                    if (scrollY > maxScroll) scrollY = maxScroll;
                    redraw();
                }
            }
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) return;
            MenuStars::update(tft);
            delay(20);
        }
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

        // Kleiner "?"-Info-Button oben rechts, gleiches Muster wie bei der
        // Rufzeichen-Beobachtungsliste/den Standort-Presets/dem WLAN-Manager.
        Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
        drawButton(tft, infoBtn, "?");

        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::SQUAWK_WATCH_TITLE));
        tft.setCursor(10, 40);
        tft.println(I18n::t(StringId::SQUAWK_WATCH_DESC1));
        tft.setCursor(10, 52);
        tft.println(I18n::t(StringId::SQUAWK_WATCH_DESC2));

        uint8_t count = SquawkWatchlist::count();
        int16_t y = 56;

        Rect rowRects[SquawkWatchlist::MAX_WATCHED];
        Rect removeRects[SquawkWatchlist::MAX_WATCHED];

        if (count == 0) {
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setCursor(10, y + 14);
            tft.println(I18n::t(StringId::SQUAWK_WATCH_EMPTY));
            y += ROW_H;
        }

        for (uint8_t i = 0; i < count; i++) {
            Rect rowRect = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20 - REMOVE_BTN_W - 6), ROW_H};
            Rect removeRect = {(int16_t)(Config::SCREEN_WIDTH - 10 - REMOVE_BTN_W), y, REMOVE_BTN_W, ROW_H};
            rowRects[i] = rowRect;
            removeRects[i] = removeRect;

            tft.fillRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_BLACK);
            tft.drawRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, UiTheme::accentColor(tft));
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.drawString(SquawkWatchlist::squawkAt(i), rowRect.x + rowRect.w / 2, rowRect.y + rowRect.h / 2);
            tft.setTextDatum(TL_DATUM);
            drawButton(tft, removeRect, "X", true);

            y += ROW_H + ROW_GAP;
        }

        Rect addBtn = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        bool canAdd = count < SquawkWatchlist::MAX_WATCHED;
        if (canAdd) {
            drawButton(tft, addBtn, I18n::t(StringId::SQUAWK_WATCH_ADD));
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
        if (infoBtn.contains(tap.x, tap.y)) {
            runInfoScreen(tft);
            handled = true;
        }
        for (uint8_t i = 0; i < count && !handled; i++) {
            if (removeRects[i].contains(tap.x, tap.y)) {
                SquawkWatchlist::removeWatched(i);
                handled = true;
            }
        }
        if (!handled && canAdd && addBtn.contains(tap.x, tap.y)) {
            String squawk = runSquawkKeypad(tft);
            if (squawk.length() == 4) {
                SquawkWatchlist::addWatched(squawk.c_str());
            }
            handled = true;
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
