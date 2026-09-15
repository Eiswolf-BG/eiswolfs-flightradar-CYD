#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Sitzungsstatistik-Screen (Menue > Flugoptionen > Statistik & Logbuch,
// neben "Flugbuch" einsortiert) - zeigt reine In-RAM-Werte seit dem letzten
// Neustart (siehe session_stats.h): Anzahl unterschiedlicher Flugzeuge,
// dichtester Vorbeiflug, hoechste Geschwindigkeit, hoechste Flughoehe,
// haeufigster Flugzeugtyp. NICHT persistiert, im Unterschied zum
// bestehenden SD-gestuetzten "Statistiken"-Screen (stats_screen.cpp).
namespace SessionStatsScreen {
    void run(TFT_eSPI& tft);
}
