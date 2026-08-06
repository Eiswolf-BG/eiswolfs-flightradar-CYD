#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace SplashScreen {
    void begin(TFT_eSPI& tft);

    void setStatusLine(TFT_eSPI& tft, uint8_t slot, const String& text, uint16_t color = TFT_WHITE);

    // Zeigt eine tageszeit-abhaengige Begruessung (Morgen/Tag/Abend) unter dem
    // "Flightradar"-Titel an. Braucht eine per NTP synchronisierte Uhrzeit -
    // ist sie noch nicht verfuegbar, wird nichts gezeichnet (kein Raten).
    // Am besten NACH der WLAN-Verbindung + Zeit-Synchronisierung aufrufen,
    // NICHT direkt in begin() (die Uhrzeit steht zu dem Zeitpunkt noch nicht).
    void showGreeting(TFT_eSPI& tft);

    // Blockiert, bis seit begin() mindestens MIN_DISPLAY_MS vergangen sind.
    // Direkt vor dem Verlassen des Splash-Screens aufrufen. Zeichnet waehrend
    // des Wartens dieselbe Sterne-Animation wie die Menues.
    void waitRemaining(TFT_eSPI& tft);
}