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

    // Button frame in the same style as the Start button on the Welcome
    // screen (first_run_welcome_screen.cpp::drawButtonFrame) - black fill,
    // thin green border, rounded corners. Deliberately no longer fully
    // green-filled like the old version of this button, so both first-run
    // screens look consistent.
    void drawButtonFrame(TFT_eSPI& tft, const Rect& r) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, TFT_GREEN);
    }

    // Big, bold (1px-offset technique, same as the "Start" text on the
    // Welcome screen) countdown digit in the middle of the button -
    // replaces the previous "Flightradar" text.
    void drawCountdownText(TFT_eSPI& tft, const Rect& r, const String& text) {
        tft.fillRect((int16_t)(r.x + 2), (int16_t)(r.y + 2), (int16_t)(r.w - 4), (int16_t)(r.h - 4), TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(3);
        int16_t cx = r.x + r.w / 2;
        int16_t cy = r.y + r.h / 2;
        tft.drawString(text, cx, cy);
        tft.drawString(text, (int16_t)(cx + 1), cy);
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
    drawButtonFrame(tft, startBtn);

    // Automatic countdown in the button instead of a tappable
    // "Flightradar" text - continues automatically into the splash
    // screen/app afterward. Tapping the button during the countdown
    // skips the wait immediately (same expectation as other buttons in
    // the app). 7 seconds total (7-6-5-4-3-2-1, 1 second each) instead of
    // the original 3 seconds - a bit more time to read the message above
    // before it automatically continues.
    constexpr uint32_t COUNTDOWN_STEP_MS = 1000;
    constexpr int8_t COUNTDOWN_START = 7;
    for (int8_t count = COUNTDOWN_START; count >= 1; count--) {
        drawCountdownText(tft, startBtn, String(count));

        uint32_t stepStart = millis();
        bool skipped = false;
        while (millis() - stepStart < COUNTDOWN_STEP_MS) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap) && startBtn.contains(tap.x, tap.y)) {
                skipped = true;
                break;
            }
            MenuStars::update(tft);
            delay(20);
        }
        if (skipped) break;
    }
}

}
