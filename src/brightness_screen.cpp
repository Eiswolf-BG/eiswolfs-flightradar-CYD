#include "brightness_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace BrightnessScreen {

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

    // Gleicher PWM-Kanal wie in main.cpp (dort in setup() mit ledcSetup()
    // initialisiert) - hier bewusst als eigene Konstante dupliziert statt
    // geteilt, siehe CLAUDE.md-Konvention ("jeder Screen unabhaengig").
    constexpr uint8_t BACKLIGHT_PWM_CHANNEL = 0;

    void applyLive(uint8_t percent) {
        uint8_t pwm = (uint8_t)((uint16_t)percent * 255 / 100);
        ledcWrite(BACKLIGHT_PWM_CHANNEL, pwm);
    }
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();

    while (!done) {
        uint8_t percent = SettingsStore::brightnessPercent();

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::BRIGHTNESS_TITLE));

        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(3);
        tft.drawString(buf, Config::SCREEN_WIDTH / 2, 130);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);

        Rect minusBtn = {10, 190, 100, 60};
        Rect plusBtn  = {(int16_t)(Config::SCREEN_WIDTH - 110), 190, 100, 60};
        drawButton(tft, minusBtn, "-");
        drawButton(tft, plusBtn, "+");

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50),
                         (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }

        if (minusBtn.contains(tap.x, tap.y)) {
            uint8_t next = (percent > Config::BRIGHTNESS_MIN_PERCENT + Config::BRIGHTNESS_STEP_PERCENT - 1)
                ? (uint8_t)(percent - Config::BRIGHTNESS_STEP_PERCENT)
                : Config::BRIGHTNESS_MIN_PERCENT;
            SettingsStore::setBrightnessPercent(next);
            applyLive(next);
        } else if (plusBtn.contains(tap.x, tap.y)) {
            uint8_t next = (percent + Config::BRIGHTNESS_STEP_PERCENT <= Config::BRIGHTNESS_MAX_PERCENT)
                ? (uint8_t)(percent + Config::BRIGHTNESS_STEP_PERCENT)
                : Config::BRIGHTNESS_MAX_PERCENT;
            SettingsStore::setBrightnessPercent(next);
            applyLive(next);
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
