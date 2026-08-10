#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace FirstRunLocationScreen {
    // Einmaliger Hinweis-Screen beim allerersten Start (nach der
    // Sprachauswahl) - erklaert den Genauigkeitsvorteil eines per Adresse
    // gesetzten Standorts, mit direktem Einstieg in die Adresssuche.
    // Ueberspringbar.
    void run(TFT_eSPI& tft);
}
