#pragma once
#include <TFT_eSPI.h>

// Wiederverwendbares kleines Radar-Logo (3 Reticle-Kreise + Fadenkreuz, 2
// Mini-Flugzeuge, 2 Mini-Helikopter, zentrales Flugzeug-Symbol) - urspruenglich
// nur fest im Splash-Screen verdrahtet (siehe splash_screen.cpp), jetzt
// hierher ausgelagert und um cx/cy/scale parametrisiert, damit dieselbe
// Grafik auch an anderer Stelle wiederverwendet werden kann (z.B. als Logo
// auf dem Ruhebildschirm, siehe main.cpp).
//
// cx/cy = Mittelpunkt des Reticles. scale=1.0 entspricht exakt der
// urspruenglichen Splash-Screen-Groesse (Reticle-Aussenradius 80px) - bei
// scale=1.0 und denselben cx/cy wie vorher (Bildschirmmitte, cy=174) ist das
// Ergebnis pixelidentisch zum bisherigen Splash-Screen-Bild.
namespace RadarLogo {
    void draw(TFT_eSPI& tft, int16_t cx, int16_t cy, float scale = 1.0f);
}
