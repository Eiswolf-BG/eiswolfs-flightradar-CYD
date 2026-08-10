#include "first_run_complete_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace FirstRunCompleteScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                           int16_t lineHeight, const String& text) {
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
            tft.setCursor(x, y);
            tft.print(line);
            y += lineHeight;
            start += line.length();
        }
        return y;
    }

    void drawStartButton(TFT_eSPI& tft, const Rect& r) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, TFT_GREEN);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 6, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_BLACK, TFT_GREEN);
        tft.setTextSize(2);
        int16_t cx = r.x + r.w / 2;
        int16_t cy = r.y + r.h / 2;
        tft.drawString("Flightradar", cx, cy);
        tft.drawString("Flightradar", (int16_t)(cx + 1), cy);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
    }
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::FIRST_RUN_COMPLETE_TITLE));

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    String body = String(I18n::t(StringId::FIRST_RUN_COMPLETE_BODY1)) +
                  Config::SD_ROOT_DIR +
                  I18n::t(StringId::FIRST_RUN_COMPLETE_BODY2);
    int16_t textEndY = layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 18, body);

    constexpr int16_t BTN_H = 60;
    int16_t btnY = (int16_t)(textEndY + 20);
    int16_t maxBtnY = (int16_t)(Config::SCREEN_HEIGHT - (BTN_H + 20));
    if (btnY > maxBtnY) btnY = maxBtnY;

    Rect startBtn = {10, btnY, (int16_t)(Config::SCREEN_WIDTH - 20), BTN_H};
    drawStartButton(tft, startBtn);

    while (true) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }
        if (startBtn.contains(tap.x, tap.y)) return;
    }
}

}
