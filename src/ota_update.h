#pragma once
#include <Arduino.h>

// Firmware-Update ueber WLAN (Menue > System > "Nach Update suchen") -
// prueft gegen die neueste Version im GitHub-Repository (Releases-API) und
// installiert sie bei Bestaetigung direkt auf dem Geraet, ohne Kabel/
// Web-Flasher. Blockierende, synchrone HTTPS-Aufrufe - werden nur bei
// explizitem Tastendruck ausgefuehrt (kein Hintergrund-Polling), analog zu
// AircraftDetails' Modell-/Routen-Abfragen, nur eben auf Core 1 statt
// Core 0, da hier direkt auf Nutzer-Interaktion reagiert wird.
namespace OtaUpdate {

    enum class CheckResult { Error, UpToDate, UpdateAvailable };

    struct CheckInfo {
        CheckResult result = CheckResult::Error;
        // Neueste verfuegbare Version OHNE "v"-Praefix (z.B. "3.5.0") -
        // sowohl bei UpToDate als auch bei UpdateAvailable gesetzt, damit
        // der aufrufende Screen sie in beiden Faellen anzeigen kann.
        char latestVersion[16] = {0};
        // Direkter Download-Link zur CYD-flightradar.bin des Releases -
        // nur gesetzt, wenn result == UpdateAvailable.
        char downloadUrl[192] = {0};
    };

    // Fragt die GitHub-Releases-API nach dem neuesten Release ab, vergleicht
    // dessen Versionsnummer (Tag-Name, "v"-Praefix wird ignoriert) gegen
    // Config::APP_VERSION und sucht im Release den Anhang
    // "CYD-flightradar.bin". CheckResult::Error bei jedem Fehler unterwegs
    // (kein WLAN, Zeitueberschreitung, unerwartetes JSON, kein
    // CYD-flightradar.bin im Release).
    CheckInfo checkForUpdate();

    // Laedt die CYD-flightradar.bin von 'url' herunter und flasht sie (ueber die
    // Standard-Arduino-HTTPUpdate-Bibliothek). Ruft bei Erfolg BEWUSST
    // NICHT selbst ESP.restart() auf - der Aufrufer zeigt zuerst eine
    // kurze Erfolgsmeldung an und startet danach selbst neu. onProgress
    // wird waehrend des Downloads wiederholt mit 0-100 (Prozent) aufgerufen
    // (darf nullptr sein). Gibt false zurueck, wenn Download oder Flash-
    // Vorgang fehlschlagen - das Geraet laeuft in dem Fall unveraendert mit
    // der bisherigen Firmware weiter.
    bool performUpdate(const char* url, void (*onProgress)(uint8_t percent));
}
