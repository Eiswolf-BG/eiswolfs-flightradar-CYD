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
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();

    constexpr int16_t textMaxWidth = Config::SCREEN_WIDTH - 20;
    constexpr int16_t LINE_H = 16;
    constexpr int16_t VIEW_TOP = 36;

    // Wenn WLAN verbunden ist, bekommt der Absatztext nur noch ein kleines
    // (bei Bedarf scrollbares) Fenster, weil darunter fest der QR-Code samt
    // IP-Adresse Platz braucht (siehe Alex' Feedback: der vorherige separate
    // "QR"-Button oben rechts sah seltsam aus - jetzt wird der Code direkt
    // hier gezeigt, mittig, mit der IP-Adresse mittig darunter). Ohne WLAN
    // gibt es keinen QR-Code, der Text bekommt dann wie vorher die volle
    // Bildschirmhoehe.
    constexpr int16_t TEXT_VIEW_BOTTOM_CONNECTED = 84;
    constexpr int16_t TEXT_VIEW_BOTTOM_DISCONNECTED = Config::SCREEN_HEIGHT - 60;

    bool wifiConnected = WiFi.status() == WL_CONNECTED;
    String urlLine = wifiConnected ? ("http://" + WiFi.localIP().toString() + "/") : String();
    int16_t viewBottom = wifiConnected ? TEXT_VIEW_BOTTOM_CONNECTED : TEXT_VIEW_BOTTOM_DISCONNECTED;

    // QR-Code einmalig erzeugen (die URL aendert sich waehrend dieser
    // Bildschirm offen ist nicht). Version 4 (33x33 Module) reicht mit
    // deutlicher Reserve fuer eine "http://<lokale-IP>/"-URL (max. rund 24
    // Zeichen) - ECC_LOW statt eines hoeheren Fehlerkorrektur-Levels, um die
    // Module moeglichst gross (und damit leicht scannbar) darstellen zu
    // koennen.
    constexpr uint8_t QR_VERSION = 4;
    constexpr int16_t QR_SIZE_MODULES = 33; // Version 4: 4*4+17 = 33
    constexpr int16_t QR_BLOCK = 4;
    constexpr int16_t QR_QUIET = 2; // Ruhezone in Modulen rundherum, Scanner brauchen etwas Rand
    constexpr int16_t QR_PIXEL_SIZE = (QR_SIZE_MODULES + 2 * QR_QUIET) * QR_BLOCK;
    constexpr int16_t QR_X = (Config::SCREEN_WIDTH - QR_PIXEL_SIZE) / 2;
    constexpr int16_t QR_GAP = 6;
    constexpr int16_t QR_Y = TEXT_VIEW_BOTTOM_CONNECTED + QR_GAP;
    constexpr int16_t IP_TEXT_Y = QR_Y + QR_PIXEL_SIZE + QR_GAP + 8;

    uint8_t qrData[qrcode_getBufferSize(QR_VERSION)];
    QRCode qrcode;
    if (wifiConnected) {
        qrcode_initText(&qrcode, qrData, QR_VERSION, ECC_LOW, urlLine.c_str());
    }

    // Gesamthoehe des Absatztextes vorab berechnen (draw=false), um zu
    // wissen, ob Scroll-Pfeile gebraucht werden - muss exakt zur
    // Zeichenreihenfolge in redraw() unten passen.
    int16_t totalH = VIEW_TOP;
    totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::WEBUI_INFO_PARA1), 0, 0, 0, false);
    if (!wifiConnected) {
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::WEBUI_INFO_PARA2), 0, 0, 0, false);
    }

    int16_t maxScroll = totalH - viewBottom;
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
        tft.println(I18n::t(StringId::WEBUI_TITLE));

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = VIEW_TOP;
        y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::WEBUI_INFO_PARA1), scrollY, VIEW_TOP, viewBottom, true);
        if (!wifiConnected) {
            y += 8;
            layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::WEBUI_INFO_PARA2), scrollY, VIEW_TOP, viewBottom, true);
        }

        if (wifiConnected) {
            // QR-Code + IP-Adresse stehen fest unterhalb des (ggf.
            // scrollbaren) Textes, unabhaengig von scrollY - beides mittig
            // zentriert, direkt ueber dem Zurueck-Button.
            tft.fillRect(QR_X, QR_Y, QR_PIXEL_SIZE, QR_PIXEL_SIZE, TFT_WHITE);
            for (uint8_t my = 0; my < qrcode.size; my++) {
                for (uint8_t mx = 0; mx < qrcode.size; mx++) {
                    if (qrcode_getModule(&qrcode, mx, my)) {
                        int16_t px = (int16_t)(QR_X + (QR_QUIET + mx) * QR_BLOCK);
                        int16_t py = (int16_t)(QR_Y + (QR_QUIET + my) * QR_BLOCK);
                        tft.fillRect(px, py, QR_BLOCK, QR_BLOCK, TFT_BLACK);
                    }
                }
            }

            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(urlLine, Config::SCREEN_WIDTH / 2, IP_TEXT_Y);
            tft.setTextDatum(TL_DATUM);
        }

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
            if (backBtn.contains(tap.x, tap.y)) {
                return;
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
