#include "first_run_location_screen.h"
#include "address_search_screen.h"
#include "location_presets.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace FirstRunLocationScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Lokale Kopie, siehe Konvention in location_presets_screen.cpp.
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
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::FIRST_RUN_LOCATION_TITLE));

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    int16_t textEndY = layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                      I18n::t(StringId::FIRST_RUN_LOCATION_BODY));

    // Buttons folgen direkt unter dem Text (mit etwas Abstand) statt an
    // einer fest einprogrammierten Position - eine laengere Uebersetzung
    // hat sonst den Text bis unter/hinter die Buttons laufen lassen
    // (auf Deutsch beobachtet: Text unlesbar hinter "Adresse eingeben"/
    // "Ueberspringen"). Nach unten hin trotzdem an den Bildschirmrand
    // geklemmt, falls ein Text ausnahmsweise doch sehr lang waere.
    constexpr int16_t BTN_H = 40;
    constexpr int16_t BTN_GAP = 8;
    int16_t setY = textEndY + 14;
    int16_t maxSetY = (int16_t)(Config::SCREEN_HEIGHT - (2 * BTN_H + BTN_GAP + 10));
    if (setY > maxSetY) setY = maxSetY;

    Rect setBtn  = {10, setY, (int16_t)(Config::SCREEN_WIDTH - 20), BTN_H};
    Rect skipBtn = {10, (int16_t)(setY + BTN_H + BTN_GAP), (int16_t)(Config::SCREEN_WIDTH - 20), BTN_H};
    drawButton(tft, setBtn, I18n::t(StringId::FIRST_RUN_LOCATION_SET_BTN));
    drawButton(tft, skipBtn, I18n::t(StringId::FIRST_RUN_LOCATION_SKIP_BTN));

    while (true) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

        if (setBtn.contains(tap.x, tap.y)) {
            if (AddressSearchScreen::run(tft)) {
                // Direkt aktivieren - das ist der ganze Sinn dieses
                // Screens (sofort beim ersten Start den praezisen
                // Standort nutzen, statt weiter auf Auto/IP zu bleiben).
                uint8_t idx = LocationPresets::count();
                if (idx > 0) LocationPresets::setActiveIndex((int8_t)(idx - 1));
            }
            return;
        }
        if (skipBtn.contains(tap.x, tap.y)) {
            return;
        }
    }
}

}
