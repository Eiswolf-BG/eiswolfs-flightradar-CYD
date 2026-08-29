#include "top_aircraft_screen.h"
#include "flight_logbook.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "ui_theme.h"

// "Meistgesehene Flugzeuge"-Rangliste - erreichbar ueber den kleinen "Top"-
// Button oben rechts im Statistik-Screen (siehe stats_screen.cpp). Einfache
// Liste statt Balkendiagramm (anders als StatsHistoryScreen), da hier Text
// (Kennzeichen/Hex-Code + Anzahl) im Vordergrund steht, keine Zeitreihe.
namespace TopAircraftScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    constexpr uint8_t MAX_ROWS = 5;
    constexpr int16_t ROW_TOP = 40;
    constexpr int16_t ROW_H = 34;
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::TOP_AIRCRAFT_TITLE));
    tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
    tft.setCursor(10, ROW_TOP);
    tft.print(I18n::t(StringId::LOADING));

    FlightLogbook::TopAircraft top[MAX_ROWS];
    uint8_t count = FlightLogbook::computeTopAircraft(top, MAX_ROWS);

    Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

    MenuStars::reset();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::TOP_AIRCRAFT_TITLE));

    if (count == 0) {
        tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
        tft.setCursor(10, ROW_TOP);
        tft.print(I18n::t(StringId::TOP_AIRCRAFT_EMPTY));
    } else {
        for (uint8_t i = 0; i < count; i++) {
            int16_t y = ROW_TOP + i * ROW_H;
            String label = top[i].reg[0] ? String(top[i].reg) : String(top[i].hex);

            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, y);
            tft.print(String(i + 1) + ". " + label);

            tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
            tft.setCursor(20, y + 16);
            tft.print(String(top[i].sightings) + I18n::t(StringId::TOP_AIRCRAFT_SIGHTINGS_SUFFIX));
        }
    }

    drawButton(tft, backBtn, I18n::t(StringId::BACK));

    while (true) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            if (backBtn.contains(tap.x, tap.y)) return;
        }
        // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
        if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) return;
        MenuStars::update(tft);
        delay(20);
    }
}

}
