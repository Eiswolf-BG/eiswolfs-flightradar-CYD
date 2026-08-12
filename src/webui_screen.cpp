#include "webui_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include <WiFi.h>
#include <qrcode.h>

namespace WebUiScreen {

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

    // QR-Code-Unterscreen: zeigt die WebUI-URL als QR-Code, damit man sie
    // nicht mehr von Hand ins Handy tippen muss. Nutzt die ricmoo/QRCode-
    // Bibliothek (kein dynamisches Allozieren, Version fest auf 4/33x33
    // Module mit niedrigster Fehlerkorrektur, reicht locker fuer eine kurze
    // "http://192.168.x.x/"-URL).
    void runQrScreen(TFT_eSPI& tft, const String& url) {
        QRCode qrcode;
        uint8_t qrData[qrcode_getBufferSize(4)];
        qrcode_initText(&qrcode, qrData, 4, ECC_LOW, url.c_str());

        constexpr int16_t MODULE_PX = 6;
        constexpr int16_t QUIET_ZONE = 4;
        int16_t qrPx = qrcode.size * MODULE_PX;
        int16_t boxPx = qrPx + 2 * QUIET_ZONE * MODULE_PX;
        int16_t boxX = (Config::SCREEN_WIDTH - boxPx) / 2;
        int16_t boxY = 30;

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::WEBUI_TITLE));

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(I18n::t(StringId::WEBUI_QR_HINT), Config::SCREEN_WIDTH / 2, boxY - 6);
        tft.setTextDatum(TL_DATUM);

        tft.fillRect(boxX, boxY + 10, boxPx, boxPx, TFT_WHITE);
        for (uint8_t y = 0; y < qrcode.size; y++) {
            for (uint8_t x = 0; x < qrcode.size; x++) {
                if (qrcode_getModule(&qrcode, x, y)) {
                    int16_t px = boxX + QUIET_ZONE * MODULE_PX + x * MODULE_PX;
                    int16_t py = boxY + 10 + QUIET_ZONE * MODULE_PX + y * MODULE_PX;
                    tft.fillRect(px, py, MODULE_PX, MODULE_PX, TFT_BLACK);
                }
            }
        }

        int16_t urlY = boxY + 10 + boxPx + 16;
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(url, Config::SCREEN_WIDTH / 2, urlY);
        tft.setTextDatum(TL_DATUM);

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (backBtn.contains(tap.x, tap.y)) return;
            }
            MenuStars::update(tft);
            delay(20);
        }
    }
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();

    constexpr int16_t textMaxWidth = Config::SCREEN_WIDTH - 20;
    constexpr int16_t LINE_H = 16;
    constexpr int16_t VIEW_TOP = 36;
    constexpr int16_t VIEW_BOTTOM = Config::SCREEN_HEIGHT - 60;

    bool wifiConnected = WiFi.status() == WL_CONNECTED;
    String urlLine = wifiConnected ? ("http://" + WiFi.localIP().toString() + "/") : String();

    // Gesamthoehe vorab berechnen (draw=false), um zu wissen, ob Scroll-
    // Pfeile gebraucht werden - muss exakt zur Zeichenreihenfolge in
    // redraw() unten passen.
    int16_t totalH = VIEW_TOP;
    totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::WEBUI_INFO_PARA1), 0, 0, 0, false);
    totalH += 8;
    if (wifiConnected) {
        totalH += LINE_H;
    } else {
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::WEBUI_INFO_PARA2), 0, 0, 0, false);
    }

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

    Rect qrBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};

    auto redraw = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::WEBUI_TITLE));

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = VIEW_TOP;
        y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::WEBUI_INFO_PARA1), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
        y += 8;
        if (wifiConnected) {
            int16_t screenY = y - scrollY;
            if (screenY >= VIEW_TOP && screenY <= VIEW_BOTTOM) {
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.setCursor(10, screenY);
                tft.print(urlLine);
            }
        } else {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::WEBUI_INFO_PARA2), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
        }

        drawButton(tft, backBtn, I18n::t(StringId::BACK));
        if (scrollable) {
            drawButton(tft, upBtn, "^");
            drawButton(tft, downBtn, "v");
        }
        if (wifiConnected) {
            drawButton(tft, qrBtn, "QR");
        }
    };

    redraw();

    while (true) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            if (backBtn.contains(tap.x, tap.y)) return;
            if (wifiConnected && qrBtn.contains(tap.x, tap.y)) {
                runQrScreen(tft, urlLine);
                redraw();
            } else if (scrollable && upBtn.contains(tap.x, tap.y) && scrollY > 0) {
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
