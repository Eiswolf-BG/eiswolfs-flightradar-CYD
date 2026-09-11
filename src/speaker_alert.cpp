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

    bool toneActive = false;
    bool highTone = false;
    uint32_t lastSwitchMs = 0;
}

void begin() {
    ledcSetup(SPK_PWM_CHANNEL, TONE_LOW_HZ, 10);
    ledcAttachPin(Config::SPK_PIN, SPK_PWM_CHANNEL);
    ledcWriteTone(SPK_PWM_CHANNEL, 0); // still, bis update(true) den ersten Ton ausloest
}

void update(bool active) {
    if (!active) {
        if (toneActive) {
            ledcWriteTone(SPK_PWM_CHANNEL, 0);
            toneActive = false;
        }
        return;
    }

    uint32_t now = millis();
    if (!toneActive || now - lastSwitchMs >= TONE_SWITCH_MS) {
        highTone = !highTone;
        lastSwitchMs = now;
        ledcWriteTone(SPK_PWM_CHANNEL, highTone ? TONE_HIGH_HZ : TONE_LOW_HZ);
    }
    toneActive = true;
}

}
