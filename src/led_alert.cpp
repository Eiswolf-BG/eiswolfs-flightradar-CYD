#include "led_alert.h"
#include "settings_store.h"
#include <atomic>

namespace LedAlert {

namespace {
    constexpr uint8_t PIN_RED   = 4;
    constexpr uint8_t PIN_GREEN = 16;
    constexpr uint8_t PIN_BLUE  = 17;

    // PWM statt reinem digitalWrite (siehe writeChannel()/AMBER_*-Kommentar
    // unten) - eigene ledc-Kanaele, getrennt vom Backlight-Kanal (Kanal 0,
    // siehe BACKLIGHT_PWM_CHANNEL in main.cpp). 5kHz/8-Bit wie beim
    // Backlight, fuer eine LED voellig ausreichend (kein sichtbares
    // Flackern, feine Helligkeitsstufen 0-255).
    constexpr uint8_t RED_PWM_CHANNEL   = 1;
    constexpr uint8_t GREEN_PWM_CHANNEL = 2;
    constexpr uint8_t BLUE_PWM_CHANNEL  = 3;
    constexpr uint32_t PWM_FREQ_HZ = 5000;
    constexpr uint8_t PWM_RESOLUTION_BITS = 8;

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

    // Update-Verfuegbar-Signal: dreimal kurz MAGENTA blinken, alle 10
    // Sekunden wiederholt, solange OtaUpdate::isUpdateAvailable() true
    // liefert. Anders als beim Heartbeat wird das "verfuegbar?"-Flag NICHT
    // hier im LED-Modul selbst gecacht - der Aufrufer (radar_screen.cpp::
    // updateProximityAlert(), Core 1) uebergibt es direkt als Parameter an
    // update(), gelesen ueber OtaUpdate::isUpdateAvailable() (das intern
    // bereits ein std::atomic<bool> ist, da es von Core 0 - Hintergrund-
    // Update-Pruefung in net_task.cpp - gesetzt und hier auf Core 1 gelesen
    // wird). Dadurch braucht dieses Modul selbst keinen weiteren Cross-
    // Core-Zustand: nowMs/updateAvailable kommen beide bereits fertig
    // synchronisiert von Core 1 aus rein, genau wie mode auch.
    constexpr uint32_t UPDATE_BLINK_ON_MS  = 120;
    constexpr uint32_t UPDATE_BLINK_OFF_MS = 120;
    constexpr uint32_t UPDATE_BLINK_SLOT_MS = UPDATE_BLINK_ON_MS + UPDATE_BLINK_OFF_MS;
    constexpr uint8_t  UPDATE_BLINK_COUNT  = 3;
    constexpr uint32_t UPDATE_CYCLE_MS     = 10000;

    // Rein rechnerisch aus nowMs abgeleitet (kein eigener Zustand, kein
    // Race moeglich) - dreht sich im UPDATE_CYCLE_MS-Takt, blinkt in den
    // ersten UPDATE_BLINK_COUNT Slots des Zyklus, danach Pause bis zum
    // naechsten Zyklus.
    bool updateBlinkOn(uint32_t nowMs) {
        uint32_t phase = nowMs % UPDATE_CYCLE_MS;
        uint32_t burstEnd = UPDATE_BLINK_COUNT * UPDATE_BLINK_SLOT_MS;
        if (phase >= burstEnd) return false;
        return (phase % UPDATE_BLINK_SLOT_MS) < UPDATE_BLINK_ON_MS;
    }

    // Einzelner Kanal, 0 (aus) bis 255 (volle Helligkeit) - die LED ist
    // active-low (LOW = an), ledcWrite() erwartet aber den HIGH-Zeitanteil
    // (Duty), deshalb hier invertiert (255-brightness).
    void writeChannel(uint8_t pwmChannel, uint8_t brightness) {
        ledcWrite(pwmChannel, 255 - brightness);
    }

    void setAllOff() {
        writeChannel(RED_PWM_CHANNEL, 0);
        writeChannel(GREEN_PWM_CHANNEL, 0);
        writeChannel(BLUE_PWM_CHANNEL, 0);
    }

