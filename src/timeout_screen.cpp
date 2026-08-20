#include "timeout_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace TimeoutScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    String onOff(bool on) { return I18n::t(on ? StringId::ON : StringId::OFF); }

    // Einfacher Zeilenumbruch-Helfer, gleiches Prinzip wie in
    // menu_screen.cpp (dort mit zusaetzlicher Scroll-Unterstuetzung, hier
    // bewusst ohne - der Beschreibungstext ist kurz genug, dass Scrollen
    // nicht noetig ist). draw=false liefert nur die Gesamthoehe, ohne etwas
    // zu zeichnen (fuer die vorab-Platzberechnung des Zurueck-Buttons).
    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                           int16_t lineHeight, const String& text, bool draw) {
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
                tft.setCursor(x, y);
                tft.print(line);
            }
            y += lineHeight;
            start += line.length();
        }
        return (int16_t)(y - startY);
    }

    // Schieberegler-Positionen: 1..Config::SCREEN_TIMEOUT_MAX_MINUTES
    // Minuten (je eine Position pro Minute) plus eine zusaetzliche
    // Endposition ganz rechts fuer "Nie" (kein Timeout) - intern weiterhin
    // als 0 gespeichert, wie schon vor diesem Screen.
    constexpr uint8_t STEP_COUNT = Config::SCREEN_TIMEOUT_MAX_MINUTES + 1;

    uint8_t indexFromMinutes(uint8_t minutes) {
        if (minutes == 0 || minutes > Config::SCREEN_TIMEOUT_MAX_MINUTES) return STEP_COUNT - 1;
        return minutes - 1;
    }

    uint8_t minutesFromIndex(uint8_t index) {
        if (index >= STEP_COUNT - 1) return 0;
        return index + 1;
    }

    String valueLabel(uint8_t minutes) {
        if (minutes == 0) return I18n::t(StringId::NEVER);
        return String(minutes) + " min";
    }
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();

    uint8_t index = indexFromMinutes(SettingsStore::screenTimeoutMinutes());
    bool dragging = false;

    constexpr int16_t TRACK_X = 20;
    constexpr int16_t TRACK_Y = 95;
    constexpr int16_t TRACK_W = Config::SCREEN_WIDTH - 2 * TRACK_X;
    constexpr int16_t TRACK_H = 6;
    constexpr int16_t THUMB_R = 12;
    // Grosszuegige vertikale Trefferzone rund um den duennen Regler-Strich -
    // sonst muesste man beim Ziehen extrem praezise genau auf die paar
    // Pixel der Linie treffen.
    constexpr int16_t TRACK_HIT_Y_MIN = TRACK_Y - 24;
    constexpr int16_t TRACK_HIT_Y_MAX = TRACK_Y + 24;

    Rect screensaverBtn = {10, 135, (int16_t)(Config::SCREEN_WIDTH - 20), 36};

    // Beschreibungstext-Hoehe (haengt von der jeweils eingestellten Sprache
    // ab) einmalig vorab messen, damit der Zurueck-Button IMMER direkt
    // darunter landet - nie mit dem Text ueberlappend und nie unnoetig viel
    // Leerraum lassend. Gleiches Prinzip wie bei confirmWarningScreen()/
    // infoScreen() in menu_screen.cpp.
    constexpr int16_t DESC_Y = 185;
    constexpr int16_t DESC_LINE_H = 14;
    constexpr int16_t DESC_MAX_WIDTH = Config::SCREEN_WIDTH - 20;
    int16_t descH = layoutWrapped(tft, 10, DESC_Y, DESC_MAX_WIDTH, DESC_LINE_H,
                                   I18n::t(StringId::TIMEOUT_SCREENSAVER_DESC), false);
    int16_t backY = (int16_t)(DESC_Y + descH + 10);
    // Sicherheitsnetz, falls eine Uebersetzung doch mal laenger ausfaellt,
    // als der verfuegbare Platz hergibt - der Zurueck-Button bleibt so
    // IMMER erreichbar, statt vom Bildschirmrand abgeschnitten zu werden.
    constexpr int16_t BACK_Y_MAX = Config::SCREEN_HEIGHT - 46;
    if (backY > BACK_Y_MAX) backY = BACK_Y_MAX;
    Rect backBtn = {10, backY, (int16_t)(Config::SCREEN_WIDTH - 20), 36};

    auto redraw = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::TIMEOUT_SCREEN_TITLE));

        uint8_t minutes = minutesFromIndex(index);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(3);
        tft.drawString(valueLabel(minutes), Config::SCREEN_WIDTH / 2, 55);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);

        int16_t thumbX = (int16_t)(TRACK_X + (int32_t)index * TRACK_W / (STEP_COUNT - 1));
        tft.fillRoundRect(TRACK_X, (int16_t)(TRACK_Y - TRACK_H / 2), TRACK_W, TRACK_H,
                           (int16_t)(TRACK_H / 2), TFT_DARKGREY);
        if (thumbX > TRACK_X) {
            tft.fillRoundRect(TRACK_X, (int16_t)(TRACK_Y - TRACK_H / 2), (int16_t)(thumbX - TRACK_X),
                               TRACK_H, (int16_t)(TRACK_H / 2), TFT_GREEN);
        }
        tft.fillCircle(thumbX, TRACK_Y, THUMB_R, TFT_GREEN);
        tft.drawCircle(thumbX, TRACK_Y, THUMB_R, TFT_BLACK);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(String(Config::SCREEN_TIMEOUT_MIN_MINUTES) + " min", TRACK_X,
                        (int16_t)(TRACK_Y + THUMB_R + 6));
        tft.setTextDatum(TR_DATUM);
        tft.drawString(I18n::t(StringId::NEVER), (int16_t)(TRACK_X + TRACK_W),
                        (int16_t)(TRACK_Y + THUMB_R + 6));
        tft.setTextDatum(TL_DATUM);

        drawButton(tft, screensaverBtn,
                   String(I18n::t(StringId::MENU_SCREENSAVER)) + onOff(SettingsStore::screensaverEnabled()));

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        layoutWrapped(tft, 10, DESC_Y, DESC_MAX_WIDTH, DESC_LINE_H,
                      I18n::t(StringId::TIMEOUT_SCREENSAVER_DESC), true);

        drawButton(tft, backBtn, I18n::t(StringId::BACK));
    };

    redraw();

    while (!done) {
        TouchInput::Point live = TouchInput::mappedPoint();
        if (live.touched && live.y >= TRACK_HIT_Y_MIN && live.y <= TRACK_HIT_Y_MAX) {
            int16_t clampedX = live.x;
            if (clampedX < TRACK_X) clampedX = TRACK_X;
            if (clampedX > TRACK_X + TRACK_W) clampedX = (int16_t)(TRACK_X + TRACK_W);
            uint8_t newIndex = (uint8_t)(((int32_t)(clampedX - TRACK_X) * (STEP_COUNT - 1) + TRACK_W / 2) / TRACK_W);
            if (newIndex != index) {
                index = newIndex;
                redraw();
            }
            dragging = true;
        } else if (dragging && !live.touched) {
            // Loslassen: jetzt erst dauerhaft speichern (nicht bei jeder
            // Zwischenposition waehrend des Ziehens selbst, um unnoetig
            // viele SD-Kartenschreibvorgaenge zu vermeiden).
            SettingsStore::setScreenTimeoutMinutes(minutesFromIndex(index));
            dragging = false;
        }

        // wasTapped() IMMER aufrufen (auch waehrend des Ziehens), damit ihr
        // interner Loslassen-Erkennungszustand synchron bleibt - nur die
        // AUSWERTUNG des Ergebnisses wird waehrend des Ziehens unterdrueckt.
        // Wuerde man den Aufruf selbst waehrend des Ziehens auslassen,
        // koennte beim Loslassen faelschlich ein "Tap" mit einer veralteten
        // Position (von VOR Beginn des Ziehens) gemeldet werden.
        TouchInput::Point tap;
        bool tapped = TouchInput::wasTapped(tap);
        if (!dragging && tapped) {
            if (screensaverBtn.contains(tap.x, tap.y)) {
                SettingsStore::setScreensaverEnabled(!SettingsStore::screensaverEnabled());
                redraw();
            } else if (backBtn.contains(tap.x, tap.y)) {
                done = true;
            }
        }

        // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS. Nicht
        // waehrend aktivem Ziehen des Schiebereglers ausloesen (dragging) -
        // dabei bleibt lastTapMs unveraendert (wasTapped() feuert erst beim
        // Loslassen), ein laengerer Ziehvorgang darf aber nicht als
        // Inaktivitaet gewertet werden.
        if (!dragging && TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) done = true;

        MenuStars::update(tft);
        delay(20);
    }
}

}
