#include "stats_screen.h"
#include "flight_logbook.h"
#include "touch_input.h"
#include "config.h"
#include "i18n.h"

namespace StatsScreen {

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

    void drawStatRow(TFT_eSPI& tft, int16_t y, const String& label, const String& value) {
        tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
        tft.setCursor(10, y);
        tft.print(label);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, y + 14);
        tft.print(value);
        tft.setTextSize(1);
    }

    constexpr uint32_t CONFIRM_WINDOW_MS = 4000;
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 10);
    tft.println(I18n::t(StringId::STATS_TITLE));
    tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    tft.setCursor(10, 44);
    tft.print(I18n::t(StringId::LOADING));

    uint16_t today = FlightLogbook::todayCount();
    uint32_t allTimeAircraft = 0;
    uint16_t allTimeDays = 0;
    FlightLogbook::computeAllTimeStats(allTimeAircraft, allTimeDays);

    Rect resetBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 100), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
    Rect backBtn  = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

    bool confirmPending = false;
    uint32_t confirmArmedAtMs = 0;
    bool justReset = false;

    auto redraw = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 10);
        tft.println(I18n::t(StringId::STATS_TITLE));

        drawStatRow(tft, 44, I18n::t(StringId::STATS_TODAY), String(today));
        drawStatRow(tft, 90, I18n::t(StringId::STATS_ALLTIME), String(allTimeAircraft));
        drawStatRow(tft, 136, I18n::t(StringId::STATS_DAYS), String(allTimeDays));

        if (justReset) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 182);
            tft.print(I18n::t(StringId::STATS_RESET_DONE));
        } else if (allTimeDays > 0) {
            float avgPerDay = (float)allTimeAircraft / (float)allTimeDays;
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", avgPerDay);
            drawStatRow(tft, 182, I18n::t(StringId::STATS_AVG), String(buf));
        }

        uint32_t upSec = millis() / 1000;
        uint32_t upH = upSec / 3600;
        uint32_t upM = (upSec % 3600) / 60;
        char upBuf[8];
        snprintf(upBuf, sizeof(upBuf), "%luh %lum", (unsigned long)upH, (unsigned long)upM);
        tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
        tft.setCursor(10, 210);
        tft.print(String(I18n::t(StringId::STATS_UPTIME_PREFIX)) + upBuf);

        if (confirmPending) {
            drawButton(tft, resetBtn, I18n::t(StringId::STATS_RESET_CONFIRM), true);
        } else {
            drawButton(tft, resetBtn, I18n::t(StringId::STATS_RESET_BTN));
        }
        drawButton(tft, backBtn, I18n::t(StringId::BACK));
    };

    redraw();

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
            }
        }
        delay(20);
    }
}

}