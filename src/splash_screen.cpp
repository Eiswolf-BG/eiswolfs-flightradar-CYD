#include "splash_screen.h"

namespace SplashScreen {

namespace {
    constexpr uint32_t MIN_DISPLAY_MS = 5000;
    uint32_t startMs = 0;

    constexpr int16_t STATUS_LINE_H = 18;
    constexpr int16_t STATUS_START_Y = 260;
    constexpr uint8_t MAX_STATUS_LINES = 3;

    // Dezente konzentrische Ringe + Fadenkreuz als "Zielfernrohr"-Hintergrund,
    // in gedaempftem Gruen (gleicher Ton wie die Sweep-Nachzieh-Linie im
    // Radar-Bildschirm), damit das Flugzeug wie ins Visier genommen wirkt.
    // UNVERAENDERT gegenueber vorher - nur das Flugzeug wurde verkleinert/gedreht.
    void drawRadarReticle(TFT_eSPI& tft, int16_t cx, int16_t cy) {
        uint16_t dim = 0x0320;
        tft.drawCircle(cx, cy, 80, dim);
        tft.drawCircle(cx, cy, 54, dim);
        tft.drawCircle(cx, cy, 29, dim);
        tft.drawFastHLine(cx - 80, cy, 160, dim);
        tft.drawFastVLine(cx, cy - 80, 160, dim);
    }

    // Flugzeug-Silhouette: 40% kleiner (Skalierung 0.6) und 25 Grad im
    // Uhrzeigersinn gedreht (Nase zeigt jetzt leicht nach rechts), um den
    // Reticle-Mittelpunkt (cx, 174) - der Radar-Kreis selbst bleibt exakt
    // gleich gross wie vorher.
    void drawAirplane(TFT_eSPI& tft, int16_t cx) {
        uint16_t color = TFT_GREEN;

        // Rumpf (nur Umriss)
        tft.drawTriangle(cx + 15, 142, cx - 15, 200, cx - 10, 202, color);

        // Deltafluegel (nur Umriss)
        tft.drawTriangle(cx + 5, 164, cx - 47, 175, cx - 3, 179, color);
        tft.drawTriangle(cx + 5, 164, cx + 30, 211, cx - 3, 179, color);

        // Leitwerk (nur Umriss)
        tft.drawTriangle(cx - 11, 198, cx - 23, 201, cx - 6, 210, color);
    }
}

void begin(TFT_eSPI& tft) {
    startMs = millis();

    int16_t cx = tft.width() / 2;

    tft.fillScreen(TFT_BLACK);
    drawRadarReticle(tft, cx, 174);
    drawAirplane(tft, cx);

    tft.setTextDatum(MC_DATUM);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3);
    tft.drawString("Eiswolfs", cx, 40);

    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Flightradar", cx, 78);

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

void waitRemaining() {
    uint32_t elapsed = millis() - startMs;
    if (elapsed < MIN_DISPLAY_MS) {
        delay(MIN_DISPLAY_MS - elapsed);
    }
}

}