    // AMBER-Mischung: auf dem verbauten RGB-LED-Modul dominiert bei
    // GLEICH HELLEM Ansteuern von Rot+Gruen sichtbar das Gruen, Rot bleibt
    // praktisch unsichtbar (Live-Test an ZWEI Geraeten 31.08., siehe
    // Chat-Verlauf) - reines Rot UND reines Gruen einzeln funktionieren
    // beide einwandfrei, die gruene Diode dieses billigen CYD-LED-Moduls
    // ist gegenueber der roten aber deutlich heller/dominanter. Zwei
    // Zwischenschritte bereits verworfen: (1) gleichzeitiges digitalWrite
    // beider Kanaele (nur Gruen sichtbar), (2) zeitliches Alternieren
    // (Time-Multiplexing, zuletzt mit 3ms/165Hz) - selbst bei theoretisch
    // 50/50 Zeitaufteilung wurde es als klar ungleich (~80/20 fuer Gruen)
    // UND als getrenntes Blinken statt echter Mischfarbe wahrgenommen,
    // nicht nur als Timing-Artefakt, sondern weil die gruene Diode pro
    // Zeiteinheit schlicht heller ist. Jetzt echtes PWM (siehe
    // writeChannel() oben): Rot auf volle Helligkeit, Gruen gedimmt auf
    // AMBER_GREEN_BRIGHTNESS, um die staerkere gruene Diode auszugleichen
    // - beide Kanaele GLEICHZEITIG und DAUERHAFT an (kein Blinken
    // zwischen den Farben mehr, nur noch das normale Alarm-Blinken der
    // Mischfarbe als Ganzes). Der Wert ist eine erste Schaetzung (kein
    // Messgeraet verfuegbar) und muss ggf. am echten Geraet nachjustiert
    // werden, bis der Amber-Eindruck stimmt.
    constexpr uint8_t AMBER_GREEN_BRIGHTNESS = 25;

    void writeAlarmChannels(bool r, bool g, bool b) {
        if (r && g && !b) {
            writeChannel(RED_PWM_CHANNEL, 255);
            writeChannel(GREEN_PWM_CHANNEL, AMBER_GREEN_BRIGHTNESS);
            writeChannel(BLUE_PWM_CHANNEL, 0);
            return;
        }
        writeChannel(RED_PWM_CHANNEL,   r ? 255 : 0);
        writeChannel(GREEN_PWM_CHANNEL, g ? 255 : 0);
        writeChannel(BLUE_PWM_CHANNEL,  b ? 255 : 0);
    }

