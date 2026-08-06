#include "location_presets_screen.h"
#include "location_presets.h"
#include "location_manager.h"
#include "airport_lookup.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

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

    bool addPresetFlow(TFT_eSPI& tft) {
        String latStr = runNumericKeypad(tft, I18n::t(StringId::LOCATION_LAT_PROMPT));
        if (latStr.length() == 0) return false;

        String lonStr = runNumericKeypad(tft, I18n::t(StringId::LOCATION_LON_PROMPT));
        if (lonStr.length() == 0) return false;

        double lat = latStr.toDouble();
        double lon = lonStr.toDouble();
        if (lat == 0.0 && lon == 0.0) return false;

        return LocationPresets::addPreset(lat, lon);
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

    String truncateForWidth(TFT_eSPI& tft, const String& text, int16_t maxWidth) {
        String s = text;
        if (tft.textWidth(s) <= maxWidth) return s;
        while (s.length() > 1 && tft.textWidth(s + "...") > maxWidth) {
            s.remove(s.length() - 1);
        }
        return s + "...";
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
        String withGap = text + "     "; // 5 Leerzeichen Luecke vor der Wiederholung
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

        tft.fillRect(x, y - h + 4, w, h, TFT_BLACK);
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
            int32_t singleLen = (int32_t)airportMarquee.text.length() + 5;
            if (airportMarquee.charOffset >= singleLen) airportMarquee.charOffset = 0;
        }

        tft.print(marqueeWindow(tft, airportMarquee.ring, airportMarquee.charOffset, w));
        tft.setTextSize(1);
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
            layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(StringId::LOCATION_INFO_PARA4), scrollY, VIEW_TOP, VIEW_BOTTOM, true);

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
                char coords[24];
                snprintf(coords, sizeof(coords), "%d: %.2f, %.2f", i + 1, lat, lon);
                String label = (active == (int8_t)i ? "> " : "") + presetWord + " " + coords;
                label = truncateForWidth(tft, label, (int16_t)(rowRect.w - 10));
                drawButton(tft, rowRect, label, active == (int8_t)i);
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
        {
            double activeLat = 0, activeLon = 0;
            if (active < 0) {
                LocationManager::getHomeLocation(activeLat, activeLon);
            } else {
                LocationPresets::getLatLon((uint8_t)active, activeLat, activeLon);
            }
            AirportLookup::Nearest nearest = AirportLookup::findNearest(activeLat, activeLon);
            if (nearest.found) {
                char buf[48];
                snprintf(buf, sizeof(buf), "%s %s (%.0f km)", nearest.icao, nearest.name, nearest.distanceKm);
                String line = String(I18n::t(StringId::LOCATION_NEAREST_AIRPORT_PREFIX)) + buf;
                setupMarquee(tft, line, AIRPORT_LINE_W);
                drawMarquee(tft, AIRPORT_LINE_X, airportLineY, AIRPORT_LINE_W, 20);
            } else {
                airportMarquee.text = "";
            }
        }

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            drawMarquee(tft, AIRPORT_LINE_X, airportLineY, AIRPORT_LINE_W, 20);
            MenuStars::update(tft);
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
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}