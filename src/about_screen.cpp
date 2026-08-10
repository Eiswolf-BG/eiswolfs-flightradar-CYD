#include "about_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace AboutScreen {

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
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();

    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);

        // Projektname bewusst hart codiert statt ueber i18n - genau wie
        // schon im Splash-Screen (splash_screen.cpp), da ein Eigenname
        // ohnehin nicht uebersetzt wird.
        tft.setCursor(10, 14);
        tft.println("Eiswolfs Flightradar");

        tft.setCursor(10, 40);
        tft.println(I18n::t(StringId::ABOUT_DESC1));
        tft.setCursor(10, 52);
        tft.println(I18n::t(StringId::ABOUT_DESC2));

        tft.setCursor(10, 74);
        tft.println("(c) 2026 Eiswolf");

        tft.setCursor(10, 86);
        tft.print(I18n::t(StringId::ABOUT_VERSION_PREFIX));
        tft.println(Config::APP_VERSION);

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50),
                         (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            MenuStars::update(tft);
            delay(20);
        }

        if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
