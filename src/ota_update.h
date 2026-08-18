#pragma once
#include <Arduino.h>

// Firmware-Update ueber WLAN (Menue > System > "Nach Update suchen") -
// prueft gegen die neueste Version im GitHub-Repository (Releases-API) und
// installiert sie bei Bestaetigung direkt auf dem Geraet, ohne Kabel/
// Web-Flasher. Der manuelle Check+Install-Ablauf (checkForUpdate() +
// performUpdate()) bleibt blockierend/synchron und laeuft auf Core 1, direkt
// als Reaktion auf einen Tastendruck (siehe menu_screen.cpp::
// runOtaUpdateScreen()). ZUSAETZLICH prueft pollBackground() periodisch und
// unauffaellig im Hintergrund auf Core 0 (siehe net_task.cpp) - findet sie
// ein Update, zeigt die UI an mehreren Stellen einen kleinen roten Punkt
// (Menue-Button, "System"-Kachel, Update-Button selbst, Ruhebildschirm),
// installiert aber NIE von selbst - das bleibt immer ein bewusster,
// bestaetigter Tastendruck.
namespace OtaUpdate {

    enum class CheckResult { Error, UpToDate, UpdateAvailable };

    struct CheckInfo {
        CheckResult result = CheckResult::Error;
        // Neueste verfuegbare Version OHNE "v"-Praefix (z.B. "3.5.0") -
        // sowohl bei UpToDate als auch bei UpdateAvailable gesetzt, damit
        // der aufrufende Screen sie in beiden Faellen anzeigen kann.
        char latestVersion[16] = {0};
        // Direkter Download-Link zur firmware.bin des Releases - nur
        // gesetzt, wenn result == UpdateAvailable.
        char downloadUrl[192] = {0};
    };

    // Fragt die GitHub-Releases-API nach dem neuesten Release ab, vergleicht
    // dessen Versionsnummer (Tag-Name, "v"-Praefix wird ignoriert) gegen
    // Config::APP_VERSION und sucht im Release den Anhang "firmware.bin".
    // CheckResult::Error bei jedem Fehler unterwegs (kein WLAN, Zeitueber-
    // schreitung, unerwartetes JSON, kein firmware.bin im Release).
    CheckInfo checkForUpdate();

    // Wird regelmaessig aus dem NetTask (Core 0) aufgerufen (siehe
    // net_task.cpp) - kuemmert sich intern um das Abfrage-Intervall
    // (Config::OTA_BACKGROUND_CHECK_INTERVAL_MS) und ruft bei Faelligkeit
    // checkForUpdate() auf. Anders als der manuelle Button-Ablauf unten KEIN
    // eigenes NetTask::pause()/resume() noetig - laeuft ja bereits ALS TEIL
    // des NetTask-Loops, also ohnehin seriell zu ADS-B/Wetter/WebUI, keine
    // gleichzeitige zweite HTTPS-Verbindung wie beim Core-1-Button.
    void pollBackground();

    // Ob die letzte (Hintergrund- ODER manuelle) Pruefung ein neueres
    // Release gefunden hat - threadsicher genug fuer diesen Zweck (nur vom
    // NetTask geschrieben, vom UI-Thread nur gelesen, analog zu
    // Weather::current(), siehe weather.h - ein kurzfristig veraltetes
    // Lesen ist hier voellig unkritisch).
    bool isUpdateAvailable();

    // Versionsnummer des gefundenen Updates ohne "v"-Praefix (z.B. "3.8.0"),
    // leerer String wenn isUpdateAvailable() false liefert.
    const char* availableVersion();

    // Laedt die firmware.bin von 'url' herunter und flasht sie (ueber die
    // Standard-Arduino-HTTPUpdate-Bibliothek). Ruft bei Erfolg BEWUSST
    // NICHT selbst ESP.restart() auf - der Aufrufer zeigt zuerst eine
    // kurze Erfolgsmeldung an und startet danach selbst neu. onProgress
    // wird waehrend des Downloads wiederholt mit 0-100 (Prozent) aufgerufen
    // (darf nullptr sein). Gibt false zurueck, wenn Download oder Flash-
    // Vorgang fehlschlagen - das Geraet laeuft in dem Fall unveraendert mit
    // der bisherigen Firmware weiter.
    bool performUpdate(const char* url, void (*onProgress)(uint8_t percent));
}
