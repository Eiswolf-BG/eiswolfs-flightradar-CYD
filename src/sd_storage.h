#pragma once
#include <Arduino.h>

namespace SdStorage {

    bool init();
    bool isMounted();
    void seedDefaultDataFiles();
    void logEvent(const char* csvLine);

    // Loescht rekursiv einen kompletten Ordner (alle Dateien und
    // Unterordner) inklusive des Ordners selbst. Fuer den Menuepunkt
    // "Einstellungen zuruecksetzen" (settings_backup.cpp::factoryReset()) -
    // der einzige aktuelle Aufrufer, der damit den kompletten
    // Flightradar-Ordner von der SD-Karte entfernt. Gibt false zurueck,
    // wenn der Pfad nicht existiert/kein Ordner ist.
    bool deleteDirectoryRecursive(const char* path);

}