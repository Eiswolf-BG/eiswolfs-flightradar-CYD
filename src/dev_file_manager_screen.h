#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Reines Entwickler-Werkzeug (Alex' eigener Wunsch) - erreichbar nur ueber
// das 6-fache Antippen des QR-Codes im "Ueber"-Screen (main.cpp::
// runGithubQrScreen()), sonst nirgendwo im Menue verlinkt und bewusst nicht
// dokumentiert. Zeigt den Inhalt der SD-Karte (Dateien/Ordner, Navigation,
// Loeschen) fuer Fehlersuche direkt am Geraet, ohne die Karte ausbauen zu
// muessen.
namespace DevFileManagerScreen {
    void run(TFT_eSPI& tft);
}
