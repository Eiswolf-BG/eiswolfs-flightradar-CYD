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
        // "Intelligenter" Naeherungsalarm (SettingsStore::
        // proximityAlertSmartMode(), siehe radar_screen.cpp::
        // updateProximityAlert()) - ALTERNATIVE zu ProximityGreen, nie
        // gleichzeitig aktiv. Folgt wie ProximityGreen dem Systemthema
        // (themeLedChannels(), siehe led_alert.cpp) statt einer fest
        // zugewiesenen Farbe - alle Grundfarben sind bereits an andere
        // Zustaende vergeben (Rot=Notfall, Cyan=Watchlist, Weiss=Heartbeat/
        // Update), daher unterscheiden sich die drei Zonen NICHT per
        // Farbe, sondern per Blink-Geschwindigkeit (langsam/mittel/
        // schnell = Gelb/Orange/Rot), genau wie der bestehende einfache
        // Alarm sich per Blink-MUSTER (statt Farbe) vom Notfall-Alarm
        // abhebt, wenn beide zufaellig dieselbe Themenfarbe haben. Die
        // Namen (Yellow/Orange/Red) beschreiben die Zonen-BEDEUTUNG, NICHT
        // die tatsaechliche LED-Farbe.
        ProximitySmartYellow,
        ProximitySmartOrange,
        ProximitySmartRed,
    };

    void begin();

    // updateAvailable: wenn true, blinkt die LED zusaetzlich zum normalen
    // mode dreimal kurz MAGENTA, alle 10 Sekunden wiederholt (Hinweis auf
    // ein verfuegbares OTA-Update, siehe OtaUpdate::isUpdateAvailable()) -
    // wie der Heartbeat-Weiss-Blitz NIEMALS waehrend Mode::EmergencyRed.
    bool update(Mode mode, uint32_t nowMs, bool updateAvailable = false);

    void pulseHeartbeat(uint32_t nowMs);

    // Blockierendes, kurzes weisses Aufblitzen (alle 3 Farbkanaele an) als
    // sofortige Bestaetigung fuer eine einmalige Nutzeraktion (z.B. "Cam"-
    // Button getroffen). Bewusst blockierend (delay), da nur aus dem
    // Haupt-Loop bei einem Tap aufgerufen, nicht aus der NetTask-Schleife.
    void flashWhite(uint32_t durationMs = 150);
}