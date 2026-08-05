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
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::WIFI_INFO_PARA1), 0, 0, 0, false);
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::WIFI_INFO_PARA2), 0, 0, 0, false);
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::WIFI_INFO_PARA3), 0, 0, 0, false);

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
            tft.println(I18n::t(StringId::WIFI_INFO_TITLE));

            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            int16_t y = VIEW_TOP;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::WIFI_INFO_PARA1), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::WIFI_INFO_PARA2), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::WIFI_INFO_PARA3), scrollY, VIEW_TOP, VIEW_BOTTOM, true);

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
    constexpr int16_t ROW_H = 46;
    constexpr int16_t ROW_GAP = 8;
    constexpr int16_t REMOVE_BTN_W = 60;

    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::WIFI_NETWORKS_TITLE));

        Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
        drawButton(tft, infoBtn, "?");

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
        if (infoBtn.contains(tap.x, tap.y)) {
            runInfoScreen(tft);
            handled = true;
        }
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