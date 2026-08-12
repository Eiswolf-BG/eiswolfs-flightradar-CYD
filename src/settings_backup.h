#pragma once
#include <Arduino.h>

namespace SettingsBackup {
    bool backup();
    bool restore();
    bool hasBackup();

    // Loescht den kompletten Flightradar-Ordner von der SD-Karte und
    // startet das Geraet neu - beim naechsten Boot laeuft dadurch wieder
    // die komplette Ersteinrichtung (siehe main.cpp: isFirstRun-Erkennung
    // ueber Config::SD_SETTINGS_FILE). Kehrt nur im Fehlerfall zurueck
    // (z.B. SD nicht eingehaengt) - im Erfolgsfall startet das Geraet
    // neu, bevor die Funktion "zurueckkehrt" (ESP.restart()).
    bool factoryReset();
}