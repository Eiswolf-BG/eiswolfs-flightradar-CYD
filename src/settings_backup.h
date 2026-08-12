#pragma once
#include <Arduino.h>

namespace SettingsBackup {
    // onStep (falls angegeben) wird bei jedem der beiden Kopiervorgaenge
    // (erst Einstellungen, dann WLAN-Zugangsdaten) direkt VOR dem
    // jeweiligen Kopieren aufgerufen - der aufrufende Screen
    // (menu_screen.cpp) nutzt das, um waehrend des SD-bedingt spuerbar
    // langsamen Sicherns/Wiederherstellens Fortschrittspunkte auf dem
    // Button anzuzeigen, statt dass der Button eingefroren wirkt.
    bool backup(void (*onStep)() = nullptr);
    bool restore(void (*onStep)() = nullptr);
    bool hasBackup();

    // Loescht den kompletten Flightradar-Ordner von der SD-Karte und
    // startet das Geraet neu - beim naechsten Boot laeuft dadurch wieder
    // die komplette Ersteinrichtung (siehe main.cpp: isFirstRun-Erkennung
    // ueber Config::SD_SETTINGS_FILE). Kehrt nur im Fehlerfall zurueck
    // (z.B. SD nicht eingehaengt) - im Erfolgsfall startet das Geraet
    // neu, bevor die Funktion "zurueckkehrt" (ESP.restart()).
    bool factoryReset();
}