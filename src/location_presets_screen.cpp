#include "location_presets_screen.h"
#include "location_presets.h"
#include "location_manager.h"
#include "airport_lookup.h"
#include "address_search_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "units.h"

namespace LocationPresetsScreen {

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

    String runNumericKeypad(TFT_eSPI& tft, const String& title) {
        MenuStars::reset();
        char buf[16] = {0};
        uint8_t len = 0;

        constexpr int16_t KEY_W = 74;
        constexpr int16_t KEY_H = 34;
        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t GRID_LEFT = (Config::SCREEN_WIDTH - 3 * KEY_W - 2 * KEY_GAP) / 2;
        constexpr int16_t GRID_TOP = 84;

        const char* keys[12] = {"1","2","3","4","5","6","7","8","9","-","0","."};
        Rect keyRects[12];
        for (uint8_t i = 0; i < 12; i++) {
            int16_t col = i % 3;
            int16_t row = i / 3;
            keyRects[i] = {(int16_t)(GRID_LEFT + col * (KEY_W + KEY_GAP)),
                           (int16_t)(GRID_TOP + row * (KEY_H + KEY_GAP)),
                           KEY_W, KEY_H};
        }

        Rect backspaceBtn = {(int16_t)GRID_LEFT, (int16_t)(GRID_TOP + 4 * (KEY_H + KEY_GAP)),
                              (int16_t)(3 * KEY_W + 2 * KEY_GAP), 30};
        Rect cancelBtn  = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), 110, 40};
        Rect confirmBtn = {(int16_t)(Config::SCREEN_WIDTH - 120), (int16_t)(Config::SCREEN_HEIGHT - 50), 110, 40};

        bool done = false;
        bool confirmed = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(title);

            tft.fillRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_BLACK);
            tft.drawRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_GREEN);
            tft.setTextSize(2);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(14, 66);
            tft.print(buf);
            tft.setTextSize(1);

            for (uint8_t i = 0; i < 12; i++) drawButton(tft, keyRects[i], keys[i]);
            drawButton(tft, backspaceBtn, "<- Backspace");
            drawButton(tft, cancelBtn, I18n::t(StringId::CANCEL), false, true);
            drawButton(tft, confirmBtn, I18n::t(StringId::OK));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            for (uint8_t i = 0; i < 12 && !handled; i++) {
                if (keyRects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) {
                    buf[len++] = keys[i][0];
                    buf[len] = 0;
                    handled = true;
                }
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

            if (handled) redraw();
        }

        return confirmed ? String(buf) : String();
    }

    // Vier Tastaturreihen wie bei der Rufzeichen-Eingabe (siehe
    // aircraft_watchlist_screen.cpp::runCallsignKeypad), aber mit Leertaste
    // (Ortsnamen enthalten oft Leerzeichen, z.B. "Bei Oma") und OHNE
    // Eingabepflicht - der Name ist optional, "Ohne Namen" bestaetigt mit
    // leerem Puffer statt die Eingabe abzubrechen.
    String runPresetNameKeypad(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr const char* DIGITS = "1234567890";
        constexpr const char* ROW1 = "QWERTYUIOP";
        constexpr const char* ROW2 = "ASDFGHJKL";
        constexpr const char* ROW3 = "ZXCVBNM";

        char buf[17] = {0};
        uint8_t len = 0;

        constexpr int16_t KEY_H = 30;
        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t FIELD_H = 34;

        // Kopfbereich (Titel + Namens-Hinweis) einmal zeichnen, um seine
        // tatsaechliche Hoehe per getCursorY() zu messen - selbes Muster wie
        // in address_search_screen.cpp::runAddressKeyboard().
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::LOCATION_NAME_PROMPT));
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println(I18n::t(StringId::LOCATION_NAME_HINT));
        int16_t fieldY = (int16_t)(tft.getCursorY() + 4);
        int16_t ROW0_Y = (int16_t)(fieldY + FIELD_H + 8);

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
        layoutRow(DIGITS, ROW0_Y, digitRects, 10);
        layoutRow(ROW1, (int16_t)(ROW0_Y + (KEY_H + KEY_GAP)), row1Rects, 10);
        layoutRow(ROW2, (int16_t)(ROW0_Y + 2 * (KEY_H + KEY_GAP)), row2Rects, 9);
        layoutRow(ROW3, (int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), row3Rects, 7);

        Rect spaceBtn     = {4, (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), 150, KEY_H};
        Rect backspaceBtn = {158, (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8 - 154), KEY_H};
        Rect skipBtn      = {4, (int16_t)(ROW0_Y + 5 * (KEY_H + KEY_GAP)), 110, KEY_H};
        Rect confirmBtn   = {118, (int16_t)(ROW0_Y + 5 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8 - 114), KEY_H};

        bool done = false;
        bool confirmed = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::LOCATION_NAME_PROMPT));
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            tft.println(I18n::t(StringId::LOCATION_NAME_HINT));

            tft.fillRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, TFT_BLACK);
            tft.drawRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, TFT_GREEN);
            tft.setTextSize(2);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(14, (int16_t)(fieldY + 26));
            tft.print(buf);
            tft.setTextSize(1);

            for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(DIGITS[i]));
            for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(ROW1[i]));
            for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(ROW2[i]));
            for (uint8_t i = 0; i < 7; i++) drawButton(tft, row3Rects[i], String(ROW3[i]));

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
                if (digitRects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = DIGITS[i]; buf[len] = 0; handled = true; }
            }
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (row1Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = ROW1[i]; buf[len] = 0; handled = true; }
            }
            for (uint8_t i = 0; i < 9 && !handled; i++) {
                if (row2Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = ROW2[i]; buf[len] = 0; handled = true; }
            }
            for (uint8_t i = 0; i < 7 && !handled; i++) {
                if (row3Rects[i].contains(tap.x, tap.y) && len < sizeof(buf) - 1) { buf[len++] = ROW3[i]; buf[len] = 0; handled = true; }
            }
            if (!handled && spaceBtn.contains(tap.x, tap.y) && len < sizeof(buf) - 1) {
                buf[len++] = ' ';
                buf[len] = 0;
                handled = true;
            }
            if (!handled && backspaceBtn.contains(tap.x, tap.y)) {
                if (len > 0) { len--; buf[len] = 0; }
                handled = true;
            }
            if (!handled && skipBtn.contains(tap.x, tap.y)) {
                buf[0] = 0;
                len = 0;
                done = true;
                confirmed = true;
                handled = true;
            }
            if (!handled && confirmBtn.contains(tap.x, tap.y)) {
                done = true;
                confirmed = true;
                handled = true;
            }

            if (handled) redraw();
        }

        return confirmed ? String(buf) : String();
    }

    bool addPresetByCoordsFlow(TFT_eSPI& tft) {
        String latStr = runNumericKeypad(tft, I18n::t(StringId::LOCATION_LAT_PROMPT));
        if (latStr.length() == 0) return false;

        String lonStr = runNumericKeypad(tft, I18n::t(StringId::LOCATION_LON_PROMPT));
        if (lonStr.length() == 0) return false;

        double lat = latStr.toDouble();
        double lon = lonStr.toDouble();
        if (lat == 0.0 && lon == 0.0) return false;

        String name = runPresetNameKeypad(tft);
        if (!LocationPresets::addPreset(lat, lon, name)) return false;
        // Neu angelegtes Preset sofort aktivieren, gleiches Verhalten wie
        // beim Anlegen per Adresssuche (siehe address_search_screen.cpp) -
        // sonst blieb z.B. der automatische IP-Standort aktiv, obwohl gerade
        // extra ein neuer Standort eingegeben wurde.
        LocationPresets::setActiveIndex((int8_t)(LocationPresets::count() - 1));
        return true;
    }

    // Erster Schritt beim Antippen von "+": manuelle Koordinaten oder
    // Adresssuche (AddressSearchScreen, kuemmert sich dort bereits selbst
    // um Namensvergabe + Speichern als Preset).
    bool addPresetFlow(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::LOCATION_ADD_CHOICE_TITLE));

        Rect addressBtn = {10, 90, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
        Rect coordsBtn  = {10, 144, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
        Rect cancelBtn  = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, addressBtn, I18n::t(StringId::LOCATION_ADD_BY_ADDRESS));
        drawButton(tft, coordsBtn, I18n::t(StringId::LOCATION_ADD_BY_COORDS));
        drawButton(tft, cancelBtn, I18n::t(StringId::CANCEL), false, true);

        while (true) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) {
                // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) return false;
                MenuStars::update(tft);
                delay(20);
                continue;
            }

            if (addressBtn.contains(tap.x, tap.y)) return AddressSearchScreen::run(tft);
            if (coordsBtn.contains(tap.x, tap.y)) return addPresetByCoordsFlow(tft);
            if (cancelBtn.contains(tap.x, tap.y)) return false;
        }
    }

    // Kurze Meldung unten am Bildschirmrand, z.B. wenn beim Antippen des
    // Naechster-Flughafen-Textes bereits alle 3 Preset-Slots belegt sind.
    void showBriefMessage(TFT_eSPI& tft, const String& msg, uint16_t color) {
        tft.fillRect(0, Config::SCREEN_HEIGHT - 18, Config::SCREEN_WIDTH, 18, TFT_BLACK);
        tft.setTextColor(color, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(msg, Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT - 9);
        tft.setTextDatum(TL_DATUM);
        delay(1200);
    }

    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                          int16_t lineHeight, const String& text, int16_t scrollY,
                          int16_t viewTop, int16_t viewBottom, bool draw) {
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
                int16_t screenY = y - scrollY;
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

    // Laufschrift fuer den "Naechster Flughafen"-Text: statt ihn mit "..."
    // abzuschneiden, scrollt er horizontal durch, falls er nicht in die
    // verfuegbare Breite passt. BEWUSST OHNE tft.setViewport() (das hatte
    // im Zusammenspiel mit unserem eigenen Font dazu gefuehrt, dass der Text
    // komplett unsichtbar wurde) - stattdessen wird bei jedem Schritt einfach
    // der Text-Ausschnitt neu berechnet, der garantiert in die Breite passt.
    struct Marquee {
        String text;
        String ring;          // text + Luecke, doppelt aneinandergehaengt
        bool needsScroll = false;
        int32_t charOffset = 0;
        uint32_t lastStepMs = 0;
    };
    Marquee airportMarquee;

    // Etwas kuerzeres Intervall als frueher fuer einen ruhigeren Lauf.
    // WICHTIG: der Cursor wird beim Zeichnen IMMER exakt auf x gesetzt
    // (nie davor/danach verschoben) - ein Versuch mit pixelgenauer
    // Sub-Zeichen-Verschiebung (Cursor links von x) hat zu einem
    // fehlerhaft gezeichneten Zeichen ausserhalb des geloeschten Bereichs
    // gefuehrt (sichtbar als gruener Kasten links vom Text).
    constexpr uint32_t MARQUEE_STEP_MS = 200; // alle 200ms ein Zeichen weiter

    // Einmal aufrufen, wenn sich der anzuzeigende Text aendert (z.B. weil ein
    // anderer Standort aktiviert wurde) - merkt sich Text + Ringpuffer und
    // setzt den Scroll-Fortschritt zurueck.
    void setupMarquee(TFT_eSPI& tft, const String& text, int16_t viewportW) {
        // WICHTIG: textWidth() haengt von der aktuell gesetzten Textgroesse
        // ab. Der Marquee wird in Size 2 gezeichnet (siehe drawMarquee),
        // also muss hier ebenfalls Size 2 aktiv sein, sonst wird die
        // Breite falsch (zu schmal) gemessen und der Text ragt beim
        // Zeichnen ueber den verfuegbaren Platz hinaus.
        tft.setTextSize(2);
        airportMarquee.text = text;
        airportMarquee.needsScroll = tft.textWidth(text) > viewportW;
        tft.setTextSize(1);
        String withGap = text + "   "; // 3 Leerzeichen Luecke vor der Wiederholung
        airportMarquee.ring = withGap + withGap;
        airportMarquee.charOffset = 0;
        airportMarquee.lastStepMs = millis();
    }

    // Liefert den laengsten Ausschnitt ab startIdx, der noch in maxWidth
    // passt - OHNE "..." anzuhaengen (im Gegensatz zu truncateForWidth).
    String marqueeWindow(TFT_eSPI& tft, const String& src, int32_t startIdx, int16_t maxWidth) {
        String s = src.substring(startIdx);
        while (s.length() > 1 && tft.textWidth(s) > maxWidth) {
            s.remove(s.length() - 1);
        }
        return s;
    }

    // Bei jedem Aufruf (auch in der Warteschleife) neu zeichnen - wenn der
    // Text nicht scrollen muss, wird er einfach normal (fest) angezeigt.
    void drawMarquee(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h) {
        if (airportMarquee.text.length() == 0) return;

        // Der geloeschte Bereich muss zur tatsaechlichen Texthoehe bei
        // Size 2 passen (~16px), nicht zur alten Size-1-Hoehe - sonst
        // bleiben oben Reste vom vorherigen Frame stehen (sichtbar als
        // Strich ueber dem Text). Etwas grosszuegiger nach oben (start
        // bei y-20) als vorher, da ein einzelner Frame-Ausreisser sonst
        // noch sichtbar blieb.
        constexpr int16_t MARQUEE_CLEAR_TOP = 20;
        constexpr int16_t MARQUEE_CLEAR_H = 26;
        tft.fillRect(x, y - MARQUEE_CLEAR_TOP, w, MARQUEE_CLEAR_H, TFT_BLACK);
        tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
        // Groessere Schrift fuer den Nearest-Airport-Marquee (auf Wunsch
        // vergroessert von Size 1 auf Size 2) - nach dem Zeichnen wieder auf
        // Size 1 zurueckstellen, damit nachfolgender Code (z.B. der
        // "Zurueck"-Button) nicht versehentlich auch vergroessert wird.
        tft.setTextSize(2);
        tft.setCursor(x, y);

        if (!airportMarquee.needsScroll) {
            tft.print(airportMarquee.text);
            tft.setTextSize(1);
            return;
        }

        uint32_t now = millis();
        if (now - airportMarquee.lastStepMs >= MARQUEE_STEP_MS) {
            airportMarquee.lastStepMs = now;
            airportMarquee.charOffset++;
            // Zurueck an den Anfang, sobald der erste (nicht doppelte)
            // Text+Luecke-Block durchgelaufen ist.
            int32_t singleLen = (int32_t)airportMarquee.text.length() + 3;
            if (airportMarquee.charOffset >= singleLen) airportMarquee.charOffset = 0;
        }

        tft.print(marqueeWindow(tft, airportMarquee.ring, airportMarquee.charOffset, w));
        tft.setTextSize(1);
    }

    // Laufschrift-Zustand pro Preset-Zeile, analog zu airportMarquee oben,
    // aber als Array (eine Instanz je Zeile) und LINKSBUENDIG gezeichnet
    // statt zentriert - zentriertes Scrollen wuerde bei jedem Schritt
    // sichtbar hin- und herspringen, da sich die Textbreite laufend
    // aendert. Ersetzt die bisherige truncateForWidth()-Kuerzung mit "...".
    struct RowMarquee {
        String text;
        String ring;
        bool needsScroll = false;
        int32_t charOffset = 0;
        uint32_t lastStepMs = 0;
    };
    RowMarquee rowMarquees[LocationPresets::MAX_PRESETS];

    // Wie setupMarquee() oben, aber ohne Textgroessen-Umschaltung (die
    // Preset-Zeilen nutzen durchgehend Size 1).
    void setupRowMarquee(TFT_eSPI& tft, RowMarquee& m, const String& text, int16_t maxWidth) {
        m.text = text;
        m.needsScroll = tft.textWidth(text) > maxWidth;
        String withGap = text + "   ";
        m.ring = withGap + withGap;
        m.charOffset = 0;
        m.lastStepMs = millis();
    }

    // Zeichnet eine Preset-Zeile im selben Rahmen-/Fuellstil wie
    // drawButton(), aber mit linksbuendiger Laufschrift statt zentriertem,
    // ggf. abgeschnittenem Text. Wird sowohl beim ersten Bildschirmaufbau
    // als auch (nur fuer Zeilen mit needsScroll) bei jedem Tick in der
    // Warteschleife erneut aufgerufen, um den naechsten Scroll-Schritt zu
    // zeichnen.
    void drawRowMarquee(TFT_eSPI& tft, const Rect& r, RowMarquee& m, bool active) {
        uint16_t bg = active ? TFT_GREEN : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : TFT_GREEN;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);

        constexpr int16_t PAD = 8;
        int16_t maxWidth = (int16_t)(r.w - 2 * PAD);
        tft.setTextColor(fg, bg);
        tft.setTextDatum(ML_DATUM);
        int16_t textY = (int16_t)(r.y + r.h / 2);

        if (!m.needsScroll) {
            tft.drawString(m.text, (int16_t)(r.x + PAD), textY);
            tft.setTextDatum(TL_DATUM);
            return;
        }

        uint32_t now = millis();
        if (now - m.lastStepMs >= MARQUEE_STEP_MS) {
            m.lastStepMs = now;
            m.charOffset++;
            int32_t singleLen = (int32_t)m.text.length() + 3;
            if (m.charOffset >= singleLen) m.charOffset = 0;
        }
        tft.drawString(marqueeWindow(tft, m.ring, m.charOffset, maxWidth), (int16_t)(r.x + PAD), textY);
        tft.setTextDatum(TL_DATUM);
    }

    void runInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();

        constexpr int16_t textMaxWidth = Config::SCREEN_WIDTH - 20;
        constexpr int16_t LINE_H = 16;
        constexpr int16_t VIEW_TOP = 36;
        constexpr int16_t VIEW_BOTTOM = Config::SCREEN_HEIGHT - 60;

        int16_t totalH = VIEW_TOP;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA1), 0, 0, 0, false);
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA2), 0, 0, 0, false);
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA3), 0, 0, 0, false);
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA4), 0, 0, 0, false);
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA5), 0, 0, 0, false);
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA6), 0, 0, 0, false);

        int16_t maxScroll = totalH - VIEW_BOTTOM;
        if (maxScroll < 0) maxScroll = 0;
        bool scrollable = maxScroll > 0;
        int16_t scrollY = 0;

        Rect backBtn = scrollable
            ? Rect{10, (int16_t)(Config::SCREEN_HEIGHT - 50), 130, 40}
            : Rect{10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect upBtn   = {146, (int16_t)(Config::SCREEN_HEIGHT - 50), 38, 40};
        Rect downBtn = {190, (int16_t)(Config::SCREEN_HEIGHT - 50), 38, 40};
        constexpr int16_t SCROLL_STEP = 48;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::LOCATION_INFO_TITLE));

            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            int16_t y = VIEW_TOP;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA1), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA2), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA3), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA4), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA5), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA6), scrollY, VIEW_TOP, VIEW_BOTTOM, true);

            drawButton(tft, backBtn, I18n::t(StringId::BACK));
            if (scrollable) {
                drawButton(tft, upBtn, "^");
                drawButton(tft, downBtn, "v");
            }
        };

        redraw();

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (backBtn.contains(tap.x, tap.y)) return;
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
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) return;
            MenuStars::update(tft);
            delay(20);
        }
    }
}

