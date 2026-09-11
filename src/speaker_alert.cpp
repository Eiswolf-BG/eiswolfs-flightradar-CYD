#include "speaker_alert.h"
#include "config.h"

namespace SpeakerAlert {

namespace {
    // Eigener LEDC-Kanal, getrennt von Backlight (0, main.cpp) und der
    // RGB-Status-LED (1-3, led_alert.cpp).
    constexpr uint8_t SPK_PWM_CHANNEL = 4;

    // Zwei feste Toene im Wechsel (Alex' Wunsch: "grob im Stil des
    // Web-Alarmtons", der dort zwischen 400-1000Hz durchgaengig
    // auf-/abschwillt) - hier bewusst einfacher als zwei feste Frequenzen
    // statt einer weichen Rampe gehalten (per ledcWriteTone() ohnehin nur
    // Sprung-Frequenzen moeglich, keine kontinuierliche Modulation wie beim
    // Web Audio API-Oszillator).
    constexpr uint16_t TONE_LOW_HZ = 600;
    constexpr uint16_t TONE_HIGH_HZ = 1000;
    constexpr uint32_t TONE_SWITCH_MS = 300;

    // Kurzer Einzelton fuer einen neuen Watchlist-Treffer - eine feste
    // Frequenz statt des Wechseltons oben, damit er sich hoerbar vom
    // durchgehenden Notfall-Sirenenton unterscheidet. 220ms liegt in Alex'
    // gewuenschtem 200-250ms-Bereich.
    constexpr uint16_t WATCH_BEEP_HZ = 900;
    constexpr uint32_t WATCH_BEEP_MS = 220;

    bool toneActive = false;
    bool highTone = false;
    uint32_t lastSwitchMs = 0;
    // 0 = kein Einzelton geplant, sonst millis()-Zeitpunkt, bis zu dem der
    // aktuell laufende Einzelton noch klingen soll.
    uint32_t watchBeepEndMs = 0;
}

void begin() {
    ledcSetup(SPK_PWM_CHANNEL, TONE_LOW_HZ, 10);
    ledcAttachPin(Config::SPK_PIN, SPK_PWM_CHANNEL);
    ledcWriteTone(SPK_PWM_CHANNEL, 0); // still, bis update() den ersten Ton ausloest
}

void update(bool emergencyActive, bool newWatchHit) {
    uint32_t now = millis();

    if (emergencyActive) {
        // Sirene hat Vorrang - ein eventuell noch laufender Einzelton wird
        // verworfen, siehe Kommentar in speaker_alert.h.
        watchBeepEndMs = 0;
        if (!toneActive || now - lastSwitchMs >= TONE_SWITCH_MS) {
            highTone = !highTone;
            lastSwitchMs = now;
            ledcWriteTone(SPK_PWM_CHANNEL, highTone ? TONE_HIGH_HZ : TONE_LOW_HZ);
        }
        toneActive = true;
        return;
    }

    if (newWatchHit) watchBeepEndMs = now + WATCH_BEEP_MS;

    if (watchBeepEndMs != 0 && now < watchBeepEndMs) {
        if (!toneActive) {
            ledcWriteTone(SPK_PWM_CHANNEL, WATCH_BEEP_HZ);
            toneActive = true;
        }
        return;
    }

    watchBeepEndMs = 0;
    if (toneActive) {
        ledcWriteTone(SPK_PWM_CHANNEL, 0);
        toneActive = false;
    }
}

}
