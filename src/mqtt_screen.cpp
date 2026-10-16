#include "mqtt_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "menu_screen.h"
#include "config.h"
#include "i18n.h"
#include "ui_theme.h"

// Einstell-Screen fuer die optionale MQTT-Schnittstelle (Menue > System >
// "MQTT", SettingsStore::mqttEnabled(), AUS per Default) - fuer Nutzer, die
// ein paar Radar-Kennzahlen in ein eigenes Smart-Home-System (z.B. Home
// Assistant) einspeisen wollen, siehe mqtt_client.h fuer die eigentliche
// Verbindungslogik/Topics.
namespace MqttScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool active = false) {
        uint16_t bg = active ? UiTheme::accentColor(tft) : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : UiTheme::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Linksbuendige Variante von drawButton() fuer die Broker-/Nutzername-/
    // Passwort-Zeilen - ein zentriertes "Broker: broker.hivemq.com:1883"
    // waere bei langen Werten schwerer zu lesen als "Label: Wert" von links
    // beginnend.
    void drawLeftButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.drawString(label, (int16_t)(r.x + 8), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    // Echtes ankreuzbares Kaestchen, gleiches Muster wie radar_theme_screen.cpp
    // (hier lokal dupliziert, siehe CLAUDE.md-Konvention "jeder Screen bleibt
    // unabhaengig lauffaehig").
    void drawCheckboxRow(TFT_eSPI& tft, const Rect& r, const String& label, bool checked) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));

        constexpr int16_t BOX_SIZE = 22;
        int16_t boxX = r.x + 10;
        int16_t boxY = (int16_t)(r.y + (r.h - BOX_SIZE) / 2);
        if (checked) {
            tft.fillRoundRect(boxX, boxY, BOX_SIZE, BOX_SIZE, 3, UiTheme::accentColor(tft));
        } else {
            tft.drawRoundRect(boxX, boxY, BOX_SIZE, BOX_SIZE, 3, UiTheme::accentColor(tft));
        }

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.drawString(label, (int16_t)(boxX + BOX_SIZE + 10), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    constexpr int16_t ROW_INFO_BTN_SIZE = 20;
    constexpr int16_t ROW_INFO_BTN_PAD = 6;

    Rect rowInfoBtnRect(const Rect& row) {
        return {(int16_t)(row.x + row.w - ROW_INFO_BTN_SIZE - ROW_INFO_BTN_PAD),
                (int16_t)(row.y + (row.h - ROW_INFO_BTN_SIZE) / 2),
                ROW_INFO_BTN_SIZE, ROW_INFO_BTN_SIZE};
    }

    void drawRowInfoButton(TFT_eSPI& tft, const Rect& row) {
        Rect btn = rowInfoBtnRect(row);
        drawButton(tft, btn, "?");
    }

    // BUGFIX (Alex' Auftrag, nach Live-Test am lokalen Broker): das
    // bisherige kombinierte "Broker (Host:Port)"-Feld lief bei laengeren
    // Werten (z.B. "192.168.178.143:1883", 20 Zeichen bei Groesse 2) aus
    // der Eingabebox heraus - weitere Tasteneingaben wurden zwar korrekt
    // im internen Puffer uebernommen (siehe runTextKeypad()), aber
    // visuell nicht mehr angezeigt, was wie ein blockiertes Eingabefeld
    // wirkte ("Port nicht eingebbar"). Jetzt getrennte Host-/Port-Felder
    // (je eigene Box, deutlich mehr Luft) PLUS ein genereller Fix in
    // runTextKeypad() (siehe visibleTail()), der auch bei langen Domain-
    // Namen im Host-Feld nie wieder blind ueberlaufen kann.
    constexpr uint8_t ROW_COUNT = 6; // Enable, Host, Port, Username, Passwort, Zurueck
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t START_Y = 40;
    constexpr int16_t END_Y = Config::SCREEN_HEIGHT - 10;
    constexpr int16_t ROW_H = (END_Y - START_Y - (ROW_COUNT - 1) * ROW_GAP) / ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
    }

    // Zeigt nur das ENDE von "buf", das in maxWidth passt (gleiches Muster
    // wie address_search_screen.cpp/location_presets_screen.cpp, hier
    // dupliziert, Projekt-Konvention) - das Tippende interessiert meist
    // am meisten, was zuletzt eingegeben wurde. UTF-8-sicher (ueberspringt
    // 2-Byte-Folgen am Wortanfang nie mitten im Zeichen).
    String visibleTail(TFT_eSPI& tft, const char* buf, int16_t maxWidth) {
        String s(buf);
        while (s.length() > 0 && tft.textWidth(s) > maxWidth) {
            int32_t cut = ((((uint8_t)s[0]) & 0xE0) == 0xC0) ? 2 : 1;
            if (cut > (int32_t)s.length()) cut = s.length();
            s = s.substring(cut);
        }
        return s;
    }

    // Zerlegt den intern weiterhin als EIN "host:port"-String gespeicherten
    // Broker-Wert (SettingsStore::mqttBroker(), siehe mqtt_client.h) fuer
    // die getrennte Host-/Port-Anzeige - 1:1 dieselbe Logik wie
    // mqtt_client.cpp::splitHostPort() (dort in anonymem Namespace, nicht
    // exportiert, deshalb hier dupliziert, Projekt-Konvention). Am LETZTEN
    // Doppelpunkt getrennt, nicht am ersten, damit eine IPv6-Adresse nicht
    // faelschlich mitten zerschnitten wird.
    void splitHostPort(const String& hostPort, String& outHost, String& outPort) {
        int colon = hostPort.lastIndexOf(':');
        if (colon < 0) {
            outHost = hostPort;
            outPort = "";
            return;
        }
        outHost = hostPort.substring(0, colon);
        outPort = hostPort.substring(colon + 1);
    }

    // Einfaches Text-Tastenfeld: QWERTY-Buchstaben + Ziffern (gleiches
    // Grundmuster wie andernorts im Projekt, z.B. die Rufzeichen-/Namens-
    // Tastenfelder), zusaetzlich eine schmale Symbolzeile mit den fuer
    // Broker-Adressen/Zugangsdaten typischen Sonderzeichen (. : - _ @ /) -
    // gleiches Prinzip wie die zusaetzlichen Satzzeichen-Tasten bei der
    // Adresssuche (location_presets_screen.cpp/address_search_screen.cpp).
    //
    // BUGFIX (GitHub Issue #12, Folgemeldung dottgrasso: MQTT-Verbindung
    // laeuft jetzt, aber Benutzername/Passwort wurden IMMER in
    // Grossbuchstaben gespeichert) - dieses Tastenfeld hatte urspruenglich
    // bewusst KEINE Umschalttaste/Kleinbuchstaben (anders als das volle
    // WLAN-Passwort-Tastenfeld in wifi_setup_screen.cpp, siehe dortiges
    // KeyType::Shift), weil es fuer Felder wie Rufzeichen/Flughafencodes
    // gedacht war, die ueblicherweise grossgeschrieben werden - wurde aber
    // unveraendert auch fuer MQTT-Zugangsdaten (UND das ntfy.sh-Topic,
    // siehe ntfy_push_screen.cpp) weiterverwendet, wo die exakte Schreib-
    // weise zaehlt (Home Assistant/Mosquitto sind case-sensitive). Jetzt
    // mit einfachem Umschalt-Knopf (kein One-Shot/Caps-Lock-Doppeltipp wie
    // beim grossen WLAN-Tastenfeld - bewusst einfacher gehalten, passend
    // zum Rest dieser Funktion) - Standard ist klein geschrieben, da
    // Zugangsdaten ueberwiegend klein/gemischt sind, nicht komplett gross.
    // allowEmpty=true erlaubt Bestaetigen ohne Eingabe (Nutzername/
    // Passwort duerfen leer bleiben), bei false (Host-/Port-Feld) muss
    // mindestens ein Zeichen stehen.
    String runTextKeypad(TFT_eSPI& tft, const String& title, bool allowEmpty) {
        MenuStars::reset();
        constexpr const char* DIGITS = "1234567890";
        constexpr const char* ROW1_UPPER = "QWERTYUIOP";
        constexpr const char* ROW1_LOWER = "qwertyuiop";
        constexpr const char* ROW2_UPPER = "ASDFGHJKL";
        constexpr const char* ROW2_LOWER = "asdfghjkl";
        constexpr const char* ROW3_UPPER = "ZXCVBNM";
        constexpr const char* ROW3_LOWER = "zxcvbnm";
        constexpr const char* SYMBOLS = ".:-_@/";

        char buf[48] = {0};
        uint8_t len = 0;
        bool shiftOn = false;

        constexpr int16_t KEY_H = 26;
        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t FIELD_H = 30;

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(title);
        int16_t fieldY = (int16_t)(tft.getCursorY() + 4);
        int16_t ROW0_Y = (int16_t)(fieldY + FIELD_H + 6);

        auto layoutRow = [&](int16_t y, Rect* outRects, uint8_t n) {
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect digitRects[10], row1Rects[10], row2Rects[9], row3Rects[7], symbolRects[6];
        layoutRow(ROW0_Y, digitRects, 10);
        layoutRow((int16_t)(ROW0_Y + (KEY_H + KEY_GAP)), row1Rects, 10);
        layoutRow((int16_t)(ROW0_Y + 2 * (KEY_H + KEY_GAP)), row2Rects, 9);
        layoutRow((int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), row3Rects, 7);
        layoutRow((int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), symbolRects, 6);

        // Umschalt-Knopf links statt der vollen Backspace-Zeile (spart eine
        // eigene Zeile/mehr Bildschirmhoehe) - Breite 70px reicht fuer
        // "Shift"/"ABC" bequem aus.
        Rect shiftBtn     = {4, (int16_t)(ROW0_Y + 5 * (KEY_H + KEY_GAP)), 70, KEY_H};
        Rect backspaceBtn = {78, (int16_t)(ROW0_Y + 5 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8 - 74), KEY_H};
        Rect cancelBtn    = {4, (int16_t)(ROW0_Y + 6 * (KEY_H + KEY_GAP)), 110, KEY_H};
        Rect confirmBtn   = {118, (int16_t)(ROW0_Y + 6 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8 - 114), KEY_H};

        bool done = false;
        bool confirmed = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(title);

            tft.fillRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, TFT_BLACK);
            tft.drawRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, UiTheme::accentColor(tft));
            tft.setTextSize(2);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(14, (int16_t)(fieldY + 22));
            // Passwort bewusst unmaskiert angezeigt (Alex' Wunsch) - auf
            // diesem Geraet gibt es keine andere Person, die mitlesen
            // koennte, und die Sichtbarkeit hilft beim Tippen von Gross-/
            // Kleinschreibung und Sonderzeichen ohne Vertipper.
            tft.print(visibleTail(tft, buf, (int16_t)(Config::SCREEN_WIDTH - 28)));
            tft.setTextSize(1);

            const char* row1 = shiftOn ? ROW1_UPPER : ROW1_LOWER;
            const char* row2 = shiftOn ? ROW2_UPPER : ROW2_LOWER;
            const char* row3 = shiftOn ? ROW3_UPPER : ROW3_LOWER;

            for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(DIGITS[i]));
            for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(row1[i]));
            for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(row2[i]));
            for (uint8_t i = 0; i < 7; i++) drawButton(tft, row3Rects[i], String(row3[i]));
            for (uint8_t i = 0; i < 6; i++) drawButton(tft, symbolRects[i], String(SYMBOLS[i]));

            // "active" (gefuellt statt nur Umriss) zeigt den aktuellen
            // Umschalt-Zustand an - gleiches Prinzip wie drawCheckboxRow()
            // oben.
            drawButton(tft, shiftBtn, shiftOn ? "ABC" : "abc", shiftOn);
            drawButton(tft, backspaceBtn, "<- Backspace");
            drawButton(tft, cancelBtn, I18n::t(StringId::CANCEL));
            drawButton(tft, confirmBtn, I18n::t(StringId::OK));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (digitRects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = DIGITS[i]; buf[len] = 0; handled = true; }
            }
            // Handy-Tastatur-Verhalten (Alex' Wunsch, wie schon in
            // ntfy_push_screen.cpp::runTopicKeypad()): nach einem per Shift
            // gross getippten Buchstaben automatisch wieder auf
            // Kleinschreibung zurueckschalten (kein dauerhaftes Caps-Lock).
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (row1Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = (shiftOn ? ROW1_UPPER : ROW1_LOWER)[i]; buf[len] = 0; handled = true; if (shiftOn) shiftOn = false; }
            }
            for (uint8_t i = 0; i < 9 && !handled; i++) {
                if (row2Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = (shiftOn ? ROW2_UPPER : ROW2_LOWER)[i]; buf[len] = 0; handled = true; if (shiftOn) shiftOn = false; }
            }
            for (uint8_t i = 0; i < 7 && !handled; i++) {
                if (row3Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = (shiftOn ? ROW3_UPPER : ROW3_LOWER)[i]; buf[len] = 0; handled = true; if (shiftOn) shiftOn = false; }
            }
            for (uint8_t i = 0; i < 6 && !handled; i++) {
                if (symbolRects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = SYMBOLS[i]; buf[len] = 0; handled = true; }
            }
            if (!handled && shiftBtn.contains(tap.x, tap.y)) {
                shiftOn = !shiftOn;
                handled = true;
            }
            if (!handled && backspaceBtn.contains(tap.x, tap.y)) {
                if (len > 0) { len--; buf[len] = 0; }
                handled = true;
            }
            if (!handled && cancelBtn.contains(tap.x, tap.y)) {
                done = true;
                confirmed = false;
                handled = true;
            }
            if (!handled && confirmBtn.contains(tap.x, tap.y) && (allowEmpty || len > 0)) {
                done = true;
                confirmed = true;
                handled = true;
            }

            if (handled) redraw();
        }

        return confirmed ? String(buf) : String();
    }
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::MQTT_TITLE));

        Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
        drawButton(tft, infoBtn, "?");

        Rect enableRow = rowRect(0);
        drawCheckboxRow(tft, enableRow, I18n::t(StringId::MQTT_ENABLE), SettingsStore::mqttEnabled());
        drawRowInfoButton(tft, enableRow);

        String broker = SettingsStore::mqttBroker();
        String currentHost, currentPort;
        splitHostPort(broker, currentHost, currentPort);

        String hostLabel = String(I18n::t(StringId::MQTT_HOST_LABEL)) +
                            (currentHost.length() ? currentHost : String(I18n::t(StringId::MQTT_NOT_SET)));
        Rect hostRow = rowRect(1);
        drawLeftButton(tft, hostRow, hostLabel);

        // Leeres Port-Feld zeigt den tatsaechlich wirksamen Standard-Port
        // an (Config::MQTT_DEFAULT_PORT, siehe mqtt_client.cpp), damit auf
        // einen Blick klar ist, dass dann trotzdem automatisch 1883
        // verwendet wird, statt es als "nicht gesetzt" misszuverstehen.
        String portLabel = String(I18n::t(StringId::MQTT_PORT_LABEL)) +
                            (currentPort.length() ? currentPort : String(Config::MQTT_DEFAULT_PORT));
        Rect portRow = rowRect(2);
        drawLeftButton(tft, portRow, portLabel);

        String user = SettingsStore::mqttUsername();
        String userLabel = String(I18n::t(StringId::MQTT_USERNAME_LABEL)) +
                            (user.length() ? user : String(I18n::t(StringId::MQTT_NOT_SET)));
        Rect userRow = rowRect(3);
        drawLeftButton(tft, userRow, userLabel);

        String pass = SettingsStore::mqttPassword();
        String passLabel = String(I18n::t(StringId::MQTT_PASSWORD_LABEL)) +
                            (pass.length() ? pass : String(I18n::t(StringId::MQTT_NOT_SET)));
        Rect passRow = rowRect(4);
        drawLeftButton(tft, passRow, passLabel);

        Rect backBtn = rowRect(5);
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }
        if (done) break;

        if (infoBtn.contains(tap.x, tap.y) || rowInfoBtnRect(enableRow).contains(tap.x, tap.y)) {
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::MQTT_INFO_TITLE),
                                        I18n::t(StringId::MQTT_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
        } else if (enableRow.contains(tap.x, tap.y)) {
            SettingsStore::setMqttEnabled(!SettingsStore::mqttEnabled());
        } else if (hostRow.contains(tap.x, tap.y)) {
            String value = runTextKeypad(tft, I18n::t(StringId::MQTT_HOST_PROMPT), false);
            if (value.length() > 0) {
                // Aktuellen Port (falls gesetzt) beibehalten - nur der Host
                // wird hier geaendert, siehe splitHostPort()-Kommentar oben.
                String combined = currentPort.length() ? (value + ":" + currentPort) : value;
                SettingsStore::setMqttBroker(combined);
            }
        } else if (portRow.contains(tap.x, tap.y)) {
            String value = runTextKeypad(tft, I18n::t(StringId::MQTT_PORT_PROMPT), true);
            // Host MUSS zu diesem Zeitpunkt bereits gesetzt sein (die
            // Port-Zeile zeigt den Standard-Port nur als Info an, erlaubt
            // aber kein Speichern ohne Host - sonst koennte hier ein
            // "host"-loser String wie ":1883" entstehen).
            if (currentHost.length() > 0) {
                String combined = value.length() ? (currentHost + ":" + value) : currentHost;
                SettingsStore::setMqttBroker(combined);
            }
        } else if (userRow.contains(tap.x, tap.y)) {
            String value = runTextKeypad(tft, I18n::t(StringId::MQTT_USERNAME_PROMPT), true);
            SettingsStore::setMqttUsername(value);
        } else if (passRow.contains(tap.x, tap.y)) {
            String value = runTextKeypad(tft, I18n::t(StringId::MQTT_PASSWORD_PROMPT), true);
            SettingsStore::setMqttPassword(value);
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
