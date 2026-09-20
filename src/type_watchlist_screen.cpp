#include "type_watchlist_screen.h"
#include "type_watchlist.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "settings_store.h"
#include "i18n.h"
#include "ui_theme.h"

namespace TypeWatchlistScreen {

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

    // Ziffern + Grossbuchstaben (kein Kleinschreibungs-Umschalter, anders
    // als z.B. die ntfy.sh-Themen-Tastatur - Flugzeugtyp-Codes sind wie
    // ICAO-Codes ueblicherweise durchgehend grossgeschrieben, Alex'
    // Vorgabe). Gleiches Layout-Muster wie runCallsignKeypad() in
    // aircraft_watchlist_screen.cpp, nur mit kuerzerem 4-Zeichen-Puffer
    // (passend zu Aircraft::typeCode, char[5]).
    String runTypeKeypad(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr const char* DIGITS = "1234567890";
        constexpr const char* ROW1 = "QWERTYUIOP";
        constexpr const char* ROW2 = "ASDFGHJKL";
        constexpr const char* ROW3 = "ZXCVBNM";

        char buf[5] = {0};
        uint8_t len = 0;

        constexpr int16_t KEY_H = 30;
        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t ROW0_Y = 78;

        auto layoutRow = [&](const char* row, int16_t y, Rect* outRects, uint8_t n) {
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect digitRects[10], row1Rects[10], row2Rects[9], row3Rects[7];
        layoutRow(DIGITS, ROW0_Y, digitRects, 10);
        layoutRow(ROW1, ROW0_Y + (KEY_H + KEY_GAP), row1Rects, 10);
        layoutRow(ROW2, ROW0_Y + 2 * (KEY_H + KEY_GAP), row2Rects, 9);
        layoutRow(ROW3, ROW0_Y + 3 * (KEY_H + KEY_GAP), row3Rects, 7);

        Rect backspaceBtn = {4, (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), 100, KEY_H};
        Rect cancelBtn     = {(int16_t)(Config::SCREEN_WIDTH - 100), (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), 96, KEY_H};
        Rect confirmBtn    = {4, (int16_t)(ROW0_Y + 5 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8), KEY_H};

        bool done = false;
        bool confirmed = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::TYPE_WATCH_ADD_TITLE));

