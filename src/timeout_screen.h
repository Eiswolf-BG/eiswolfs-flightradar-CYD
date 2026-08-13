#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Bildschirm-Timeout einstellen (Menue > System > Bildschirm-Timeout) -
// Schieberegler von Config::SCREEN_TIMEOUT_MIN_MINUTES bis
// Config::SCREEN_TIMEOUT_MAX_MINUTES, danach "Nie" als eigene Endposition.
// Der Ruhebildschirm-Umschalter (siehe SettingsStore::screensaverEnabled())
// lebt ebenfalls hier, direkt unter dem Regler, da er inhaltlich eng mit
// dem Timeout zusammenhaengt (bestimmt nur, WAS beim Timeout passiert).
namespace TimeoutScreen {
    void run(TFT_eSPI& tft);
}
