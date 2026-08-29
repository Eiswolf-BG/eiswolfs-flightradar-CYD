#include "radar_logo.h"
#include "ui_theme.h"
#include <math.h>

namespace RadarLogo {

namespace {
    int16_t sc(float v, float scale) {
        return (int16_t)lroundf(v * scale);
    }

    void drawRadarReticle(TFT_eSPI& tft, int16_t cx, int16_t cy, float scale) {
        uint16_t dim = UiTheme::accentColorDimmed(tft, 0.4f);
        int16_t r1 = sc(80, scale), r2 = sc(54, scale), r3 = sc(29, scale);
        tft.drawCircle(cx, cy, r1, dim);
        tft.drawCircle(cx, cy, r2, dim);
        tft.drawCircle(cx, cy, r3, dim);
        tft.drawFastHLine((int16_t)(cx - r1), cy, (int16_t)(r1 * 2), dim);
        tft.drawFastVLine(cx, (int16_t)(cy - r1), (int16_t)(r1 * 2), dim);
    }

    void rotatePoint(int16_t x, int16_t y, float sinA, float cosA,
                      float px, float py, float scale, int16_t& outX, int16_t& outY) {
        px *= scale;
        py *= scale;
        outX = x + (int16_t)lroundf(px * cosA - py * sinA);
        outY = y + (int16_t)lroundf(px * sinA + py * cosA);
    }

    void drawMiniJet(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color, float angleDeg, float scale) {
        float rad = angleDeg * (PI / 180.0f);
        float s = sinf(rad), c = cosf(rad);

        int16_t nx, ny, flx, fly, frx, fry;
        rotatePoint(x, y, s, c, 0, -11, scale, nx, ny);
        rotatePoint(x, y, s, c, -3, 11, scale, flx, fly);
        rotatePoint(x, y, s, c, 3, 11, scale, frx, fry);
        tft.fillTriangle(nx, ny, flx, fly, frx, fry, color);

        int16_t wlx, wly, wrx, wry, wcx, wcy;
        rotatePoint(x, y, s, c, -13, 3, scale, wlx, wly);
        rotatePoint(x, y, s, c, 13, 3, scale, wrx, wry);
        rotatePoint(x, y, s, c, 0, -3, scale, wcx, wcy);
        tft.fillTriangle(wlx, wly, wrx, wry, wcx, wcy, color);

        int16_t tlx, tly, trx, try_, ttx, tty;
        rotatePoint(x, y, s, c, -5, 8, scale, tlx, tly);
        rotatePoint(x, y, s, c, 5, 8, scale, trx, try_);
        rotatePoint(x, y, s, c, 0, 13, scale, ttx, tty);
        tft.fillTriangle(tlx, tly, trx, try_, ttx, tty, color);
    }

    void drawMiniHeli(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color, float angleDeg, float scale) {
        float rad = angleDeg * (PI / 180.0f);
        float s = sinf(rad), c = cosf(rad);

        int16_t rcx, rcy;
        rotatePoint(x, y, s, c, 0, -7, scale, rcx, rcy);
        int16_t rotorR = sc(6, scale);
        if (rotorR < 1) rotorR = 1;
        tft.drawCircle(rcx, rcy, rotorR, color);

        int16_t b1x, b1y, b2x, b2y;
        rotatePoint(x, y, s, c, -7, -7, scale, b1x, b1y);
        rotatePoint(x, y, s, c, 7, -7, scale, b2x, b2y);
        tft.drawLine(b1x, b1y, b2x, b2y, color);
        rotatePoint(x, y, s, c, 0, -13, scale, b1x, b1y);
        rotatePoint(x, y, s, c, 0, -1, scale, b2x, b2y);
        tft.drawLine(b1x, b1y, b2x, b2y, color);

        int16_t cx1, cy1, cx2, cy2, cx3, cy3;
        rotatePoint(x, y, s, c, 0, -6, scale, cx1, cy1);
        rotatePoint(x, y, s, c, -4, 4, scale, cx2, cy2);
        rotatePoint(x, y, s, c, 4, 4, scale, cx3, cy3);
        tft.fillTriangle(cx1, cy1, cx2, cy2, cx3, cy3, color);

        int16_t tbx1, tby1, tbx2, tby2;
        rotatePoint(x, y, s, c, 0, 4, scale, tbx1, tby1);
        rotatePoint(x, y, s, c, 0, 12, scale, tbx2, tby2);
        tft.drawLine(tbx1, tby1, tbx2, tby2, color);

        int16_t fx1, fy1, fx2, fy2, fx3, fy3;
        rotatePoint(x, y, s, c, -3, 10, scale, fx1, fy1);
        rotatePoint(x, y, s, c, 3, 10, scale, fx2, fy2);
        rotatePoint(x, y, s, c, 0, 15, scale, fx3, fy3);
        tft.fillTriangle(fx1, fy1, fx2, fy2, fx3, fy3, color);
    }

    void drawAirplane(TFT_eSPI& tft, int16_t cx, int16_t cy, float scale, uint16_t color) {
        auto px = [&](float dx) { return (int16_t)(cx + sc(dx, scale)); };
        auto py = [&](float dy) { return (int16_t)(cy + sc(dy, scale)); };

        tft.drawTriangle(px(15), py(-32), px(-15), py(26), px(-10), py(28), color);

        tft.drawTriangle(px(5), py(-10), px(-47), py(1), px(-3), py(5), color);
        tft.drawTriangle(px(5), py(-10), px(30), py(37), px(-3), py(5), color);

        tft.drawTriangle(px(-11), py(24), px(-23), py(27), px(-6), py(36), color);
    }
}

void draw(TFT_eSPI& tft, int16_t cx, int16_t cy, float scale) {
    drawRadarReticle(tft, cx, cy, scale);

    uint16_t dimAccent = UiTheme::accentColorDimmed(tft, 0.4f);
    auto mx = [&](float dx) { return (int16_t)(cx + sc(dx, scale)); };
    auto my = [&](float dy) { return (int16_t)(cy + sc(dy, scale)); };

    drawMiniJet(tft, mx(-45), my(-39), dimAccent, 25.0f, scale);
    drawMiniJet(tft, mx(50), my(-14), dimAccent, 160.0f, scale);
    drawMiniHeli(tft, mx(-35), my(41), dimAccent, 250.0f, scale);
    drawMiniHeli(tft, mx(45), my(-59), dimAccent, 100.0f, scale);

    drawAirplane(tft, cx, cy, scale, UiTheme::accentColor(tft));
}

}
