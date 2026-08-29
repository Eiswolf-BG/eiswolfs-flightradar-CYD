#include "splash_screen.h"
#include "menu_stars.h"
#include "radar_logo.h"
#include "i18n.h"
#include <math.h>

namespace SplashScreen {

namespace {
    constexpr uint32_t MIN_DISPLAY_MS = 5000;
    uint32_t startMs = 0;

    constexpr int16_t STATUS_LINE_H = 18;
    constexpr int16_t STATUS_START_Y = 260;
    constexpr uint8_t MAX_STATUS_LINES = 3;

    // Reiner Flavour-Text (kein echter Statuswert wie bei den SPLASH_*-
    // Strings oben) - Reihenfolge muss zur Reihenfolge von BOOT_SEQ_1..6 in
    // i18n.h passen.
    constexpr StringId BOOT_LINES[] = {
        StringId::BOOT_SEQ_1, StringId::BOOT_SEQ_2, StringId::BOOT_SEQ_3,
        StringId::BOOT_SEQ_4, StringId::BOOT_SEQ_5, StringId::BOOT_SEQ_6,
    };
    constexpr uint8_t BOOT_LINE_COUNT = sizeof(BOOT_LINES) / sizeof(BOOT_LINES[0]);
    constexpr uint32_t BOOT_LINE_DELAY_MS = 350;
    constexpr uint32_t BOOT_FINAL_PAUSE_MS = 300;
    constexpr int16_t BOOT_START_X = 14;
    constexpr int16_t BOOT_START_Y = 40;
    constexpr int16_t BOOT_LINE_H = 22;
}

void playBootSequence(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);

    for (uint8_t i = 0; i < BOOT_LINE_COUNT; i++) {
        int16_t y = BOOT_START_Y + i * BOOT_LINE_H;
        String line = String("> ") + I18n::t(BOOT_LINES[i]);
        tft.drawString(line, BOOT_START_X, y);
        delay(BOOT_LINE_DELAY_MS);
    }
    delay(BOOT_FINAL_PAUSE_MS);
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