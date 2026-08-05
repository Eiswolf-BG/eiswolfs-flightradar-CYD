#include "wifi_manage_screen.h"
#include "wifi_manager.h"
#include "wifi_setup_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace WifiManageScreen {

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
}

void run(TFT_eSPI& tft) {
    constexpr int16_t ROW_H = 46;
    constexpr int16_t ROW_GAP = 8;
    constexpr int16_t REMOVE_BTN_W = 60;

    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 10);
        tft.println(I18n::t(StringId::WIFI_NETWORKS_TITLE));

        uint8_t count = WifiMgr::networkCount();
        int16_t y = 40;

        Rect rowRects[Config::MAX_WIFI_NETWORKS];
        Rect removeRects[Config::MAX_WIFI_NETWORKS];

        for (uint8_t i = 0; i < Config::MAX_WIFI_NETWORKS; i++) {
            Rect rowRect = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20 - REMOVE_BTN_W - 6), ROW_H};
            Rect removeRect = {(int16_t)(Config::SCREEN_WIDTH - 10 - REMOVE_BTN_W), y, REMOVE_BTN_W, ROW_H};
            rowRects[i] = rowRect;
            removeRects[i] = removeRect;

            if (i < count) {
                drawButton(tft, rowRect, WifiMgr::networkSsid(i));
                drawButton(tft, removeRect, "X", true);
            } else {
                tft.fillRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_BLACK);
                tft.drawRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_DARKGREEN);
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
                tft.drawString(I18n::t(StringId::WIFI_EMPTY_SLOT), rowRect.x + rowRect.w / 2, rowRect.y + rowRect.h / 2);
                tft.setTextDatum(TL_DATUM);
            }
            y += ROW_H + ROW_GAP;
        }

        Rect addBtn = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        bool canAdd = count < Config::MAX_WIFI_NETWORKS;
        if (canAdd) {
            drawButton(tft, addBtn, I18n::t(StringId::WIFI_ADD_NETWORK));
        }
        y += 48;

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            MenuStars::update(tft);
            delay(20);
        }

        bool handled = false;
        for (uint8_t i = 0; i < count && !handled; i++) {
            if (removeRects[i].contains(tap.x, tap.y)) {
                WifiMgr::removeNetwork(i);
                handled = true;
            }
        }
        if (!handled && canAdd && addBtn.contains(tap.x, tap.y)) {
            WifiSetupScreen::run(tft);
            handled = true;
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}