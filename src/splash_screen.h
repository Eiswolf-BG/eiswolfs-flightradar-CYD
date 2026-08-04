#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace SplashScreen {
    void begin(TFT_eSPI& tft);
    void setStatusLine(TFT_eSPI& tft, uint8_t slot, const String& text, uint16_t color = TFT_WHITE);
    void waitRemaining();
}
