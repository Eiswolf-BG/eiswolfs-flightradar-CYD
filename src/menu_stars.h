#pragma once
#include <TFT_eSPI.h>

// Gemeinsames Sterne-Twinkle-Modul fuer ALLE Menue-/Untermenue-Bildschirme
// (schwarzer Hintergrund) sowie den Splash-Screen - dieselbe Optik wie im
// Flugzeug-Detail-Panel des Radars.
namespace MenuStars {
    // Verteilt die Sterne neu ueber den ganzen Bildschirm - einmal beim
    // Betreten eines neuen Screens aufrufen (NICHT bei jedem Redraw
    // innerhalb desselben Screens, sonst "springt" die Animation staendig).
    void reset();

    // In der Warte-/Idle-Schleife eines Screens aufrufen (bei jedem
    // Schleifendurchlauf ist ok - die Funktion drosselt sich intern selbst
    // auf ca. alle 60ms, um das Display nicht unnoetig oft anzusprechen).
    void update(TFT_eSPI& tft);
}