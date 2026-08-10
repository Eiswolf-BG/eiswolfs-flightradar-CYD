#include "stats_history_screen.h"
#include "flight_logbook.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace StatsHistoryScreen {

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
    constexpr uint8_t MAX_BARS = 7;

    constexpr int16_t CHART_TOP = 60;
    constexpr int16_t CHART_BOTTOM = 210;
    constexpr int16_t LABEL_RESERVE_TOP = 16;
    constexpr int16_t USABLE_BAR_HEIGHT = (CHART_BOTTOM - CHART_TOP) - LABEL_RESERVE_TOP;
    constexpr int16_t DAY_LABEL_Y = CHART_BOTTOM + 14;

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

        int16_t totalH = VIEW_TOP;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::STATS_HISTORY_INFO_PARA1), 0, 0, 0, false);

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
            tft.println(I18n::t(StringId::STATS_HISTORY_INFO_TITLE));

            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            int16_t y = VIEW_TOP;
            layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::STATS_HISTORY_INFO_PARA1), scrollY, VIEW_TOP, VIEW_BOTTOM, true);

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
            MenuStars::update(tft);
            delay(20);
        }
    }
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::STATS_HISTORY_TITLE));
    tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    tft.setCursor(10, 30);
    tft.print(I18n::t(StringId::LOADING));

    FlightLogbook::DayEntry days[MAX_DAYS_QUERIED];
    // listDaySummaries() statt listDays(): mehrere Sitzungs-Dateien am
    // selben Kalendertag (z.B. nach erneutem Einschalten) sollen hier
    // weiterhin als EIN Balken pro Tag zaehlen statt als mehrere.
    uint8_t count = FlightLogbook::listDaySummaries(days, MAX_DAYS_QUERIED);

    uint8_t barCount = (count > MAX_BARS) ? MAX_BARS : count;
    uint8_t startIdx = count - barCount;

    uint32_t maxCount = 0;
    for (uint8_t i = startIdx; i < count; i++) {
        if (days[i].count > maxCount) maxCount = days[i].count;
    }

    Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
    Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::STATS_HISTORY_TITLE));
        drawButton(tft, infoBtn, "?");

        if (barCount == 0) {
            tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
            tft.setCursor(10, 40);
            tft.println(I18n::t(StringId::STATS_HISTORY_EMPTY));
        } else {
            tft.drawFastHLine(10, CHART_BOTTOM, (int16_t)(Config::SCREEN_WIDTH - 20), TFT_DARKGREEN);

            int16_t barW = (int16_t)((Config::SCREEN_WIDTH - 20 - (barCount - 1) * 6) / barCount);
            int16_t x = 10;

            for (uint8_t i = startIdx; i < count; i++) {
                uint32_t dayCount = days[i].count;

                if (dayCount == 0) {
                    tft.drawFastHLine(x, CHART_BOTTOM, barW, TFT_DARKGREEN);
                } else {
                    int16_t barH = (maxCount > 0)
                        ? (int16_t)((uint32_t)USABLE_BAR_HEIGHT * dayCount / maxCount)
                        : 0;
                    if (barH < 2) barH = 2;
                    int16_t barTop = CHART_BOTTOM - barH;

                    tft.fillRoundRect(x, barTop, barW, barH, 2, TFT_GREEN);

                    tft.setTextDatum(BC_DATUM);
                    tft.setTextColor(TFT_GREEN, TFT_BLACK);
                    tft.drawString(String(dayCount), x + barW / 2, barTop - 2);
                    tft.setTextDatum(TL_DATUM);
                }

                const char* date = days[i].date;
                size_t dateLen = strlen(date);
                String dayLabel = dateLen >= 2 ? String(date + dateLen - 2) : String(date);
                tft.setTextDatum(TC_DATUM);
                tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
                tft.drawString(dayLabel, x + barW / 2, DAY_LABEL_Y);
                tft.setTextDatum(TL_DATUM);

                x += barW + 6;
            }
        }

        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            MenuStars::update(tft);
            delay(20);
        }

        if (infoBtn.contains(tap.x, tap.y)) {
            runInfoScreen(tft);
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
