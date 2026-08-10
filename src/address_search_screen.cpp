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
        uint16_t accent = danger ? TFT_RED : TFT_GREEN;
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
    constexpr const char* ROW3 = "ZXCVBNM";

    // Sonderzeichen-Seite: nur Zeichen, die bereits an anderer Stelle in
    // der App verwendet werden (i18n_de/fr/tr/es/it.h) - der Font
    // (UiFont11pt, deckt U+0020-U+015F ab) stellt sie garantiert korrekt
    // dar. Deckt die in DE/FR/ES/IT/TR-Adressen gaengigsten Sonderzeichen ab.
    constexpr const char* SPEC0[6] = {"À", "Á", "Â", "Ä", "Ç", "É"};
    constexpr const char* SPEC1[6] = {"È", "Ê", "Ë", "Í", "Î", "Ñ"};
    constexpr const char* SPEC2[6] = {"Ó", "Ò", "Ô", "Ö", "Ù", "Ú"};
    constexpr const char* SPEC3[6] = {"Û", "Ü", "ß", "Ğ", "İ", "Ş"};

    // Gibt die eingegebene Adresse zurueck, oder einen leeren String, wenn
    // der Nutzer abgebrochen hat.
    String runAddressKeyboard(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr uint8_t CAP = 64;
        char buf[CAP] = {0};
        uint8_t len = 0;
        bool specialPage = false;

        constexpr int16_t KEY_H = 30;
        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t ROW0_Y = 78;

        auto layoutRow = [&](int16_t y, Rect* outRects, uint8_t n) {
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect digitRects[10], row1Rects[10], row2Rects[9], row3Rects[7];
        layoutRow(ROW0_Y, digitRects, 10);
        layoutRow((int16_t)(ROW0_Y + (KEY_H + KEY_GAP)), row1Rects, 10);
        layoutRow((int16_t)(ROW0_Y + 2 * (KEY_H + KEY_GAP)), row2Rects, 9);
        layoutRow((int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), row3Rects, 7);

        Rect specRects[4][6];
        for (uint8_t r = 0; r < 4; r++) {
            layoutRow((int16_t)(ROW0_Y + r * (KEY_H + KEY_GAP)), specRects[r], 6);
        }

        Rect spaceBtn     = {4, (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), 150, KEY_H};
        Rect backspaceBtn = {158, (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8 - 154), KEY_H};

        constexpr int16_t BTN_ROW_Y = ROW0_Y + 5 * (KEY_H + KEY_GAP);
        constexpr int16_t BTN_W = (Config::SCREEN_WIDTH - 8 - 2 * KEY_GAP) / 3;
        Rect toggleBtn = {4, BTN_ROW_Y, BTN_W, KEY_H};
        Rect cancelBtn = {(int16_t)(4 + BTN_W + KEY_GAP), BTN_ROW_Y, BTN_W, KEY_H};
        Rect searchBtn = {(int16_t)(4 + 2 * (BTN_W + KEY_GAP)), BTN_ROW_Y,
                           (int16_t)(Config::SCREEN_WIDTH - 8 - 2 * (BTN_W + KEY_GAP)), KEY_H};

        bool done = false;
        bool searched = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::ADDRESS_SEARCH_TITLE));

            tft.fillRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_BLACK);
            tft.drawRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_GREEN);
            tft.setTextSize(2);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(14, 66);
            tft.print(visibleTail(tft, buf, (int16_t)(Config::SCREEN_WIDTH - 28)));
            tft.setTextSize(1);

            if (!specialPage) {
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(DIGITS[i]));
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(ROW1[i]));
                for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(ROW2[i]));
                for (uint8_t i = 0; i < 7; i++) drawButton(tft, row3Rects[i], String(ROW3[i]));
            } else {
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[0][i], SPEC0[i]);
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[1][i], SPEC1[i]);
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[2][i], SPEC2[i]);
                for (uint8_t i = 0; i < 6; i++) drawButton(tft, specRects[3][i], SPEC3[i]);
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
                for (uint8_t i = 0; i < 7 && !handled; i++) {
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
    // absichtlich eine eigene, einfache ASCII-Tastatur (keine
    // Sonderzeichen noetig fuer einen Preset-Namen wie "Zuhause"),
    // spiegelt runPresetNameKeypad() aus location_presets_screen.cpp.
    String runNameKeypad(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr const char* NDIGITS = "1234567890";
        constexpr const char* NROW1 = "QWERTYUIOP";
        constexpr const char* NROW2 = "ASDFGHJKL";
        constexpr const char* NROW3 = "ZXCVBNM";

        char buf[17] = {0};
        uint8_t len = 0;

        constexpr int16_t KEY_H = 30;
        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t ROW0_Y = 78;

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

        Rect spaceBtn     = {4, (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), 150, KEY_H};
        Rect backspaceBtn = {158, (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8 - 154), KEY_H};
        Rect skipBtn      = {4, (int16_t)(ROW0_Y + 5 * (KEY_H + KEY_GAP)), 110, KEY_H};
        Rect confirmBtn   = {118, (int16_t)(ROW0_Y + 5 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8 - 114), KEY_H};

        bool done = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::LOCATION_NAME_PROMPT));

            tft.fillRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_BLACK);
            tft.drawRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_GREEN);
            tft.setTextSize(2);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(14, 66);
            tft.print(buf);
            tft.setTextSize(1);

            for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(NDIGITS[i]));
            for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(NROW1[i]));
            for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(NROW2[i]));
            for (uint8_t i = 0; i < 7; i++) drawButton(tft, row3Rects[i], String(NROW3[i]));

            drawButton(tft, spaceBtn, I18n::t(StringId::WIFI_SPACE));
            drawButton(tft, backspaceBtn, "<-");
            drawButton(tft, skipBtn, I18n::t(StringId::LOCATION_NAME_SKIP));
            drawButton(tft, confirmBtn, I18n::t(StringId::OK));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (digitRects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = NDIGITS[i]; buf[len] = 0; handled = true; }
            }
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (row1Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = NROW1[i]; buf[len] = 0; handled = true; }
            }
            for (uint8_t i = 0; i < 9 && !handled; i++) {
                if (row2Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = NROW2[i]; buf[len] = 0; handled = true; }
            }
            for (uint8_t i = 0; i < 7 && !handled; i++) {
                if (row3Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = NROW3[i]; buf[len] = 0; handled = true; }
            }
            if (!handled && spaceBtn.contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = ' '; buf[len] = 0; handled = true; }
            if (!handled && backspaceBtn.contains(tap.x, tap.y)) {
                if (len > 0) { len--; buf[len] = 0; }
                handled = true;
            }
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
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::ADDRESS_SEARCH_CONFIRM_TITLE));

        String shown = displayName;
        if (shown.length() > 150) shown = shown.substring(0, 150) + "...";
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 18, shown);

        Rect tryAgainBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 96), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect useBtn      = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, tryAgainBtn, I18n::t(StringId::ADDRESS_SEARCH_TRY_AGAIN));
        drawButton(tft, useBtn, I18n::t(StringId::ADDRESS_SEARCH_USE_THIS));

        while (true) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }
            if (tryAgainBtn.contains(tap.x, tap.y)) return 0;
            if (useBtn.contains(tap.x, tap.y)) return 1;
        }
    }
}

bool run(TFT_eSPI& tft) {
    while (true) {
        String address = runAddressKeyboard(tft);
        if (address.length() == 0) return false;

        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
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
        return LocationPresets::addPreset(lat, lon, name);
    }
}

}
