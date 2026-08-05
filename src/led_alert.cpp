#include "led_alert.h"

namespace LedAlert {

namespace {
    constexpr uint8_t PIN_RED   = 4;
    constexpr uint8_t PIN_GREEN = 16;
    constexpr uint8_t PIN_BLUE  = 17;

    constexpr uint32_t GREEN_BLINK_INTERVAL_MS = 400;
    constexpr uint32_t RED_BLINK_INTERVAL_MS   = 150;

    bool initialized = false;
    bool blinkState = false;
    uint32_t lastToggleMs = 0;
    Mode lastMode = Mode::Off;

    bool heartbeatActive = false;
    uint32_t heartbeatStartMs = 0;
    constexpr uint32_t HEARTBEAT_PULSE_MS = 200;

    void setAllOff() {
        digitalWrite(PIN_RED, HIGH);
        digitalWrite(PIN_GREEN, HIGH);
        digitalWrite(PIN_BLUE, HIGH);
    }
}

void begin() {
    pinMode(PIN_RED, OUTPUT);
    pinMode(PIN_GREEN, OUTPUT);
    pinMode(PIN_BLUE, OUTPUT);
    setAllOff();
    initialized = true;
}

void pulseHeartbeat(uint32_t nowMs) {
    heartbeatActive = true;
    heartbeatStartMs = nowMs;
}

bool update(Mode mode, uint32_t nowMs) {
    if (!initialized) begin();

    if (mode == Mode::Off) {
        if (heartbeatActive) {
            if (nowMs - heartbeatStartMs < HEARTBEAT_PULSE_MS) {
                digitalWrite(PIN_GREEN, LOW);
                digitalWrite(PIN_RED, HIGH);
                digitalWrite(PIN_BLUE, HIGH);
                blinkState = false;
                lastMode = mode;
                return true;
            }
            heartbeatActive = false;
        }

        setAllOff();
        blinkState = false;
        lastMode = mode;
        return false;
    }

    if (mode != lastMode) {
        lastToggleMs = nowMs;
        blinkState = true;
        lastMode = mode;
    }

    uint32_t interval = (mode == Mode::EmergencyRed) ? RED_BLINK_INTERVAL_MS : GREEN_BLINK_INTERVAL_MS;
    uint8_t pin = (mode == Mode::EmergencyRed) ? PIN_RED
                : (mode == Mode::WatchlistBlue) ? PIN_BLUE
                : PIN_GREEN;

    if (nowMs - lastToggleMs >= interval) {
        lastToggleMs = nowMs;
        blinkState = !blinkState;
    }

    digitalWrite(PIN_RED,   (pin == PIN_RED   && blinkState) ? LOW : HIGH);
    digitalWrite(PIN_GREEN, (pin == PIN_GREEN && blinkState) ? LOW : HIGH);
    digitalWrite(PIN_BLUE,  (pin == PIN_BLUE  && blinkState) ? LOW : HIGH);

    return blinkState;
}

void flashWhite(uint32_t durationMs) {
    if (!initialized) begin();

    digitalWrite(PIN_RED, LOW);
    digitalWrite(PIN_GREEN, LOW);
    digitalWrite(PIN_BLUE, LOW);
    delay(durationMs);
    setAllOff();
}

}