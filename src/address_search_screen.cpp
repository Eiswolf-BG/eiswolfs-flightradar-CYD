#include "address_search_screen.h"
#include "location_presets.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "settings_store.h"
#include "config.h"
#include "i18n.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cstring>
#include "ui_theme.h"

namespace AddressSearchScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label,
                     bool active = false, bool danger = false) {
        uint16_t accent = danger ? TFT_RED : UiTheme::accentColor(tft);
        uint16_t bg = active ? accent : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : accent;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Lokale Kopie (siehe Konvention in location_presets_screen.cpp - jeder
    // Screen haelt seine eigenen kleinen Helfer statt eines gemeinsamen
    // Moduls). Ohne Scroll-Unterstuetzung - hier immer nur kurze,
    // vorab abgeschnittene Texte (siehe runConfirmScreen/showErrorRetry).
    // Lokale Kopie (siehe Konvention in location_presets_screen.cpp - jeder
    // Screen haelt seine eigenen kleinen Helfer statt eines gemeinsamen
    // Moduls). Optionale Scroll-Unterstuetzung (scrollY/viewTop/viewBottom/
    // draw) fuer runConfirmScreen() unten - bei den bestehenden Aufrufstellen
    // (showErrorRetry, Kopfzeilen) bleiben die Defaults aktiv, es wird also
    // wie bisher immer alles auf einmal gezeichnet.
    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                           int16_t lineHeight, const String& text,
                           int16_t scrollY = 0, int16_t viewTop = -32000, int16_t viewBottom = 32000,
                           bool draw = true) {
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
            if (draw) {
                int16_t screenY = (int16_t)(y - scrollY);
                if (screenY >= viewTop && screenY <= viewBottom) {
                    tft.setCursor(x, screenY);
                    tft.print(line);
                }
            }
            y += lineHeight;
            start += line.length();
        }
        return y;
    }

    // ---- UTF-8-bewusste Puffer-Helfer (fuer die Sonderzeichen-Tasten wie
    // "Ä", die als 2-Byte-UTF-8-Sequenz eingefuegt werden muessen) ----
    void appendUtf8(char* buf, uint8_t& len, uint8_t cap, const char* s) {
        size_t sl = strlen(s);
        if (len + sl >= cap) return;
        memcpy(buf + len, s, sl);
        len += (uint8_t)sl;
        buf[len] = 0;
    }

    // Entfernt beim Loeschen eine ganze UTF-8-Zeichensequenz (nicht nur ein
    // einzelnes Byte) - sonst bliebe bei Sonderzeichen ein kaputtes
    // halbes Byte im Puffer stehen.
    void backspaceUtf8(char* buf, uint8_t& len) {
        if (len == 0) return;
        len--;
        while (len > 0 && (((uint8_t)buf[len]) & 0xC0) == 0x80) len--;
        buf[len] = 0;
    }

    // Zeigt nur so viel vom ENDE des Puffers, wie in maxWidth passt - der
    // Nutzer tippt am Ende weiter, das sichtbare Fenster soll dem folgen
    // (wie bei einem gewoehnlichen einzeiligen Texteingabefeld).
    String visibleTail(TFT_eSPI& tft, const char* buf, int16_t maxWidth) {
        String s(buf);
        while (s.length() > 0 && tft.textWidth(s) > maxWidth) {
            int32_t cut = ((((uint8_t)s[0]) & 0xE0) == 0xC0) ? 2 : 1;
            if (cut > (int32_t)s.length()) cut = s.length();
            s = s.substring(cut);
        }
        return s;
    }

    // ---- Tastatur-Layout ----
    constexpr const char* DIGITS = "1234567890";
    constexpr const char* ROW1 = "QWERTYUIOP";
    constexpr const char* ROW2 = "ASDFGHJKL";
    // Um Komma und Bindestrich erweitert (7->9 Tasten) - Adressen wie eine
    // Hausnummer "45/3" oder "Strasse 12, Ort" waren sonst gar nicht
    // eintippbar, da die Tastatur bisher keinerlei Satzzeichen enthielt.
    constexpr const char* ROW3 = "ZXCVBNM,-";

    // Sonderzeichen-Seite: in einer frueheren Version wurden die selteneren
    // Â/Ë/Î/Û gegen die fuer Adressen wichtigen Satzzeichen "/", ".", "'", "-"
    // getauscht (Hausnummern wie "45/3", Abkuerzungen wie "Str.", Apostroph-
    // Namen). Auf erneute Nachfrage (Font-/Tastatur-Analyse) jetzt als
    // fuenfte Zeile ERGAENZT statt wieder zu ersetzen - deckt seltenere
    // franzoesische Ortsnamen ab (z.B. "Chateauneuf", "Ile-de-France").
    // Alle vier Glyphen waren bereits im Font vorhanden (ui_font.h deckt
    // U+0020-U+015F durchgehend ab), keine neuen Pixel-Glyphen noetig.
    constexpr const char* SPEC0[6] = {"À", "Á", "Ä", "Ç", "É", "È"};
    constexpr const char* SPEC1[6] = {"Ê", "Í", "Ñ", "Ó", "Ò", "Ô"};
    constexpr const char* SPEC2[6] = {"Ö", "Ù", "Ú", "Ü", "ß", "Ğ"};
    constexpr const char* SPEC3[6] = {"İ", "Ş", "/", ".", "'", "-"};
    // Ï (I-Trema) ergaenzt (Alex' Wunsch, relevant fuer niederlaendische
    // Ortsnamen wie "IJsselstein") - war bisher nicht auf der Tastatur,
    // obwohl die Glyphe im Font (ui_font.h) schon vorhanden war (siehe
    // i18n_nl.h-Font-Pruefung). Ë war bereits vorhanden.
    constexpr const char* SPEC4[5] = {"Â", "Î", "Ë", "Û", "Ï"};
    constexpr uint8_t SPEC_ROW_COUNT = 5;

    // Gibt die eingegebene Adresse zurueck, oder einen leeren String, wenn
    // der Nutzer abgebrochen hat.
    String runAddressKeyboard(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr uint8_t CAP = 64;
        char buf[CAP] = {0};
        uint8_t len = 0;
        bool specialPage = false;

        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t FIELD_H = 34;

        // Kopfbereich (Titel + Format-Hinweis) einmal zeichnen, um seine
        // tatsaechliche Hoehe per getCursorY() zu messen (variiert je nach
        // Sprache/Uebersetzungslaenge) - Eingabefeld und Tastatur-Start
        // werden daraus abgeleitet statt wie zuvor eine feste Pixelposition
        // (78) anzunehmen. redraw() unten zeichnet denselben Kopf bei jedem
        // Aufruf identisch neu.
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::ADDRESS_SEARCH_TITLE));
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println(I18n::t(StringId::ADDRESS_SEARCH_HINT));
        int16_t fieldY = (int16_t)(tft.getCursorY() + 4);
        int16_t ROW0_Y = (int16_t)(fieldY + FIELD_H + 8);

        // KEY_H wird dynamisch aus dem tatsaechlich verbleibenden Platz
        // berechnet (gleiches Prinzip wie ROW_H in radar_theme_screen.cpp)
        // statt fest 30px anzunehmen - mit der fuenften Sonderzeichen-Zeile
        // (SPEC4, siehe oben) reichten feste 30px nicht mehr, die unterste
        // Aktionszeile (Zurueck/Bestaetigen) wurde ueber den Bildschirmrand
        // hinausgeschoben (Alex' Meldung). Ungueststigster Fall ist die
        // Sonderzeichen-Seite: 5 Inhaltszeilen + Leertaste/Loeschen-Zeile +
        // Aktionszeile = 7 Zeilen muessen ab ROW0_Y noch aufs Display
        // passen - dieselbe KEY_H gilt danach fuer BEIDE Tastatur-Seiten
        // (einheitliche Tastengroesse statt springender Groesse beim
        // Umschalten).
        constexpr uint8_t TOTAL_ROWS = SPEC_ROW_COUNT + 2;
        constexpr int16_t BOTTOM_MARGIN = 6;
        int16_t availableH = (int16_t)(Config::SCREEN_HEIGHT - ROW0_Y - BOTTOM_MARGIN);
        int16_t KEY_H = (int16_t)((availableH - (TOTAL_ROWS - 1) * KEY_GAP) / TOTAL_ROWS);

        auto layoutRow = [&](int16_t y, Rect* outRects, uint8_t n) {
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect digitRects[10], row1Rects[10], row2Rects[9], row3Rects[9];
        layoutRow(ROW0_Y, digitRects, 10);
        layoutRow((int16_t)(ROW0_Y + (KEY_H + KEY_GAP)), row1Rects, 10);
        layoutRow((int16_t)(ROW0_Y + 2 * (KEY_H + KEY_GAP)), row2Rects, 9);
        layoutRow((int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), row3Rects, 9);

        // 5 Sonderzeichen-Zeilen (SPEC0-3 je 6, SPEC4 nur 5 Tasten) - eine
        // Zeile mehr als die 4 Buchstaben-Zeilen oben, deshalb haengt die
        // Y-Position der Leertaste/Aktionsleiste unten von der aktuellen
        // Seite ab (siehe contentRows()/layoutTrailingButtons() unten).
        Rect specRects[SPEC_ROW_COUNT][6];
        for (uint8_t r = 0; r < 4; r++) {
            layoutRow((int16_t)(ROW0_Y + r * (KEY_H + KEY_GAP)), specRects[r], 6);
        }
        layoutRow((int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), specRects[4], 5);

        Rect spaceBtn, backspaceBtn, toggleBtn, cancelBtn, searchBtn;
        constexpr int16_t BTN_W = (Config::SCREEN_WIDTH - 8 - 2 * KEY_GAP) / 3;

        // Buchstaben-Seite hat 4 Inhaltszeilen, die Sonderzeichen-Seite 5 -
        // die Leertaste/Aktionsleiste ruecken auf der Sonderzeichen-Seite
        // deshalb eine Zeile weiter nach unten.
        auto layoutTrailingButtons = [&]() {
            uint8_t contentRows = specialPage ? SPEC_ROW_COUNT : 4;
            int16_t spaceRowY = (int16_t)(ROW0_Y + contentRows * (KEY_H + KEY_GAP));
            int16_t btnRowY = (int16_t)(ROW0_Y + (contentRows + 1) * (KEY_H + KEY_GAP));
            spaceBtn     = {4, spaceRowY, 150, KEY_H};
            backspaceBtn = {158, spaceRowY, (int16_t)(Config::SCREEN_WIDTH - 8 - 154), KEY_H};
            toggleBtn = {4, btnRowY, BTN_W, KEY_H};
            cancelBtn = {(int16_t)(4 + BTN_W + KEY_GAP), btnRowY, BTN_W, KEY_H};
            searchBtn = {(int16_t)(4 + 2 * (BTN_W + KEY_GAP)), btnRowY,
                         (int16_t)(Config::SCREEN_WIDTH - 8 - 2 * (BTN_W + KEY_GAP)), KEY_H};
        };
        layoutTrailingButtons();

        bool done = false;
        bool searched = false;

        auto redraw = [&]() {
            layoutTrailingButtons();
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::ADDRESS_SEARCH_TITLE));
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            tft.println(I18n::t(StringId::ADDRESS_SEARCH_HINT));

            tft.fillRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, TFT_BLACK);
            tft.drawRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, UiTheme::accentColor(tft));
            tft.setTextSize(2);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(14, (int16_t)(fieldY + 26));
            tft.print(visibleTail(tft, buf, (int16_t)(Config::SCREEN_WIDTH - 28)));
            tft.setTextSize(1);

            if (!specialPage) {
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(DIGITS[i]));
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(ROW1[i]));
                for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(ROW2[i]));
                for (uint8_t i = 0; i < 9; i++) drawButton(tft, row3Rects[i], String(ROW3[i]));
            } else {
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[0][i], SPEC0[i]);
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[1][i], SPEC1[i]);
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[2][i], SPEC2[i]);
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[3][i], SPEC3[i]);
                for (uint8_t i = 0; i < 5; i++) drawButton(tft, specRects[4][i], SPEC4[i]);
            }

            drawButton(tft, spaceBtn, I18n::t(StringId::WIFI_SPACE));
            drawButton(tft, backspaceBtn, "<-");
            drawButton(tft, toggleBtn, specialPage ? "ABC" : "ÄÖÜ");
            drawButton(tft, cancelBtn, I18n::t(StringId::ADDRESS_SEARCH_CANCEL), false, true);
            drawButton(tft, searchBtn, I18n::t(StringId::OK));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            if (!specialPage) {
                for (uint8_t i = 0; i < 10 && !handled; i++) {
                    if (digitRects[i].contains(tap.x, tap.y)) {
                        char s[2] = {DIGITS[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 10 && !handled; i++) {
                    if (row1Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {ROW1[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 9 && !handled; i++) {
                    if (row2Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {ROW2[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 9 && !handled; i++) {
                    if (row3Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {ROW3[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
            } else {
                for (uint8_t i = 0; i < 6 && !handled; i++) {
                    if (specRects[0][i].contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, SPEC0[i]); handled = true; }
                }
                for (uint8_t i = 0; i < 6 && !handled; i++) {
                    if (specRects[1][i].contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, SPEC1[i]); handled = true; }
                }
                for (uint8_t i = 0; i < 6 && !handled; i++) {
                    if (specRects[2][i].contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, SPEC2[i]); handled = true; }
                }
                for (uint8_t i = 0; i < 6 && !handled; i++) {
                    if (specRects[3][i].contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, SPEC3[i]); handled = true; }
                }
                for (uint8_t i = 0; i < 5 && !handled; i++) {
                    if (specRects[4][i].contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, SPEC4[i]); handled = true; }
                }
            }
            if (!handled && spaceBtn.contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, " "); handled = true; }
            if (!handled && backspaceBtn.contains(tap.x, tap.y)) { backspaceUtf8(buf, len); handled = true; }
            if (!handled && toggleBtn.contains(tap.x, tap.y)) { specialPage = !specialPage; handled = true; }
            if (!handled && cancelBtn.contains(tap.x, tap.y)) {
                buf[0] = 0;
                len = 0;
                done = true;
                searched = false;
                handled = true;
            }
            if (!handled && searchBtn.contains(tap.x, tap.y) && len > 0) {
                done = true;
                searched = true;
                handled = true;
            }

            if (handled) redraw();
        }

        return searched ? String(buf) : String();
    }

    // Namens-Tastatur fuer den finalen Schritt (Preset-Name vergeben) -
    // spiegelt runPresetNameKeypad() aus location_presets_screen.cpp.
    // Seit der Font-/Tastatur-Analyse (Alex' Wunsch) jetzt MIT
    // Sonderzeichen-Seite (gleicher Zeichensatz/gleiches Muster wie
    // runAddressKeyboard() oben, per appendUtf8()/backspaceUtf8()), damit
    // z.B. "Zürich" oder "São Paulo" korrekt als Preset-Name eingetippt
    // werden koennen - vorher bewusst ASCII-only, das war der Auftrag.
    String runNameKeypad(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr const char* NDIGITS = "1234567890";
        constexpr const char* NROW1 = "QWERTYUIOP";
        constexpr const char* NROW2 = "ASDFGHJKL";
        constexpr const char* NROW3 = "ZXCVBNM";

        // Kapazitaet bewusst identisch zu Preset::name (siehe
        // location_presets.cpp, char name[17]) gehalten, NICHT groesser -
        // ein laengerer Eingabepuffer, der dann per strncpy() in
        // addPreset() auf 16 Byte gekuerzt wird, koennte ein Mehrbyte-
        // UTF-8-Sonderzeichen mitten entzweischneiden und einen kaputten
        // Namen abspeichern.
        constexpr uint8_t CAP = 17;
        char buf[CAP] = {0};
        uint8_t len = 0;
        bool specialPage = false;

        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t FIELD_H = 34;

        // Kopfbereich (Titel + Namens-Hinweis) einmal zeichnen, um seine
        // tatsaechliche Hoehe per getCursorY() zu messen - selbes Muster wie
        // in runAddressKeyboard() oben.
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::LOCATION_NAME_PROMPT));
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println(I18n::t(StringId::LOCATION_NAME_HINT));
        int16_t fieldY = (int16_t)(tft.getCursorY() + 4);
        int16_t ROW0_Y = (int16_t)(fieldY + FIELD_H + 8);

        // KEY_H dynamisch berechnet - siehe ausfuehrlicher Kommentar in
        // runAddressKeyboard() oben (gleicher Fix, gleicher Grund: die
        // fuenfte Sonderzeichen-Zeile passte mit fest 30px nicht mehr aufs
        // Display).
        constexpr uint8_t TOTAL_ROWS = SPEC_ROW_COUNT + 2;
        constexpr int16_t BOTTOM_MARGIN = 6;
        int16_t availableH = (int16_t)(Config::SCREEN_HEIGHT - ROW0_Y - BOTTOM_MARGIN);
        int16_t KEY_H = (int16_t)((availableH - (TOTAL_ROWS - 1) * KEY_GAP) / TOTAL_ROWS);

        auto layoutRow = [&](const char* row, int16_t y, Rect* outRects, uint8_t n) {
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect digitRects[10], row1Rects[10], row2Rects[9], row3Rects[7];
        layoutRow(NDIGITS, ROW0_Y, digitRects, 10);
        layoutRow(NROW1, (int16_t)(ROW0_Y + (KEY_H + KEY_GAP)), row1Rects, 10);
        layoutRow(NROW2, (int16_t)(ROW0_Y + 2 * (KEY_H + KEY_GAP)), row2Rects, 9);
        layoutRow(NROW3, (int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), row3Rects, 7);

        // Gleiche 5 Sonderzeichen-Zeilen wie runAddressKeyboard() oben
        // (SPEC0-4) - dupliziert statt geteilt waere hier nicht mal noetig,
        // da SPEC0-4 als Datei-weite Konstanten bereits oben definiert
        // sind und von beiden Funktionen genutzt werden koennen.
        Rect specRects[SPEC_ROW_COUNT][6];
        for (uint8_t r = 0; r < 4; r++) {
            layoutRow(nullptr, (int16_t)(ROW0_Y + r * (KEY_H + KEY_GAP)), specRects[r], 6);
        }
        layoutRow(nullptr, (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), specRects[4], 5);

        Rect spaceBtn, backspaceBtn, toggleBtn, skipBtn, confirmBtn;

        // Buchstaben-Seite hat 4 Inhaltszeilen, die Sonderzeichen-Seite 5 -
        // gleiches Prinzip wie in runAddressKeyboard() oben.
        auto layoutTrailingButtons = [&]() {
            uint8_t contentRows = specialPage ? SPEC_ROW_COUNT : 4;
            int16_t spaceRowY = (int16_t)(ROW0_Y + contentRows * (KEY_H + KEY_GAP));
            int16_t btnRowY = (int16_t)(ROW0_Y + (contentRows + 1) * (KEY_H + KEY_GAP));
            spaceBtn     = {4, spaceRowY, 150, KEY_H};
            backspaceBtn = {158, spaceRowY, (int16_t)(Config::SCREEN_WIDTH - 8 - 154), KEY_H};
            toggleBtn = {4, btnRowY, 80, KEY_H};
            skipBtn   = {(int16_t)(4 + 80 + KEY_GAP), btnRowY, 90, KEY_H};
            confirmBtn = {(int16_t)(4 + 80 + KEY_GAP + 90 + KEY_GAP), btnRowY,
                          (int16_t)(Config::SCREEN_WIDTH - 8 - (80 + KEY_GAP + 90 + KEY_GAP)), KEY_H};
        };
        layoutTrailingButtons();

        bool done = false;

        auto redraw = [&]() {
            layoutTrailingButtons();
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::LOCATION_NAME_PROMPT));
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            tft.println(I18n::t(StringId::LOCATION_NAME_HINT));

            tft.fillRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, TFT_BLACK);
            tft.drawRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, UiTheme::accentColor(tft));
            tft.setTextSize(2);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(14, (int16_t)(fieldY + 26));
            tft.print(visibleTail(tft, buf, (int16_t)(Config::SCREEN_WIDTH - 28)));
            tft.setTextSize(1);

            if (!specialPage) {
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(NDIGITS[i]));
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(NROW1[i]));
                for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(NROW2[i]));
                for (uint8_t i = 0; i < 7; i++) drawButton(tft, row3Rects[i], String(NROW3[i]));
            } else {
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[0][i], SPEC0[i]);
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[1][i], SPEC1[i]);
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[2][i], SPEC2[i]);
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[3][i], SPEC3[i]);
                for (uint8_t i = 0; i < 5; i++) drawButton(tft, specRects[4][i], SPEC4[i]);
            }

            drawButton(tft, spaceBtn, I18n::t(StringId::WIFI_SPACE));
            drawButton(tft, backspaceBtn, "<-");
            drawButton(tft, toggleBtn, specialPage ? "ABC" : "ÄÖÜ");
            drawButton(tft, skipBtn, I18n::t(StringId::LOCATION_NAME_SKIP));
            drawButton(tft, confirmBtn, I18n::t(StringId::OK));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            if (!specialPage) {
                for (uint8_t i = 0; i < 10 && !handled; i++) {
                    if (digitRects[i].contains(tap.x, tap.y)) {
                        char s[2] = {NDIGITS[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 10 && !handled; i++) {
                    if (row1Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {NROW1[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 9 && !handled; i++) {
                    if (row2Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {NROW2[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 7 && !handled; i++) {
                    if (row3Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {NROW3[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
            } else {
                for (uint8_t i = 0; i < 6 && !handled; i++) {
                    if (specRects[0][i].contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, SPEC0[i]); handled = true; }
                }
                for (uint8_t i = 0; i < 6 && !handled; i++) {
                    if (specRects[1][i].contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, SPEC1[i]); handled = true; }
                }
                for (uint8_t i = 0; i < 6 && !handled; i++) {
                    if (specRects[2][i].contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, SPEC2[i]); handled = true; }
                }
                for (uint8_t i = 0; i < 6 && !handled; i++) {
                    if (specRects[3][i].contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, SPEC3[i]); handled = true; }
                }
                for (uint8_t i = 0; i < 5 && !handled; i++) {
                    if (specRects[4][i].contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, SPEC4[i]); handled = true; }
                }
            }
            if (!handled && spaceBtn.contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, " "); handled = true; }
            if (!handled && backspaceBtn.contains(tap.x, tap.y)) { backspaceUtf8(buf, len); handled = true; }
            if (!handled && toggleBtn.contains(tap.x, tap.y)) { specialPage = !specialPage; handled = true; }
            if (!handled && skipBtn.contains(tap.x, tap.y)) { buf[0] = 0; len = 0; done = true; handled = true; }
            if (!handled && confirmBtn.contains(tap.x, tap.y)) { done = true; handled = true; }

            if (handled) redraw();
        }

        return String(buf);
    }

    // Prozentkodiert einen UTF-8-String fuer die Nominatim-Query (jedes
    // Roh-Byte einzeln - funktioniert damit auch fuer die mehrbytigen
    // Sonderzeichen).
    String urlEncode(const String& s) {
        String out;
        char hex[4];
        for (size_t i = 0; i < s.length(); i++) {
            uint8_t c = (uint8_t)s[i];
            bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                               (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
            if (unreserved) {
                out += (char)c;
            } else {
                snprintf(hex, sizeof(hex), "%%%02X", c);
                out += hex;
            }
        }
        return out;
    }

    // ISO-639-1-Codes in derselben Reihenfolge wie I18n::TABLES (siehe
    // i18n.cpp) - fuer staerker lokalisierte display_name-Ergebnisse
    // (Nominatim accept-language-Parameter).
    constexpr const char* NOMINATIM_LANG_CODES[6] = {"en", "de", "fr", "tr", "es", "it"};

    enum class GeocodeResult { Ok, NoResults, NetworkError };

    // Kostenloser, anmeldefreier Geokodierungs-Dienst (OpenStreetMap
    // Nominatim, siehe Config::NOMINATIM_HOST). Blockierender Aufruf,
    // ausgeloest durch Nutzer-Interaktion (Tap auf "Suchen") - laeuft im
    // Vordergrund auf Core 1, exakt wie der bestehende Flugzeug-Detail-
    // Abruf in aircraft_details.cpp.
    GeocodeResult geocode(const String& query, double& lat, double& lon, String& displayName) {
        if (WiFi.status() != WL_CONNECTED) return GeocodeResult::NetworkError;

        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(Config::HTTP_TIMEOUT_MS);

        String url = "https://" + String(Config::NOMINATIM_HOST) +
                     "/search?format=json&limit=1&q=" + urlEncode(query);
        uint8_t lang = SettingsStore::language();
        if (lang < 6) url += "&accept-language=" + String(NOMINATIM_LANG_CODES[lang]);

        HTTPClient http;
        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return GeocodeResult::NetworkError;
        http.addHeader("User-Agent", Config::NOMINATIM_USER_AGENT);

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            http.end();
            return GeocodeResult::NetworkError;
        }

        // Body erst komplett einsammeln statt direkt aus http.getStream()
        // zu parsen - siehe weather.cpp, gleicher Grund (Chunked-Transfer-
        // Encoding fuehrte dort sonst zu ArduinoJson-"InvalidInput").
        String body = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) return GeocodeResult::NetworkError;

        JsonArray arr = doc.as<JsonArray>();
        if (arr.isNull() || arr.size() == 0) return GeocodeResult::NoResults;

        JsonObject first = arr[0];
        const char* latStr = first["lat"] | "";
        const char* lonStr = first["lon"] | "";
        if (!latStr[0] || !lonStr[0]) return GeocodeResult::NoResults;

        lat = atof(latStr);
        lon = atof(lonStr);
        const char* dn = first["display_name"] | "";
        displayName = String(dn);
        return GeocodeResult::Ok;
    }

    // Fehler-/Kein-Ergebnis-Screen mit "Erneut versuchen"/"Abbrechen".
    // Gibt true zurueck, wenn der Nutzer es erneut versuchen will.
    bool showErrorRetry(TFT_eSPI& tft, StringId msgId) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 18, I18n::t(msgId));

        Rect retryBtn  = {10, (int16_t)(Config::SCREEN_HEIGHT - 96), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect cancelBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, retryBtn, I18n::t(StringId::ADDRESS_SEARCH_TRY_AGAIN));
        drawButton(tft, cancelBtn, I18n::t(StringId::ADDRESS_SEARCH_CANCEL), false, true);

        while (true) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }
            if (retryBtn.contains(tap.x, tap.y)) return true;
            if (cancelBtn.contains(tap.x, tap.y)) return false;
        }
    }

    // Bestaetigungs-Screen mit dem von Nominatim gefundenen Ort. Gibt 1
    // zurueck ("verwenden"), 0 fuer "erneut versuchen" (zurueck zur
    // Adress-Tastatur).
    int runConfirmScreen(TFT_eSPI& tft, const String& displayName) {
        MenuStars::reset();

        constexpr int16_t textMaxWidth = Config::SCREEN_WIDTH - 20;
        constexpr int16_t LINE_H = 18;
        constexpr int16_t VIEW_TOP = 40;
        constexpr int16_t TRY_AGAIN_Y = (int16_t)(Config::SCREEN_HEIGHT - 96);
        constexpr int16_t USE_Y = (int16_t)(Config::SCREEN_HEIGHT - 50);
        constexpr int16_t VIEW_BOTTOM = (int16_t)(TRY_AGAIN_Y - 10);

        // Volle Adresse OHNE Kuerzung anzeigen (frueher wurde bei > 150
        // Zeichen mit "..." abgeschnitten) - stattdessen wird der Absatz bei
        // Bedarf vertikal scrollbar, exakt dasselbe Muster wie beim "Wie
        // funktionieren Presets"-Infoscreen (location_presets_screen.cpp
        // ::runInfoScreen). Bewusst KEIN Marquee hier: das ist ein
        // mehrzeiliger, wortumgebrochener Absatz, keine einzelne Zeile, die
        // in der Breite nicht passt - horizontales Scrollen waere hier die
        // falsche Loesung.
        int16_t totalH = layoutWrapped(tft, 10, VIEW_TOP, textMaxWidth, LINE_H, displayName, 0, 0, 0, false);
        int16_t maxScroll = (int16_t)(totalH - VIEW_BOTTOM);
        if (maxScroll < 0) maxScroll = 0;
        bool scrollable = maxScroll > 0;
        int16_t scrollY = 0;

        Rect tryAgainBtn = {10, TRY_AGAIN_Y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect useBtn = scrollable
            ? Rect{10, USE_Y, 130, 40}
            : Rect{10, USE_Y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect upBtn   = {146, USE_Y, 38, 40};
        Rect downBtn = {190, USE_Y, 38, 40};
        constexpr int16_t SCROLL_STEP = 48;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::ADDRESS_SEARCH_CONFIRM_TITLE));

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            layoutWrapped(tft, 10, VIEW_TOP, textMaxWidth, LINE_H, displayName, scrollY, VIEW_TOP, VIEW_BOTTOM, true);

            drawButton(tft, tryAgainBtn, I18n::t(StringId::ADDRESS_SEARCH_TRY_AGAIN));
            drawButton(tft, useBtn, I18n::t(StringId::ADDRESS_SEARCH_USE_THIS));
            if (scrollable) {
                drawButton(tft, upBtn, "^");
                drawButton(tft, downBtn, "v");
            }
        };

        redraw();

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (tryAgainBtn.contains(tap.x, tap.y)) return 0;
                if (useBtn.contains(tap.x, tap.y)) return 1;
                if (scrollable && upBtn.contains(tap.x, tap.y) && scrollY > 0) {
                    scrollY -= SCROLL_STEP;
                    if (scrollY < 0) scrollY = 0;
                    redraw();
                } else if (scrollable && downBtn.contains(tap.x, tap.y) && scrollY < maxScroll) {
                    scrollY += SCROLL_STEP;
                    if (scrollY > maxScroll) scrollY = maxScroll;
                    redraw();
                }
            }
            MenuStars::update(tft);
            delay(20);
        }
    }
}

