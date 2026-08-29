#include "calibration_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "ui_theme.h"

namespace CalibrationScreen {

namespace {
    struct RawAvg {
        long sumX = 0, sumY = 0;
        uint16_t n = 0;
        void add(int16_t x, int16_t y) { sumX += x; sumY += y; n++; }
        int16_t avgX() const { return n ? (int16_t)(sumX / n) : 0; }
        int16_t avgY() const { return n ? (int16_t)(sumY / n) : 0; }
    };

    // Mini radar target: same look as the radar graphic on the splash/
    // welcome screen (concentric rings + crosshair in green), but without
    // aircraft - instead a green filled center point as the point to aim
    // at, replacing the previous red filled circle with white center dot.
    // Radii deliberately kept small so the targets in the screen corners
    // (edge margin M=24, see run()) don't extend past the screen edge.
    void drawTarget(TFT_eSPI& tft, int16_t x, int16_t y) {
        uint16_t dim = 0x0320;
        tft.drawCircle(x, y, 16, dim);
        tft.drawCircle(x, y, 10, dim);
        tft.drawFastHLine((int16_t)(x - 18), y, 36, UiTheme::accentColor(tft));
        tft.drawFastVLine(x, (int16_t)(y - 18), 36, UiTheme::accentColor(tft));
        tft.fillCircle(x, y, 3, UiTheme::accentColor(tft));
    }

    RawAvg waitForTap(TFT_eSPI& tft) {
        RawAvg avg;

        while (!TouchInput::rawPoint().touched) {
            MenuStars::update(tft);
            delay(10);
        }

        uint32_t sampleStart = millis();
        while (millis() - sampleStart < 250) {
            TouchInput::Point p = TouchInput::rawPoint();
            if (p.touched) avg.add(p.x, p.y);
            delay(10);
        }

        while (TouchInput::rawPoint().touched) {
            delay(10);
        }
        delay(150);

        return avg;
    }

    void swapIfNeeded(int16_t& lo, int16_t& hi) {
        if (lo > hi) { int16_t t = lo; lo = hi; hi = t; }
    }
}

void run(TFT_eSPI& tft) {
    const int16_t M = 24;
    const int16_t W = Config::SCREEN_WIDTH;
    const int16_t H = Config::SCREEN_HEIGHT;

    struct { int16_t x, y; StringId label; } targets[4] = {
        { M,     M,     StringId::CALIB_TOP_LEFT },
        { W - M, M,     StringId::CALIB_TOP_RIGHT },
        { W - M, H - M, StringId::CALIB_BOTTOM_RIGHT },
        { M,     H - M, StringId::CALIB_BOTTOM_LEFT },
    };

    RawAvg samples[4];

    for (uint8_t i = 0; i < 4; i++) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(StringId::CALIB_TITLE), W / 2, H / 2 - 12);
        tft.drawString(I18n::t(StringId::CALIB_PROMPT), W / 2, H / 2 + 4);
        tft.drawString(I18n::t(targets[i].label), W / 2, H / 2 + 20);
        tft.setTextDatum(TL_DATUM);

        drawTarget(tft, targets[i].x, targets[i].y);
        MenuStars::reset();
        samples[i] = waitForTap(tft);
    }

    int16_t xmin = (samples[0].avgX() + samples[3].avgX()) / 2;
    int16_t xmax = (samples[1].avgX() + samples[2].avgX()) / 2;
    int16_t ymin = (samples[0].avgY() + samples[1].avgY()) / 2;
    int16_t ymax = (samples[3].avgY() + samples[2].avgY()) / 2;

    swapIfNeeded(xmin, xmax);
    swapIfNeeded(ymin, ymax);

    TouchInput::setCalibration(xmin, xmax, ymin, ymax);
    TouchInput::saveCalibration();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::CALIB_SAVED));
    delay(800);
}

}