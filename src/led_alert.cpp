#include "led_alert.h"
#include <atomic>

namespace LedAlert {

namespace {
    constexpr uint8_t PIN_RED   = 4;
    constexpr uint8_t PIN_GREEN = 16;
    constexpr uint8_t PIN_BLUE  = 17;

    constexpr uint32_t GREEN_BLINK_INTERVAL_MS = 400;
    constexpr uint32_t RED_BLINK_INTERVAL_MS   = 150;

    // blinkState/lastToggleMs/lastMode/initialized werden AUSSCHLIESSLICH
    // von update()/begin() aus gelesen/geschrieben, und die werden beide nur
    // von Core 1 aufgerufen (radar_screen.cpp::updateProximityAlert(), aus
    // main.cpp::loop()) - kein Cross-Core-Zugriff, daher hier normale
    // (nicht-atomare) Variablen ausreichend.
    bool initialized = false;
    bool blinkState = false;
    uint32_t lastToggleMs = 0;
    Mode lastMode = Mode::Off;

    // TESTWEISE - Race-Condition-Fix (siehe Absprache mit Karl): frueher
    // ein bool+Timestamp-Paar (heartbeatActive/heartbeatStartMs), das
    // Core 0 (net_task.cpp::pulseHeartbeat()) UND Core 1 (update(), las
    // UND schrieb bool zurueck) beide unabgesichert anfassten - dabei
    // konnte Core 1s Rueckschreib-Zugriff einen gerade erst von Core 0
    // gesetzten Puls loeschen, bevor er dargestellt wurde (siehe Analyse).
    // Jetzt nur noch EIN einzelner atomarer Zeitstempel: Core 0 schreibt
    // ihn einmal atomar (store()), Core 1 liest ihn atomar (load()) und
    // leitet "gerade aktiv?" rein rechnerisch ab, OHNE je etwas
    // zurueckzuschreiben - es gibt also strukturell keinen Reset-
    // Schreibzugriff mehr, der mit Core 0s Schreibzugriff kollidieren
    // koennte. memory_order_relaxed genuegt hier, da nowMs/heartbeatStartMs
    // reine Zeitstempel sind, die nicht mit anderem Zustand synchronisiert
    // werden muessen (keine Daten "haengen" an diesem Wert).
    constexpr uint32_t HEARTBEAT_PULSE_MS = 200;
    // Startwert bewusst NICHT 0: "nowMs - heartbeatStartMs" muss direkt nach
    // dem Boot (bevor pulseHeartbeat() je aufgerufen wurde) garantiert >=
    // HEARTBEAT_PULSE_MS ergeben, sonst wuerde kurz nach dem Boot
    // faelschlich ein Heartbeat angezeigt (die fruehere bool-Absicherung
    // dafuer ist mit dem Wegfall von heartbeatActive entfallen). Durch den
    // Unterlauf/Wraparound von uint32_t liegt dieser Startwert "weit in der
    // Vergangenheit", jede echte nowMs kurz nach dem Boot liegt also sicher
    // ausserhalb des Pulsfensters.
    std::atomic<uint32_t> heartbeatStartMs{static_cast<uint32_t>(0 - (HEARTBEAT_PULSE_MS + 1))};

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
    // Einziger Schreibzugriff von Core 0 aus - atomar, kein zweiter
    // Begleitwert mehr, der aus dem Takt geraten koennte (siehe Kommentar
    // bei der Deklaration oben).
    heartbeatStartMs.store(nowMs, std::memory_order_relaxed);
}

bool update(Mode mode, uint32_t nowMs) {
    if (!initialized) begin();

    // "Gerade aktiv?" wird rein rechnerisch aus dem atomar gelesenen
    // Zeitstempel abgeleitet - KEIN Rueckschreiben mehr (siehe Kommentar bei
    // der Deklaration oben). Funktioniert unveraendert in jedem mode, genau
    // wie zuvor mit dem bool+Timestamp-Paar beabsichtigt, aber jetzt ohne
    // die Race Condition.
    uint32_t hbStart = heartbeatStartMs.load(std::memory_order_relaxed);
    bool heartbeatInWindow = (nowMs - hbStart) < HEARTBEAT_PULSE_MS;

    if (mode == Mode::Off) {
        if (heartbeatInWindow) {
            digitalWrite(PIN_GREEN, LOW);
            digitalWrite(PIN_RED, HIGH);
            digitalWrite(PIN_BLUE, HIGH);
            blinkState = false;
            lastMode = mode;
            return true;
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

    // Blink-Takt IMMER berechnen, auch waehrend eines Heartbeat-Ueberlagerung
    // (siehe unten) - so bleibt der zurueckgegebene blinkState (synchronisiert
    // die Marker-Blinkphase in radar_screen.cpp, siehe ledBlinkOn) exakt im
    // normalen Alarm-Takt, unabhaengig davon, ob der Heartbeat gerade
    // ueberlagert oder nicht.
    if (nowMs - lastToggleMs >= interval) {
        lastToggleMs = nowMs;
        blinkState = !blinkState;
    }

    // Heartbeat-Ueberlagerung: nur bei ProximityGreen/WatchlistBlue, NICHT
    // bei EmergencyRed (Notfall behaelt absolute Prioritaet, keine Aenderung
    // dort). Bewusst WEISS (alle 3 Kanaele) statt der jeweiligen Alarmfarbe -
    // ein gruener/blauer Heartbeat-Blitz waere farblich nicht vom
    // durchgehenden Alarm-Blinken zu unterscheiden gewesen. Kurze,
    // feststehende Dauer (HEARTBEAT_PULSE_MS = 200ms, deutlich kuerzer als
    // die 400ms-Blinkintervalle), damit der Alarm-Zustand selbst weiterhin
    // klar als solcher erkennbar bleibt und nicht vom weissen Blitz
    // dominiert wird.
    if (heartbeatInWindow && mode != Mode::EmergencyRed) {
        digitalWrite(PIN_RED, LOW);
        digitalWrite(PIN_GREEN, LOW);
        digitalWrite(PIN_BLUE, LOW);
        return blinkState;
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