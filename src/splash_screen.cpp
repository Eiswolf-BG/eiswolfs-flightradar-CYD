#include "splash_screen.h"
#include "menu_stars.h"
#include "radar_logo.h"
#include <math.h>

namespace SplashScreen {

namespace {
    constexpr uint32_t MIN_DISPLAY_MS = 5000;
    uint32_t startMs = 0;

    constexpr int16_t STATUS_LINE_H = 18;
    constexpr int16_t STATUS_START_Y = 260;
    constexpr uint8_t MAX_STATUS_LINES = 3;
}

void begin(TFT_eSPI& tft) {
    startMs = millis();
    MenuStars::reset();

    int16_t cx = tft.width() / 2;

    tft.fillScreen(TFT_BLACK);
    RadarLogo::draw(tft, cx, 174);

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