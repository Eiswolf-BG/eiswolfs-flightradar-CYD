#pragma once
#include <Arduino.h>

// Liest den eingebauten Lichtsensor (LDR an Config::LDR_PIN, siehe
// config.h) und leitet daraus eine geglaettete Display-Helligkeit ab -
// fuer Menue > System > Anzeige > Helligkeit > "Auto-Helligkeit"
// (SettingsStore::autoBrightnessEnabled()). Eigenes kleines Modul statt
// direkt in main.cpp, damit sowohl main.cpp (fortlaufende PWM-Anwendung)
// als auch brightness_screen.cpp (Live-Anzeige des aktuellen Werts beim
// Aufruf des Helligkeit-Screens) denselben, einzigen geglaetteten Wert
// lesen koennen.
namespace AutoBrightness {
    // Einmalig in main.cpp::setup() aufrufen, VOR dem ersten Zugriff auf
    // currentPercent() (z.B. fuer den allerersten ledcWrite()) - nimmt
    // sofort einen ersten Messwert, damit der Bildschirm beim Boot nicht
    // kurz auf 0% (komplett dunkel) faellt, falls Auto-Helligkeit bereits
    // aus einer frueheren Sitzung aktiviert gespeichert ist.
    void begin();

    // Regelmaessig aufrufen (main.cpp::loop(), 1-Sekunden-Takt) - liest bei
    // aktivem SettingsStore::autoBrightnessEnabled() einen frischen
    // Sensorwert, glaettet ihn per exponentiellem gleitendem Mittelwert
    // (Config::AUTO_BRIGHTNESS_SMOOTHING) und aktualisiert currentPercent().
    // Macht NICHTS, wenn das Feature gerade ausgeschaltet ist (spart die
    // ADC-Messung).
    void update();

    // Zuletzt berechneter, geglaetteter Helligkeitswert in Prozent
    // (Config::BRIGHTNESS_MIN_PERCENT..MAX_PERCENT) - nur aussagekraeftig,
    // wenn SettingsStore::autoBrightnessEnabled() an ist bzw. begin()/
    // update() mindestens einmal gelaufen sind.
    uint8_t currentPercent();
}