            tft.fillRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_BLACK);
            tft.drawRect(8, 40, Config::SCREEN_WIDTH - 16, 34, UiTheme::accentColor(tft));
            tft.setTextSize(2);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(14, 66);
            tft.print(buf);
            tft.setTextSize(1);

            for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(DIGITS[i]));
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
                if (digitRects[i].contains(tap.x, tap.y) && len < 4) { buf[len++] = DIGITS[i]; handled = true; }
            }
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (row1Rects[i].contains(tap.x, tap.y) && len < 4) { buf[len++] = ROW1[i]; handled = true; }
            }
            for (uint8_t i = 0; i < 9 && !handled; i++) {
                if (row2Rects[i].contains(tap.x, tap.y) && len < 4) { buf[len++] = ROW2[i]; handled = true; }
            }
            for (uint8_t i = 0; i < 7 && !handled; i++) {
                if (row3Rects[i].contains(tap.x, tap.y) && len < 4) { buf[len++] = ROW3[i]; handled = true; }
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
        int16_t titleEndY = layoutWrapped(tft, 10, 14, textMaxWidth, LINE_H, I18n::t(StringId::TYPE_WATCH_INFO_TITLE), 0, 0, 0, false);
        const int16_t VIEW_TOP = titleEndY + 4;
        constexpr int16_t VIEW_BOTTOM = Config::SCREEN_HEIGHT - 60;

        int16_t totalH = VIEW_TOP;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::TYPE_WATCH_INFO_PARA1), 0, 0, 0, false);
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::TYPE_WATCH_INFO_PARA2), 0, 0, 0, false);

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
            layoutWrapped(tft, 10, 14, textMaxWidth, LINE_H, I18n::t(StringId::TYPE_WATCH_INFO_TITLE), 0, 0, Config::SCREEN_HEIGHT, true);

            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            int16_t y = VIEW_TOP;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::TYPE_WATCH_INFO_PARA1), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::TYPE_WATCH_INFO_PARA2), scrollY, VIEW_TOP, VIEW_BOTTOM, true);

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
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) return;
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

        Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
        drawButton(tft, infoBtn, "?");

        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::TYPE_WATCH_TITLE));

        // BUGFIX (Alex' Meldung, Foto vom Geraet): DESC1/DESC2 liefen vorher
        // ueber rohes setCursor()/println() an FESTEN Y-Positionen - bei der
        // laengeren deutschen Uebersetzung brach TFT_eSPI's eingebautes
        // Auto-Wrap mitten im Wort auf eine dritte Zeile um, die dann mit
        // dem darunter fest positionierten "leere Liste"-Platzhalter
        // ueberlappte. Jetzt ueber layoutWrapped() (echter wortweiser
        // Umbruch anhand der tatsaechlichen Pixelbreite, CLAUDE.md-Pflicht
        // fuer variablen Text) als ein zusammenhaengender Absatz, die Liste
        // beginnt dynamisch unter der gemessenen End-Y-Position statt an
        // einer festen Zahl.
        String descText = String(I18n::t(StringId::TYPE_WATCH_DESC1)) + " " +
                           I18n::t(StringId::TYPE_WATCH_DESC2);
        int16_t descEndY = layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                                          descText, 0, 0, Config::SCREEN_HEIGHT, true);

        uint8_t count = TypeWatchlist::count();
        int16_t y = (int16_t)(descEndY + 16);

        Rect rowRects[TypeWatchlist::MAX_WATCHED];
        Rect removeRects[TypeWatchlist::MAX_WATCHED];

        if (count == 0) {
            // BUGFIX (Alex' Meldung, zweites Foto): derselbe Ueberlapp-
            // Fehler wie beim Erklaertext oben steckte auch noch im "leere
            // Liste"-Platzhalter - der lief ebenfalls ueber rohes
            // setCursor()/println() mit nur EINER ROW_H Platz reserviert,
            // brach bei laengeren Uebersetzungen aber auf eine zweite Zeile
            // um und ragte dadurch in den "Hinzufuegen"-Button hinein. Jetzt
            // ebenfalls ueber layoutWrapped() mit grosszuegigerem
            // Zeilenabstand/Puffer, die Buttons ruecken dynamisch weiter
            // runter statt zu knapp zu folgen.
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            y = layoutWrapped(tft, 10, (int16_t)(y + 14), (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                               I18n::t(StringId::TYPE_WATCH_EMPTY), 0, 0, Config::SCREEN_HEIGHT, true);
            y += 20;
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
            tft.drawString(TypeWatchlist::typeAt(i), rowRect.x + rowRect.w / 2, rowRect.y + rowRect.h / 2);
            tft.setTextDatum(TL_DATUM);
            drawButton(tft, removeRect, "X", true);

            y += ROW_H + ROW_GAP;
        }

        Rect addBtn = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        bool canAdd = count < TypeWatchlist::MAX_WATCHED;
        if (canAdd) {
            drawButton(tft, addBtn, I18n::t(StringId::TYPE_WATCH_ADD));
            y += 40 + 10;
        }

        // Dynamisch statt fest an SCREEN_HEIGHT-50 verankert (Bugfix oben) -
        // der Erklaertext kann je nach Sprache mehr Platz brauchen als
        // vorher angenommen, ein fester Wert koennte sonst wieder mit der
        // Liste/dem "Hinzufuegen"-Button ueberlappen.
        Rect backBtn = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
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
                TypeWatchlist::removeWatched(i);
                handled = true;
            }
        }
        if (!handled && canAdd && addBtn.contains(tap.x, tap.y)) {
            String typeCode = runTypeKeypad(tft);
            if (typeCode.length() > 0) {
                TypeWatchlist::addWatched(typeCode.c_str());
            }
            handled = true;
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
