#pragma once
#include <Arduino.h>

// Optionaler Notfall-Alarmton ueber den SPK-Steckverbinder des CYD-Boards
// (Config::SPK_PIN, siehe dortiger Pinout-Kommentar) - Alex' Wunsch: wer
// einen Lautsprecher anschliesst und das bekannte Sweep-Rauschen in Kauf
// nimmt, soll den Notfall-Alarm auch direkt am Geraet hoeren koennen. Ohne
// angeschlossenen Lautsprecher passiert einfach nichts (der Pin steuert
// still vor sich hin, ohne hoerbaren Effekt) - komplett gefahrlos, kein
// Schalter-Zustand haengt vom tatsaechlichen Vorhandensein eines
// Lautsprechers ab.
//
// Wiederverwendet denselben Schalter wie der Browser-Alarmton
// (SettingsStore::webAudioAlertEnabled(), Menue > Flugoptionen > LED-
// Alarme > "Web Alert Sound") - kein zweiter, separater Schalter. Reagiert
// NUR auf Notfall-Squawks (nicht auf einfache Watchlist-Treffer), laeuft
// unabhaengig vom bestehenden Morsecode-LED-Alarm (led_alert.cpp) - beide
// koennen gleichzeitig aktiv sein.
namespace SpeakerAlert {
    // Einmalig in setup() aufrufen - richtet den LEDC-Tonkanal auf
    // Config::SPK_PIN ein.
    void begin();

    // Jeden Tick aufrufen (radar_screen.cpp::updateProximityAlert(),
    // gleiche Stelle wie LedAlert::update()) - active = aktuell mindestens
    // ein sichtbares Flugzeug mit Notfall-Squawk UND
    // SettingsStore::webAudioAlertEnabled() an. Wechselt intern in einem
    // festen Takt zwischen zwei Toenen (grob im Stil des Web-Alarmtons),
    // solange active true ist - stumm, sobald active false wird.
    void update(bool active);
}
