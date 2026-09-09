#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Menue-Timeout einstellen (Menue > System > Anzeige > Menue-Timeout) -
// Schieberegler von Config::MENU_IDLE_TIMEOUT_MIN_SECONDS bis
// Config::MENU_IDLE_TIMEOUT_MAX_SECONDS (in MENU_IDLE_TIMEOUT_STEP_SECONDS-
// Schritten), danach "Nie" als eigene Endposition - steuert den
// Inaktivitaets-Timeout INNERHALB von Vollbild-Menues/Einstellungs-Screens
// (siehe SettingsStore::menuIdleTimeoutMs(), vorher fest auf 2 Minuten
// einprogrammiert). Gleicher Aufbau wie TimeoutScreen (timeout_screen.cpp),
// nur ohne den Ruhebildschirm-Umschalter, der inhaltlich zum Bildschirm-
// Timeout gehoert, nicht zu diesem Screen.
namespace MenuTimeoutScreen {
    void run(TFT_eSPI& tft);
}
