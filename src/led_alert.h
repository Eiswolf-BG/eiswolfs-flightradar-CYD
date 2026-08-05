#pragma once
#include <Arduino.h>

// Steuert die diskrete RGB-LED auf der Rueckseite des CYD (kein Lautsprecher
// vorhanden, daher ersetzt die LED den akustischen Alarm vom Cardputer-
// Projekt). Pins: Rot=GPIO4, Gruen=GPIO16, Blau=GPIO17, active-low (LOW = an).
namespace LedAlert {

    enum class Mode {
        Off,
        ProximityGreen,
        WatchlistBlue,
        EmergencyRed,
    };

    void begin();
    bool update(Mode mode, uint32_t nowMs);

    void pulseHeartbeat(uint32_t nowMs);

    // Blockierendes, kurzes weisses Aufblitzen (alle 3 Farbkanaele an) als
    // sofortige Bestaetigung fuer eine einmalige Nutzeraktion (z.B. "Cam"-
    // Button getroffen). Bewusst blockierend (delay), da nur aus dem
    // Haupt-Loop bei einem Tap aufgerufen, nicht aus der NetTask-Schleife.
    void flashWhite(uint32_t durationMs = 150);
}