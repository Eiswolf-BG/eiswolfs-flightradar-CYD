#include "splash_screen.h"
#include "menu_stars.h"
#include <math.h>

namespace SplashScreen {

namespace {
    constexpr uint32_t MIN_DISPLAY_MS = 5000;
    uint32_t startMs = 0;

    constexpr int16_t STATUS_LINE_H = 18;
    constexpr int16_t STATUS_START_Y = 260;
    constexpr uint8_t MAX_STATUS_LINES = 3;

    void drawRadarReticle(TFT_eSPI& tft, int16_t cx, int16_t cy) {
        uint16_t dim = 0x0320;
        tft.drawCircle(cx, cy, 80, dim);
        tft.drawCircle(cx, cy, 54, dim);
        tft.drawCircle(cx, cy, 29, dim);
        tft.drawFastHLine(cx - 80, cy, 160, dim);
        tft.drawFastVLine(cx, cy - 80, 160, dim);
    }

    // Dreht einen Punkt (px, py) im lokalen Koordinatensystem der kleinen
    // Deko-Symbole um angleDeg und verschiebt ihn nach (x, y).
    void rotatePoint(int16_t x, int16_t y, float sinA, float cosA,
                      float px, float py, int16_t& outX, int16_t& outY) {
        outX = x + (int16_t)lroundf(px * cosA - py * sinA);
        outY = y + (int16_t)lroundf(px * sinA + py * cosA);
    }

    // Kleines Flugzeug-Silhouette aus gefuellten Dreiecken (Rumpf, Tragflaechen,
    // Leitwerk), analog zum grossen Flugzeug in der Mitte, aber schlichter und
    // in beliebiger Richtung (angleDeg, 0 = Nase zeigt nach oben).
    void drawMiniJet(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color, float angleDeg) {
        float rad = angleDeg * (PI / 180.0f);
        float s = sinf(rad), c = cosf(rad);

        int16_t nx, ny, flx, fly, frx, fry;
        rotatePoint(x, y, s, c, 0, -11, nx, ny);
        rotatePoint(x, y, s, c, -3, 11, flx, fly);
        rotatePoint(x, y, s, c, 3, 11, frx, fry);
        tft.fillTriangle(nx, ny, flx, fly, frx, fry, color);

        int16_t wlx, wly, wrx, wry, wcx, wcy;
        rotatePoint(x, y, s, c, -13, 3, wlx, wly);
        rotatePoint(x, y, s, c, 13, 3, wrx, wry);
        rotatePoint(x, y, s, c, 0, -3, wcx, wcy);
        tft.fillTriangle(wlx, wly, wrx, wry, wcx, wcy, color);

        int16_t tlx, tly, trx, try_, ttx, tty;
        rotatePoint(x, y, s, c, -5, 8, tlx, tly);
        rotatePoint(x, y, s, c, 5, 8, trx, try_);
        rotatePoint(x, y, s, c, 0, 13, ttx, tty);
        tft.fillTriangle(tlx, tly, trx, try_, ttx, tty, color);
    }

    // Kleiner Helikopter: Rotorkreis mit Blaettern, gefuellter Rumpf, Heckausleger
    // mit Leitwerk. angleDeg bestimmt die Flugrichtung (0 = Nase nach oben).
    void drawMiniHeli(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color, float angleDeg) {
        float rad = angleDeg * (PI / 180.0f);
        float s = sinf(rad), c = cosf(rad);

        int16_t rcx, rcy;
        rotatePoint(x, y, s, c, 0, -7, rcx, rcy);
        tft.drawCircle(rcx, rcy, 6, color);

        int16_t b1x, b1y, b2x, b2y;
        rotatePoint(x, y, s, c, -7, -7, b1x, b1y);
        rotatePoint(x, y, s, c, 7, -7, b2x, b2y);
        tft.drawLine(b1x, b1y, b2x, b2y, color);
        rotatePoint(x, y, s, c, 0, -13, b1x, b1y);
        rotatePoint(x, y, s, c, 0, -1, b2x, b2y);
        tft.drawLine(b1x, b1y, b2x, b2y, color);

        int16_t cx1, cy1, cx2, cy2, cx3, cy3;
        rotatePoint(x, y, s, c, 0, -6, cx1, cy1);
        rotatePoint(x, y, s, c, -4, 4, cx2, cy2);
        rotatePoint(x, y, s, c, 4, 4, cx3, cy3);
        tft.fillTriangle(cx1, cy1, cx2, cy2, cx3, cy3, color);

        int16_t tbx1, tby1, tbx2, tby2;
        rotatePoint(x, y, s, c, 0, 4, tbx1, tby1);
        rotatePoint(x, y, s, c, 0, 12, tbx2, tby2);
        tft.drawLine(tbx1, tby1, tbx2, tby2, color);

        int16_t fx1, fy1, fx2, fy2, fx3, fy3;
        rotatePoint(x, y, s, c, -3, 10, fx1, fy1);
        rotatePoint(x, y, s, c, 3, 10, fx2, fy2);
        rotatePoint(x, y, s, c, 0, 15, fx3, fy3);
        tft.fillTriangle(fx1, fy1, fx2, fy2, fx3, fy3, color);
    }

    void drawAirplane(TFT_eSPI& tft, int16_t cx) {
        uint16_t color = TFT_GREEN;

        tft.drawTriangle(cx + 15, 142, cx - 15, 200, cx - 10, 202, color);

        tft.drawTriangle(cx + 5, 164, cx - 47, 175, cx - 3, 179, color);
        tft.drawTriangle(cx + 5, 164, cx + 30, 211, cx - 3, 179, color);

        tft.drawTriangle(cx - 11, 198, cx - 23, 201, cx - 6, 210, color);
    }
}

void begin(TFT_eSPI& tft) {
    startMs = millis();
    MenuStars::reset();

    int16_t cx = tft.width() / 2;

    tft.fillScreen(TFT_BLACK);
    drawRadarReticle(tft, cx, 174);

    uint16_t dimGreen = 0x0320;
    drawMiniJet(tft, cx - 45, 135, dimGreen, 25.0f);
    drawMiniJet(tft, cx + 50, 160, dimGreen, 160.0f);
    drawMiniHeli(tft, cx - 35, 215, dimGreen, 250.0f);
    drawMiniHeli(tft, cx + 45, 115, dimGreen, 100.0f);

    drawAirplane(tft, cx);

    tft.setTextDatum(MC_DATUM);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Eiswolfs", cx, 28);
    tft.drawString("Flightradar", cx, 60);

    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
}

void setStatusLine(TFT_eSPI& tft, uint8_t slot, const String& text, uint16_t color) {
    if (slot >= MAX_STATUS_LINES) return;

    int16_t y = STATUS_START_Y + slot * STATUS_LINE_H;
    tft.fillRect(0, y, tft.width(), STATUS_LINE_H, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(text, tft.width() / 2, y + STATUS_LINE_H / 2);
    tft.setTextDatum(TL_DATUM);
}

void waitRemaining(TFT_eSPI& tft) {
    uint32_t elapsed = millis() - startMs;
    while (elapsed < MIN_DISPLAY_MS) {
        MenuStars::update(tft);
        delay(20);
        elapsed = millis() - startMs;
    }
}

}