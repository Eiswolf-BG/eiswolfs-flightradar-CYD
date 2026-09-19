#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace LocationPresetsScreen {
    void run(TFT_eSPI& tft);

    // Adresse-oder-Koordinaten-Auswahl-Flow, urspruenglich nur intern fuer
    // den "+"-Button hier verwendet - jetzt auch von
    // first_run_location_screen.cpp wiederverwendet (Alex' ausdruecklicher
    // Wunsch: derselbe Code-Pfad statt einer separaten Kopie, haelt beide
    // Stellen automatisch konsistent). Zeigt Adresse/Koordinaten/Abbrechen
    // zur Auswahl, fuehrt den gewaehlten Weg komplett aus (inkl. Namens-
    // vergabe + Speichern als Preset + Aktivierung des neuen Presets bei
    // Erfolg) und liefert true zurueck, wenn dabei tatsaechlich ein neuer
    // Standort-Preset angelegt wurde.
    bool addPresetFlow(TFT_eSPI& tft);
}