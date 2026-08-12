#pragma once
#include <Arduino.h>

namespace SdStorage {

    bool init();
    bool isMounted();

    // Creates the Flightradar folder structure (root/log/screenshot dirs)
    // on the SD card - deliberately split out from init() (which now only
    // mounts the card), so that on the very first boot nothing gets
    // created on the card until the user has tapped Start on the Welcome
    // screen (see main.cpp: the button is "the gate to the app"). Safe to
    // call immediately on every later boot - ensureDir() is idempotent.
    void createStructure();
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