bool run(TFT_eSPI& tft) {
    while (true) {
        String address = runAddressKeyboard(tft);
        if (address.length() == 0) return false;

        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.drawString(I18n::t(StringId::ADDRESS_SEARCH_SEARCHING), Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
        tft.setTextDatum(TL_DATUM);

        double lat = 0, lon = 0;
        String displayName;
        GeocodeResult result = geocode(address, lat, lon, displayName);

        if (result == GeocodeResult::NetworkError) {
            if (!showErrorRetry(tft, StringId::ADDRESS_SEARCH_ERROR)) return false;
            continue;
        }
        if (result == GeocodeResult::NoResults) {
            if (!showErrorRetry(tft, StringId::ADDRESS_SEARCH_NO_RESULTS)) return false;
            continue;
        }

        int choice = runConfirmScreen(tft, displayName);
        if (choice == 0) continue;

        String name = runNameKeypad(tft);
        if (!LocationPresets::addPreset(lat, lon, name)) return false;
        // Neu angelegtes Preset sofort aktivieren - sonst blieb z.B. der
        // automatische IP-Standort aktiv, obwohl gerade extra eine genauere
        // Adresse eingegeben wurde (siehe Alex' Feedback).
        LocationPresets::setActiveIndex((int8_t)(LocationPresets::count() - 1));
        return true;
    }
}

}
