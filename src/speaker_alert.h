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
// Alarme > "Web Alert Sound") - kein zweiter, separater Schalter. Laeuft
// unabhaengig vom bestehenden Morsecode-LED-Alarm (led_alert.cpp) - beide
// koennen gleichzeitig aktiv sein. Deckt seit Kurzem beide Faelle ab, die
// auch der Browser-Alarmton kennt: ein Notfall-Squawk loest einen
// durchgehenden Wechselton (Sirene) aus, ein NEUER Watchlist-Treffer
// (Rufzeichen- oder Squawk-Wachliste) einen kurzen Einzelton - analog zu
// playWatchedAlert()/der Sirene im Web-Pendant (web_export_server.cpp).
namespace SpeakerAlert {
    // Einmalig in setup() aufrufen - richtet den LEDC-Tonkanal auf
    // Config::SPK_PIN ein.
    void begin();

    // Jeden Tick aufrufen (radar_screen.cpp::updateProximityAlert(),
    // gleiche Stelle wie LedAlert::update()). emergencyActive = aktuell
    // mindestens ein sichtbares Flugzeug mit Notfall-Squawk UND
    // SettingsStore::webAudioAlertEnabled() an - loest die durchgehende
    // Sirene aus (fester Takt zwischen zwei Toenen, grob im Stil des
    // Web-Alarmtons) und hat Vorrang, falls newWatchHit im selben Zyklus
    // ebenfalls true ist (ein PWM-Kanal kann nur einen Ton gleichzeitig
    // ausgeben). newWatchHit = in DIESEM Zyklus ist mindestens ein
    // Flugzeug neu in den Watchlist-Zustand gewechselt (UND der Schalter
    // an) - loest einen kurzen Einzelton aus, deutlich unterscheidbar von
    // der Sirene. Stumm, sobald beide Parameter false sind.
    void update(bool emergencyActive, bool newWatchHit);
}
