#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Einfacher Info-Screen: Projektname, Kurzbeschreibung, Copyright-Jahr und
// die aktuelle Versionsnummer (Config::APP_VERSION).
namespace AboutScreen {
    void run(TFT_eSPI& tft);
}
