#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Display-Helligkeit einstellen (10-100% in 10%-Schritten). Wirkt sich
// sofort live auf die Hintergrundbeleuchtung aus, damit man die Aenderung
// beim Antippen direkt sieht.
namespace BrightnessScreen {
    void run(TFT_eSPI& tft);
}
