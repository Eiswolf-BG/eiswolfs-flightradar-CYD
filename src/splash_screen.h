#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace SplashScreen {
    void begin(TFT_eSPI& tft);

    void setStatusLine(TFT_eSPI& tft, uint8_t slot, const String& text, uint16_t color = TFT_WHITE);

    // Blockiert, bis seit begin() mindestens MIN_DISPLAY_MS vergangen sind.
    // Direkt vor dem Verlassen des Splash-Screens aufrufen. Zeichnet waehrend
    // des Wartens dieselbe Sterne-Animation wie die Menues.
    void waitRemaining(TFT_eSPI& tft);
}