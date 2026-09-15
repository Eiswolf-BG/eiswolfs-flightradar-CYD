#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Einstell-Screen fuer die optionale ntfy.sh-Push-Benachrichtigung
// (Menue > System > Werkzeuge > "ntfy.sh Push", SettingsStore::
// ntfyPushEnabled(), AUS per Default) - fuer Nutzer, die bei einem Notfall-
// Squawk oder Watchlist-Treffer eine Push-Benachrichtigung aufs Handy
// bekommen wollen, ohne App-Konto/Anmeldung. Siehe ntfy_push.h fuer die
// eigentliche Versandlogik.
namespace NtfyPushScreen {
    void run(TFT_eSPI& tft);
}