void run(TFT_eSPI& tft) {
    constexpr int16_t ROW_H = 32;
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t REMOVE_BTN_W = 60;

    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::LOCATION_TITLE));

        Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
        drawButton(tft, infoBtn, "?");

        int8_t active = LocationPresets::activeIndex();
        uint8_t count = LocationPresets::count();
        int16_t y = 36;

        Rect autoRect = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
        String autoLabel = I18n::t(StringId::LOCATION_AUTO);
        drawButton(tft, autoRect, active < 0 ? ("> " + autoLabel) : autoLabel, active < 0);
        y += ROW_H + ROW_GAP;

        Rect rowRects[LocationPresets::MAX_PRESETS];
        Rect removeRects[LocationPresets::MAX_PRESETS];
        String presetWord = I18n::t(StringId::LOCATION_PRESET);

        for (uint8_t i = 0; i < LocationPresets::MAX_PRESETS; i++) {
            Rect rowRect = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20 - REMOVE_BTN_W - 6), ROW_H};
            Rect removeRect = {(int16_t)(Config::SCREEN_WIDTH - 10 - REMOVE_BTN_W), y, REMOVE_BTN_W, ROW_H};
            rowRects[i] = rowRect;
            removeRects[i] = removeRect;

            if (i < count) {
                double lat = 0, lon = 0;
                LocationPresets::getLatLon(i, lat, lon);
                String presetName = LocationPresets::getName(i);
                String label;
                if (presetName.length() > 0) {
                    // Benannte Presets (manuell vergeben oder vom
                    // Flughafen-Antippen uebernommen) zeigen den Namen statt
                    // der Koordinaten - besser lesbar in der Uebersicht.
                    label = (active == (int8_t)i ? "> " : "") + presetName;
                } else {
                    char coords[24];
                    snprintf(coords, sizeof(coords), "%d: %.2f, %.2f", i + 1, lat, lon);
                    label = (active == (int8_t)i ? "> " : "") + presetWord + " " + coords;
                }
                // Laufschrift statt "..."-Kuerzung fuer lange Preset-Namen
                // bzw. Koordinaten, die nicht in die Zeilenbreite passen
                // (siehe RowMarquee oben) - kurze Labels bleiben unveraendert
                // zentriert wie zuvor.
                if (tft.textWidth(label) <= rowRect.w - 10) {
                    rowMarquees[i].needsScroll = false;
                    drawButton(tft, rowRect, label, active == (int8_t)i);
                } else {
                    setupRowMarquee(tft, rowMarquees[i], label, (int16_t)(rowRect.w - 10));
                    drawRowMarquee(tft, rowRect, rowMarquees[i], active == (int8_t)i);
                }
                drawButton(tft, removeRect, "X", false, true);
            } else {
                tft.fillRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_BLACK);
                tft.drawRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_DARKGREEN);
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
                String label = presetWord + " " + String(i + 1) + " " + I18n::t(StringId::LOCATION_PRESET_EMPTY);
                tft.drawString(label, rowRect.x + rowRect.w / 2, rowRect.y + rowRect.h / 2);
                tft.setTextDatum(TL_DATUM);
            }
            y += ROW_H + ROW_GAP;
        }

        Rect addBtn = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), 36};
        bool canAdd = count < LocationPresets::MAX_PRESETS;
        if (canAdd) {
            drawButton(tft, addBtn, I18n::t(StringId::LOCATION_ADD));
        }

        constexpr int16_t AIRPORT_LINE_X = 10;
        constexpr int16_t AIRPORT_LINE_W = Config::SCREEN_WIDTH - 20;
        int16_t airportLineY = (int16_t)(Config::SCREEN_HEIGHT - 60);
        // Deckt denselben Bereich ab, den drawMarquee() bei jedem Frame
        // loescht (siehe MARQUEE_CLEAR_TOP/-H) - dient sowohl als Tap-Zone
        // als auch als sichtbarer Rahmen, der anzeigt, dass man hier
        // antippen kann, um den Flughafen als neuen Preset zu uebernehmen.
        Rect airportRect = {(int16_t)(AIRPORT_LINE_X - 4), (int16_t)(airportLineY - 20),
                             (int16_t)(AIRPORT_LINE_W + 8), 26};
        AirportLookup::Nearest nearest;
        {
            double activeLat = 0, activeLon = 0;
            if (active < 0) {
                LocationManager::getHomeLocation(activeLat, activeLon);
            } else {
                LocationPresets::getLatLon((uint8_t)active, activeLat, activeLon);
            }
            nearest = AirportLookup::findNearest(activeLat, activeLon);
            if (nearest.found) {
                // Respektiert jetzt die Einheiten-Einstellung (Menue >
                // Einheiten) - vorher immer "(XX km)", auch bei Imperial.
                char buf[48];
                if (LocationManager::useMetricUnits()) {
                    snprintf(buf, sizeof(buf), "%s %s (%.0f km)", nearest.icao, nearest.name, nearest.distanceKm);
                } else {
                    snprintf(buf, sizeof(buf), "%s %s (%.0f nm)", nearest.icao, nearest.name, Units::kmToNm(nearest.distanceKm));
                }
                String line = String(I18n::t(StringId::LOCATION_NEAREST_AIRPORT_PREFIX)) + buf;
                setupMarquee(tft, line, AIRPORT_LINE_W);
                drawMarquee(tft, AIRPORT_LINE_X, airportLineY, AIRPORT_LINE_W, 20);
                tft.drawRoundRect(airportRect.x, airportRect.y, airportRect.w, airportRect.h, 4, TFT_DARKGREEN);
            } else {
                airportMarquee.text = "";
            }
        }

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
            MenuStars::update(tft);
            drawMarquee(tft, AIRPORT_LINE_X, airportLineY, AIRPORT_LINE_W, 20);
            for (uint8_t i = 0; i < count; i++) {
                if (rowMarquees[i].needsScroll) {
                    drawRowMarquee(tft, rowRects[i], rowMarquees[i], active == (int8_t)i);
                }
            }
            delay(20);
        }

        bool handled = false;
        if (infoBtn.contains(tap.x, tap.y)) {
            runInfoScreen(tft);
            handled = true;
        } else if (autoRect.contains(tap.x, tap.y)) {
            LocationPresets::setActiveIndex(-1);
            handled = true;
        }
        for (uint8_t i = 0; i < count && !handled; i++) {
            if (removeRects[i].contains(tap.x, tap.y)) {
                LocationPresets::removePreset(i);
                handled = true;
            } else if (rowRects[i].contains(tap.x, tap.y)) {
                LocationPresets::setActiveIndex((int8_t)i);
                handled = true;
            }
        }
        if (!handled && canAdd && addBtn.contains(tap.x, tap.y)) {
            addPresetFlow(tft);
            handled = true;
        }
        if (!handled && nearest.found && airportRect.contains(tap.x, tap.y)) {
            if (canAdd) {
                if (LocationPresets::addPreset(nearest.lat, nearest.lon, nearest.name)) {
                    // Gleiches Verhalten wie bei den anderen beiden Wegen,
                    // ein Preset anzulegen - sofort aktivieren.
                    LocationPresets::setActiveIndex((int8_t)(LocationPresets::count() - 1));
                }
            } else {
                showBriefMessage(tft, I18n::t(StringId::LOCATION_PRESETS_FULL), TFT_RED);
            }
            handled = true;
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
