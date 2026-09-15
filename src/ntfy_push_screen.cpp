#include "ntfy_push_screen.h"
#include "ntfy_push.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "menu_screen.h"
#include "config.h"
#include "i18n.h"
#include "ui_theme.h"

// Einstell-Screen fuer die optionale ntfy.sh-Push-Benachrichtigung (Menue >
// System > "ntfy.sh Push", SettingsStore::ntfyPushEnabled(), AUS per
// Default) - gleiches Grundgeruest wie mqtt_screen.cpp (Checkbox + Text-
// Eingabefeld-Zeile + Info-Button), hier lokal dupliziert statt geteilt
// (CLAUDE.md "jeder Screen unabhaengig lauffaehig").
namespace NtfyPushScreen {

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

    // Linksbuendige Variante - "Topic: mein-eindeutiges-topic" waere
    // zentriert bei langen Topic-Namen schwerer zu lesen (gleiches Muster
    // wie drawLeftButton() in mqtt_screen.cpp).
    void drawLeftButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.drawString(label, (int16_t)(r.x + 8), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

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

    // Wortweiser Zeilenumbruch anhand echter Pixelbreite (CLAUDE.md-Pflicht
    // fuer Text variabler Laenge) - fuer den Bildschirmtitel, der je nach
    // Sprache unterschiedlich lang ausfaellt. Lokal dupliziert, gleiches
    // Muster wie in mehreren anderen Screens (z.B. system_status_screen.cpp).
    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                          int16_t lineHeight, const String& text) {
        int16_t y = startY;
        int32_t start = 0;
        int32_t len = text.length();
        while (start < len) {
            while (start < len && text[start] == ' ') start++;
            if (start >= len) break;

            String line = text.substring(start, len);
            while (tft.textWidth(line) > maxWidth) {
                int32_t lastSpace = line.lastIndexOf(' ');
                if (lastSpace <= 0) break;
                line = line.substring(0, lastSpace);
            }

            tft.setCursor(x, y);
            tft.print(line);
            y += lineHeight;
            start += line.length();
        }
        return y;
    }

    // Kuerzt "label" zeichenweise von hinten, bis "prefix+label" in maxWidth
    // passt (kein "...", zu wenig Platz fuer drei zusaetzliche Punkte bei so
    // kurzen Topic-Namen) - schuetzt die Topic-Zeile davor, dass ein langer,
    // frei eingetippter ntfy.sh-Topic-Name ueber den Zeilenrand hinaus
    // gezeichnet wird (drawString() selbst wrappt/kuerzt nicht automatisch).
    // Gleiches Grundprinzip wie drawTruncatedLeft() in radar_screen.cpp.
    String truncateToFit(TFT_eSPI& tft, const String& prefix, const String& label, int16_t maxWidth) {
        String shown = label;
        while (shown.length() > 0 && tft.textWidth(prefix + shown) > maxWidth) {
            shown.remove(shown.length() - 1);
        }
        return prefix + shown;
    }