    // Heartbeat (mode==Off) und Naeherungsalarm (Mode::ProximityGreen)
    // folgen jetzt dem gewaehlten Systemthema (SettingsStore::
    // radarThemeIndex(), dieselbe zentrale Quelle wie UiTheme::
    // accentColor() fuer die restliche UI) statt fest Gruen zu sein. Amber
    // wird als bestmoegliche Zwei-Kanal-Annaeherung ueber Rot+Gruen
    // gemeinsam dargestellt (siehe writeAlarmChannels() oben fuer die
    // Helligkeits-Kompensation), Blau ueber den Blau-Kanal allein. Wichtig
    // fuer Watchlist-Alarm (siehe update() unten): DORT wird bewusst NICHT
    // diese Funktion verwendet, sondern fest Cyan (Gruen+Blau) - sonst
    // waere der Watchlist-Alarm bei aktivem Blau-Thema (nur Blau-Kanal)
    // farblich nicht vom Naeherungsalarm zu unterscheiden.
    void themeLedChannels(bool& r, bool& g, bool& b) {
        r = false; g = false; b = false;
        switch (SettingsStore::radarThemeIndex()) {
            case 1: r = true; g = true; break;  // Amber (Naeherung: Rot+Gruen)
            case 2: b = true; break;             // Blau
            default: g = true; break;            // Gruen (Standard)
        }
    }
}

void begin() {
    ledcSetup(RED_PWM_CHANNEL, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
    ledcSetup(GREEN_PWM_CHANNEL, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
    ledcSetup(BLUE_PWM_CHANNEL, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
    ledcAttachPin(PIN_RED, RED_PWM_CHANNEL);
    ledcAttachPin(PIN_GREEN, GREEN_PWM_CHANNEL);
    ledcAttachPin(PIN_BLUE, BLUE_PWM_CHANNEL);
    setAllOff();
    initialized = true;
}

void pulseHeartbeat(uint32_t nowMs) {
    // Einziger Schreibzugriff von Core 0 aus - atomar, kein zweiter
    // Begleitwert mehr, der aus dem Takt geraten koennte (siehe Kommentar
    // bei der Deklaration oben).
    heartbeatStartMs.store(nowMs, std::memory_order_relaxed);
}

bool update(Mode mode, uint32_t nowMs, bool updateAvailable) {
    if (!initialized) begin();

    // "Gerade aktiv?" wird rein rechnerisch aus dem atomar gelesenen
    // Zeitstempel abgeleitet - KEIN Rueckschreiben mehr (siehe Kommentar bei
    // der Deklaration oben). Funktioniert unveraendert in jedem mode, genau
    // wie zuvor mit dem bool+Timestamp-Paar beabsichtigt, aber jetzt ohne
    // die Race Condition.
    uint32_t hbStart = heartbeatStartMs.load(std::memory_order_relaxed);
    bool heartbeatInWindow = (nowMs - hbStart) < HEARTBEAT_PULSE_MS;

    // Wie beim Heartbeat: Notfall-Alarm hat absolute Prioritaet, das
    // Update-Blinken wird bei EmergencyRed komplett unterdrueckt (kein
    // updateBlinkOn()-Aufruf noetig, einfach per && kurzgeschlossen).
    bool updateBlinkActive = updateAvailable && mode != Mode::EmergencyRed && updateBlinkOn(nowMs);

    if (mode == Mode::Off) {
        if (heartbeatInWindow) {
            bool r, g, b;
            themeLedChannels(r, g, b);
            writeAlarmChannels(r, g, b);
            blinkState = false;
            lastMode = mode;
            return true;
        }

        // MAGENTA (Rot+Blau) fuer das Update-Signal - Heartbeat hat bei
        // gleichzeitigem Zusammentreffen Vorrang (kuerzeres, selteneres
        // Signal, siehe Kommentar bei UPDATE_BLINK_* oben), daher erst HIER
        // nach der Heartbeat-Pruefung ausgewertet.
        if (updateBlinkActive) {
            writeChannel(RED_PWM_CHANNEL, 255);
            writeChannel(BLUE_PWM_CHANNEL, 255);
            writeChannel(GREEN_PWM_CHANNEL, 0);
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

    // Notfall bleibt fest Rot, Watchlist bleibt fest CYAN (Gruen+Blau) -
    // beide UNABHAENGIG vom Systemthema, wie von Alex ausdruecklich
    // gewuenscht (Watchlist muss sich auch bei aktivem Blau-Thema klar vom
    // dann ebenfalls blauen Naeherungsalarm unterscheiden). Nur
    // ProximityGreen folgt dem Thema (siehe themeLedChannels() oben) - der
    // Enum-Wert heisst weiterhin "WatchlistBlue" (nur interner Name, siehe
    // led_alert.h), die tatsaechliche LED-Farbe ist jetzt aber Cyan.
    bool chR = false, chG = false, chB = false;
    if (mode == Mode::EmergencyRed) {
        chR = true;
    } else if (mode == Mode::WatchlistBlue) {
        chG = true;
        chB = true;
    } else {
        themeLedChannels(chR, chG, chB);
    }

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
        writeChannel(RED_PWM_CHANNEL, 255);
        writeChannel(GREEN_PWM_CHANNEL, 255);
        writeChannel(BLUE_PWM_CHANNEL, 255);
        return blinkState;
    }

    // Update-Ueberlagerung: gleiches Prinzip wie der Heartbeat-Weiss-Blitz
    // (siehe oben) - zusaetzlich zu ProximityGreen/WatchlistBlue, NIEMALS
    // bei EmergencyRed (bereits in updateBlinkActive per && ausgeschlossen).
    // MAGENTA (Rot+Blau an, Gruen aus) statt Weiss, damit sich das Update-
    // Signal optisch klar vom Heartbeat unterscheidet - beide sollen bei
    // einem Blick auf die LED nicht verwechselbar sein.
    if (updateBlinkActive) {
        writeChannel(RED_PWM_CHANNEL, 255);
        writeChannel(GREEN_PWM_CHANNEL, 0);
        writeChannel(BLUE_PWM_CHANNEL, 255);
        return blinkState;
    }

    writeAlarmChannels(chR && blinkState, chG && blinkState, chB && blinkState);

    return blinkState;
}

void flashWhite(uint32_t durationMs) {
    if (!initialized) begin();

    writeChannel(RED_PWM_CHANNEL, 255);
    writeChannel(GREEN_PWM_CHANNEL, 255);
    writeChannel(BLUE_PWM_CHANNEL, 255);
    delay(durationMs);
    setAllOff();
}

}
