#include "logbook_files_screen.h"
#include "flight_logbook.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace LogbookFilesScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    constexpr uint8_t MAX_DAYS_QUERIED = 31;
    constexpr uint8_t VISIBLE_ROWS = 10;
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::LOGFILES_TITLE));
    tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    tft.setCursor(10, 30);
    tft.print(I18n::t(StringId::LOADING));

    FlightLogbook::DayEntry days[MAX_DAYS_QUERIED];
    uint8_t count = FlightLogbook::listDays(days, MAX_DAYS_QUERIED);

    Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::LOGFILES_TITLE));

    if (count == 0) {
        tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
        tft.setCursor(10, 40);
        tft.println(I18n::t(StringId::LOGFILES_EMPTY));
    } else {
        uint8_t startIdx = (count > VISIBLE_ROWS) ? (count - VISIBLE_ROWS) : 0;
        int16_t y = 30;

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
            y += 20;
        }
    }

    drawButton(tft, backBtn, I18n::t(StringId::BACK));
    MenuStars::reset();

    bool done = false;
    while (!done) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap) && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
        MenuStars::update(tft);
        delay(20);
    }
}

}