    constexpr uint8_t ROW_COUNT = 4; // Enable, Topic, Test-Push, Zurueck
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t START_Y = 40;
    constexpr int16_t END_Y = Config::SCREEN_HEIGHT - 10;
    constexpr int16_t ROW_H = (END_Y - START_Y - (ROW_COUNT - 1) * ROW_GAP) / ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
    }

    // Einfaches Text-Tastenfeld: Ziffern + QWERTY-Buchstaben + eine schmale
    // Symbolzeile mit den fuer ntfy.sh-Topic-Namen ueblichen Sonderzeichen
    // (- _ .) - gleiches Grundmuster wie runTextKeypad() in mqtt_screen.cpp,
    // hier lokal dupliziert statt geteilt (CLAUDE.md "jeder Screen
    // unabhaengig lauffaehig") - NUR dieses Tastenfeld hat das automatische
    // Gross-/Kleinschreibungs-Verhalten unten, nicht z.B. das ICAO/IATA-
    // Tastenfeld im Airline-Filter (dort bewusst durchgehend Grossbuchstaben,
    // eigene unabhaengige Funktion, unveraendert).
    //
    // Uebliches Handy-Tastatur-Verhalten (Alex' Wunsch): das erste getippte
    // Zeichen darf gross sein (uppercase startet true), danach schaltet die
    // Tastatur automatisch auf Kleinschreibung um - manuell jederzeit ueber
    // die Shift-Taste umschaltbar.
    String runTopicKeypad(TFT_eSPI& tft, const String& title) {
        MenuStars::reset();
        constexpr const char* DIGITS = "1234567890";
        constexpr const char* ROW1 = "QWERTYUIOP";
        constexpr const char* ROW2 = "ASDFGHJKL";
        constexpr const char* ROW3 = "ZXCVBNM";
        constexpr const char* SYMBOLS = "-_.";

        char buf[48] = {0};
        uint8_t len = 0;
        bool uppercase = true;

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

        // Symbolzeile um eine Shift-Taste ergaenzt (4 statt 3 Tasten) -
        // schiebt die 3 Sonderzeichen einfach eins nach rechts, reichlich
        // Platz frei in dieser sonst duennen Zeile.
        Rect digitRects[10], row1Rects[10], row2Rects[9], row3Rects[7], symbolRow[4];
        layoutRow(ROW0_Y, digitRects, 10);
        layoutRow((int16_t)(ROW0_Y + (KEY_H + KEY_GAP)), row1Rects, 10);
        layoutRow((int16_t)(ROW0_Y + 2 * (KEY_H + KEY_GAP)), row2Rects, 9);
        layoutRow((int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), row3Rects, 7);
        layoutRow((int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), symbolRow, 4);
        Rect shiftBtn = symbolRow[0];
        Rect symbolRects[3] = {symbolRow[1], symbolRow[2], symbolRow[3]};

        Rect backspaceBtn = {4, (int16_t)(ROW0_Y + 5 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8), KEY_H};
        Rect cancelBtn    = {4, (int16_t)(ROW0_Y + 6 * (KEY_H + KEY_GAP)), 110, KEY_H};
        Rect confirmBtn   = {118, (int16_t)(ROW0_Y + 6 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8 - 114), KEY_H};

        bool done = false;
        bool confirmed = false;

        // Faengt einen einzelnen Buchstaben in der aktuellen Gross-/
        // Kleinschreibung ein - Ziffern/Sonderzeichen bleiben von
        // "uppercase" unberuehrt (haben keine Gross-/Kleinform).
        auto letterChar = [&](char upper) -> char {
            return uppercase ? upper : (char)tolower((unsigned char)upper);
        };

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
            tft.print(buf);
            tft.setTextSize(1);

            for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(DIGITS[i]));
            for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(letterChar(ROW1[i])));
            for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(letterChar(ROW2[i])));
            for (uint8_t i = 0; i < 7; i++) drawButton(tft, row3Rects[i], String(letterChar(ROW3[i])));
            // Aktiver Zustand (gefuellt) zeigt "Grossschreibung gerade an" -
            // gleiche Aktiv-Optik wie sonstige Toggle-Buttons im Projekt.
            // ASCII-Zirkumflex statt eines Pfeil-Symbols - der eigene Font
            // (ui_font.h) deckt nur U+0020-U+015F ab, ein echtes Shift-
            // Pfeilsymbol (z.B. U+21E7) wuerde als leere Box erscheinen.
            drawButton(tft, shiftBtn, "^", uppercase);
            for (uint8_t i = 0; i < 3; i++) drawButton(tft, symbolRects[i], String(SYMBOLS[i]));

            drawButton(tft, backspaceBtn, "<- Backspace");
            drawButton(tft, cancelBtn, I18n::t(StringId::CANCEL));
            drawButton(tft, confirmBtn, I18n::t(StringId::OK));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            bool typedChar = false;
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (digitRects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = DIGITS[i]; buf[len] = 0; handled = true; typedChar = true; }
            }
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (row1Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = letterChar(ROW1[i]); buf[len] = 0; handled = true; typedChar = true; }
            }
            for (uint8_t i = 0; i < 9 && !handled; i++) {
                if (row2Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = letterChar(ROW2[i]); buf[len] = 0; handled = true; typedChar = true; }
            }
            for (uint8_t i = 0; i < 7 && !handled; i++) {
                if (row3Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = letterChar(ROW3[i]); buf[len] = 0; handled = true; typedChar = true; }
            }
            if (!handled && shiftBtn.contains(tap.x, tap.y)) {
                uppercase = !uppercase;
                handled = true;
            }
            for (uint8_t i = 0; i < 3 && !handled; i++) {
                if (symbolRects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = SYMBOLS[i]; buf[len] = 0; handled = true; typedChar = true; }
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
            if (!handled && confirmBtn.contains(tap.x, tap.y) && len > 0) {
                done = true;
                confirmed = true;
                handled = true;
            }

            // Nach dem ALLERERSTEN getippten Zeichen automatisch auf
            // Kleinschreibung umschalten (Alex' Wunsch, uebliches Handy-
            // Tastatur-Verhalten) - "len == 1" nach dem Einfuegen oben heisst
            // buf war davor leer, dies war also das erste Zeichen.
            if (typedChar && len == 1) uppercase = false;

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
        layoutWrapped(tft, 10, 14, Config::SCREEN_WIDTH - 50, 14, I18n::t(StringId::NTFY_PUSH_TITLE));

        Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
        drawButton(tft, infoBtn, "?");

        Rect enableRow = rowRect(0);
        drawCheckboxRow(tft, enableRow, I18n::t(StringId::NTFY_PUSH_ENABLE), SettingsStore::ntfyPushEnabled());
        drawRowInfoButton(tft, enableRow);

        String topic = SettingsStore::ntfyPushTopic();
        String topicValue = topic.length() ? topic : String(I18n::t(StringId::MQTT_NOT_SET));
        Rect topicRow = rowRect(1);
        // Zeilenbreite abzueglich Rand/Polsterung von drawLeftButton() (8px
        // links + etwas Luft rechts) - siehe truncateToFit()-Kommentar oben.
        String topicLabel = truncateToFit(tft, I18n::t(StringId::NTFY_PUSH_TOPIC_LABEL), topicValue,
                                           (int16_t)(topicRow.w - 16));
        drawLeftButton(tft, topicRow, topicLabel);

        // Nur antippbar, wenn ueberhaupt ein Topic gesetzt ist - ohne Topic
        // weiss NtfyPush::update() ohnehin nicht, wohin gesendet werden
        // soll (siehe dortiger stiller Abbruch), ein deaktiviert wirkender
        // Knopf ist hier klarer als ein Tap, der sichtbar nichts bewirkt.
        bool canTest = topic.length() > 0;
        Rect testBtn = rowRect(2);
        drawButton(tft, testBtn, I18n::t(StringId::NTFY_PUSH_TEST_BUTTON));

        Rect backBtn = rowRect(3);
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
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::NTFY_PUSH_INFO_TITLE),
                                        I18n::t(StringId::NTFY_PUSH_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
        } else if (enableRow.contains(tap.x, tap.y)) {
            SettingsStore::setNtfyPushEnabled(!SettingsStore::ntfyPushEnabled());
        } else if (topicRow.contains(tap.x, tap.y)) {
            String value = runTopicKeypad(tft, I18n::t(StringId::NTFY_PUSH_TOPIC_PROMPT));
            if (value.length() > 0) SettingsStore::setNtfyPushTopic(value);
        } else if (canTest && testBtn.contains(tap.x, tap.y)) {
            // Bewusst UNABHAENGIG vom Haupt-Schalter (ntfyPushEnabled()) -
            // der Testversand soll auch waehrend der Einrichtung
            // funktionieren, bevor man die automatische Benachrichtigung
            // ueberhaupt einschaltet. NtfyPush::request() merkt die
            // Nachricht nur vor, der eigentliche Versand laeuft
            // asynchron auf Core 0.
            NtfyPush::request(I18n::t(StringId::NTFY_PUSH_TEST_MESSAGE));

            // Kurze visuelle Rueckmeldung, dass der Tap registriert wurde
            // (der eigentliche Versand ist ja bewusst nicht-blockierend,
            // ohne diese Rueckmeldung wirkt der Knopf sonst wirkungslos).
            drawButton(tft, testBtn, I18n::t(StringId::NTFY_PUSH_TEST_SENT), true);
            uint32_t feedbackStartMs = millis();
            while (millis() - feedbackStartMs < 1200) {
                MenuStars::update(tft);
                delay(20);
            }
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
