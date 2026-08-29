#include "stats_screen.h"
#include "flight_logbook.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "units.h"
#include "location_manager.h"
#include "top_aircraft_screen.h"
#include "ui_theme.h"

namespace StatsScreen {

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

    void drawStatRow(TFT_eSPI& tft, int16_t labelY, const String& label, const String& value) {
        tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
        tft.setCursor(10, labelY);
        tft.print(label);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, labelY + 20);
        tft.print(value);
    }

    constexpr uint32_t CONFIRM_WINDOW_MS = 4000;

    constexpr int16_t ROW1_Y = 38;
    constexpr int16_t ROW2_Y = 72;
    constexpr int16_t ROW3_Y = 106;
    constexpr int16_t ROW4_Y = 140;
    constexpr int16_t UPTIME_Y = 174;
    constexpr int16_t TOP_ALT_Y = 194; // zweizeilig wie drawStatRow (Label + Wert)
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::STATS_TITLE));
    tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
    tft.setCursor(10, ROW1_Y);
    tft.print(I18n::t(StringId::LOADING));

    uint16_t today = FlightLogbook::todayCount();
    uint32_t allTimeAircraft = 0;
    uint16_t allTimeDays = 0;
    FlightLogbook::computeAllTimeStats(allTimeAircraft, allTimeDays);
    FlightLogbook::TopAltitude topAlt = FlightLogbook::todayMaxAltitude();

    Rect resetBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 100), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
    Rect backBtn  = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
    // Kleiner Button oben rechts (gleiches Platzierungsmuster wie das "?"-
    // Info-Icon auf anderen Screens) - oeffnet die neue "Meistgesehene
    // Flugzeuge"-Rangliste, ohne die bestehende Zeilen-/Button-Anordnung
    // dieses Screens zu veraendern. Breit genug fuer den laengsten
    // uebersetzten Button-Text (Tuerkisch "En Çok Görülen").
    Rect topAircraftBtn = {(int16_t)(Config::SCREEN_WIDTH - 100), 2, 94, 22};

    bool confirmPending = false;
    uint32_t confirmArmedAtMs = 0;
    bool justReset = false;

    auto redraw = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::STATS_TITLE));

        drawStatRow(tft, ROW1_Y, I18n::t(StringId::STATS_TODAY), String(today));
        drawStatRow(tft, ROW2_Y, I18n::t(StringId::STATS_ALLTIME), String(allTimeAircraft));
        drawStatRow(tft, ROW3_Y, I18n::t(StringId::STATS_DAYS), String(allTimeDays));

        if (justReset) {
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, ROW4_Y);
            tft.print(I18n::t(StringId::STATS_RESET_DONE));
        } else if (allTimeDays > 0) {
            float avgPerDay = (float)allTimeAircraft / (float)allTimeDays;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", avgPerDay);
            drawStatRow(tft, ROW4_Y, I18n::t(StringId::STATS_AVG), String(buf));
        }

        uint32_t upSec = millis() / 1000;
        uint32_t upH = upSec / 3600;
        uint32_t upM = (upSec % 3600) / 60;
        char upBuf[8];
        snprintf(upBuf, sizeof(upBuf), "%luh %lum", (unsigned long)upH, (unsigned long)upM);
        tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
        tft.setCursor(10, UPTIME_Y);
        tft.print(String(I18n::t(StringId::STATS_UPTIME_PREFIX)) + upBuf);

        if (topAlt.found) {
            String csign = topAlt.callsign[0] ? String(topAlt.callsign) : "?";
            char altBuf[24];
            if (LocationManager::useMetricUnits()) {
                snprintf(altBuf, sizeof(altBuf), "%s (%ldm)", csign.c_str(), (long)Units::feetToMeters((float)topAlt.altitudeFt));
            } else {
                snprintf(altBuf, sizeof(altBuf), "%s (%ld ft)", csign.c_str(), (long)topAlt.altitudeFt);
            }
            drawStatRow(tft, TOP_ALT_Y, I18n::t(StringId::STATS_TOP_ALTITUDE_PREFIX), String(altBuf));
        }

        if (confirmPending) {
            drawButton(tft, resetBtn, I18n::t(StringId::STATS_RESET_CONFIRM), true);
        } else {
            drawButton(tft, resetBtn, I18n::t(StringId::STATS_RESET_BTN));
        }
        drawButton(tft, backBtn, I18n::t(StringId::BACK));
        drawButton(tft, topAircraftBtn, I18n::t(StringId::STATS_TOP_AIRCRAFT_BTN));
    };

    redraw();
    MenuStars::reset();

    bool done = false;
    while (!done) {
        if (confirmPending && millis() - confirmArmedAtMs > CONFIRM_WINDOW_MS) {
            confirmPending = false;
            redraw();
        }

        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            if (resetBtn.contains(tap.x, tap.y)) {
                if (confirmPending) {
                    FlightLogbook::resetAllData();
                    today = 0;
                    allTimeAircraft = 0;
                    allTimeDays = 0;
                    confirmPending = false;
                    justReset = true;
                    redraw();
                } else {
                    confirmPending = true;
                    confirmArmedAtMs = millis();
                    redraw();
                }
            } else if (backBtn.contains(tap.x, tap.y)) {
                done = true;
            } else if (topAircraftBtn.contains(tap.x, tap.y)) {
                TopAircraftScreen::run(tft);
                redraw();
            }
        }
        // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
        if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) done = true;
        MenuStars::update(tft);
        delay(20);
    }
}

}
