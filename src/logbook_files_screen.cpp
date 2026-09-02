#include "logbook_files_screen.h"
#include "flight_logbook.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "menu_screen.h"
#include "config.h"
#include "i18n.h"
#include <WiFi.h>
#include "ui_theme.h"

namespace LogbookFilesScreen {

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

    constexpr uint8_t MAX_DAYS_QUERIED = 31;
    constexpr uint8_t VISIBLE_ROWS = 10;

    // Erste Inhaltszeile eine Zeile tiefer als frueher (30) ansetzen, damit
    // sie nicht mit dem "?"-Info-Button oben rechts (y=2..26) kollidiert.
    constexpr int16_t ROWS_START_Y = 50;

    // Nutzt den gemeinsamen, bereits bewaehrten Info-Screen aus
    // menu_screen.cpp (MenuScreen::showInfoScreen()) statt einer eigenen
    // Scroll-/Layout-Implementierung - der Titel wird darueber automatisch
    // umgebrochen bzw. verkleinert, wenn er in einer Sprache nicht in eine
    // Zeile passt (vorher: festes tft.println() ohne Breitenpruefung). Die
    // WLAN-URL wird dabei jetzt in derselben Akzentfarbe wie der uebrige
    // Text dargestellt statt separat in Weiss - kleine optische Vereinfachung
    // durch die Wiederverwendung des gemeinsamen einfarbigen Textblocks.
    void runInfoScreen(TFT_eSPI& tft) {
        bool wifiConnected = WiFi.status() == WL_CONNECTED;

        String body = I18n::t(StringId::LOGFILES_INFO_PARA1);
        body += "\n\n";
        body += wifiConnected ? ("http://" + WiFi.localIP().toString() + "/")
                               : String(I18n::t(StringId::LOGFILES_INFO_PARA2));

        MenuScreen::showInfoScreen(tft, I18n::t(StringId::LOGFILES_INFO_TITLE), body,
                                    UiTheme::accentColor(tft), I18n::t(StringId::BACK));
    }
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::LOGFILES_TITLE));
    tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
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
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::LOGFILES_TITLE));
        drawButton(tft, infoBtn, "?");

        Rect deleteRects[VISIBLE_ROWS];
        uint8_t deleteCount = 0;
        uint8_t startIdx = (count > VISIBLE_ROWS) ? (count - VISIBLE_ROWS) : 0;

        if (count == 0) {
            tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
            tft.setCursor(10, ROWS_START_Y);
            tft.println(I18n::t(StringId::LOGFILES_EMPTY));
        } else {
            int16_t y = ROWS_START_Y;

            if (count > VISIBLE_ROWS) {
                tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
                tft.setCursor(10, y);
                tft.print(String(I18n::t(StringId::LOGFILES_SHOWING_PREFIX)) + VISIBLE_ROWS +
                          I18n::t(StringId::LOGFILES_OF) + count + I18n::t(StringId::LOGFILES_DAYS_SUFFIX));
                y += 16;
            }

            for (uint8_t i = startIdx; i < count; i++) {
                tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
                tft.setCursor(10, y);
                tft.print(days[i].date);
                tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
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
