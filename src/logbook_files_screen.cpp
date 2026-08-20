#include "logbook_files_screen.h"
#include "flight_logbook.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include <WiFi.h>

namespace LogbookFilesScreen {

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

    constexpr uint8_t MAX_DAYS_QUERIED = 31;
    constexpr uint8_t VISIBLE_ROWS = 10;

    // Erste Inhaltszeile eine Zeile tiefer als frueher (30) ansetzen, damit
    // sie nicht mit dem "?"-Info-Button oben rechts (y=2..26) kollidiert.
    constexpr int16_t ROWS_START_Y = 50;

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
        constexpr int16_t VIEW_TOP = 36;
        constexpr int16_t VIEW_BOTTOM = Config::SCREEN_HEIGHT - 60;

        bool wifiConnected = WiFi.status() == WL_CONNECTED;
        String urlLine = wifiConnected ? ("http://" + WiFi.localIP().toString() + "/") : String();

        int16_t totalH = VIEW_TOP;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::LOGFILES_INFO_PARA1), 0, 0, 0, false);
        totalH += 8;
        if (wifiConnected) {
            totalH += LINE_H;
        } else {
            totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::LOGFILES_INFO_PARA2), 0, 0, 0, false);
        }

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
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::LOGFILES_INFO_TITLE));

            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            int16_t y = VIEW_TOP;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::LOGFILES_INFO_PARA1), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            if (wifiConnected) {
                int16_t screenY = y - scrollY;
                if (screenY >= VIEW_TOP && screenY <= VIEW_BOTTOM) {
                    tft.setTextColor(TFT_WHITE, TFT_BLACK);
                    tft.setCursor(10, screenY);
                    tft.print(urlLine);
                }
            } else {
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::LOGFILES_INFO_PARA2), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            }

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
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::LOGFILES_TITLE));
    tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    tft.setCursor(10, ROWS_START_Y);
    tft.print(I18n::t(StringId::LOADING));

    FlightLogbook::DayEntry days[MAX_DAYS_QUERIED];
    uint8_t count = FlightLogbook::listDays(days, MAX_DAYS_QUERIED);

    Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
    Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::LOGFILES_TITLE));
        drawButton(tft, infoBtn, "?");

        Rect deleteRects[VISIBLE_ROWS];
        uint8_t deleteCount = 0;
        uint8_t startIdx = (count > VISIBLE_ROWS) ? (count - VISIBLE_ROWS) : 0;

        if (count == 0) {
            tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
            tft.setCursor(10, ROWS_START_Y);
            tft.println(I18n::t(StringId::LOGFILES_EMPTY));
        } else {
            int16_t y = ROWS_START_Y;

            if (count > VISIBLE_ROWS) {
                tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
                tft.setCursor(10, y);
                tft.print(String(I18n::t(StringId::LOGFILES_SHOWING_PREFIX)) + VISIBLE_ROWS +
                          I18n::t(StringId::LOGFILES_OF) + count + I18n::t(StringId::LOGFILES_DAYS_SUFFIX));
                y += 16;
            }

            for (uint8_t i = startIdx; i < count; i++) {
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.setCursor(10, y);
                tft.print(days[i].date);
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.setCursor(110, y);
                tft.print(String(days[i].count) + I18n::t(StringId::LOGFILES_AIRCRAFT_SUFFIX));

                // Jede Datei einzeln loeschbar (selber Stil wie das rote 'X'
                // bei den WLAN-Netzwerken) - keine Bestaetigung noetig, das
                // globale "Flugbuch zuruecksetzen" im Statistik-Screen
                // bleibt fuer den Alles-loeschen-Fall.
                Rect delBtn = {(int16_t)(Config::SCREEN_WIDTH - 34), (int16_t)(y - 15), 30, 18};
                drawButton(tft, delBtn, "X", true);
                deleteRects[deleteCount++] = delBtn;

                y += 20;
            }
        }

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
        for (uint8_t i = 0; i < deleteCount; i++) {
            if (deleteRects[i].contains(tap.x, tap.y)) {
                FlightLogbook::deleteFile(days[startIdx + i].date);
                count = FlightLogbook::listDays(days, MAX_DAYS_QUERIED);
                handled = true;
                break;
            }
        }
        if (!handled && infoBtn.contains(tap.x, tap.y)) {
            runInfoScreen(tft);
        } else if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
