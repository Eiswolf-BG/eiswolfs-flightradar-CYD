#include "splash_screen.h"
#include "menu_stars.h"
#include "radar_logo.h"
#include "i18n.h"
#include <math.h>
#include "ui_theme.h"

namespace SplashScreen {

namespace {
    constexpr uint32_t MIN_DISPLAY_MS = 5000;
    uint32_t startMs = 0;

    constexpr int16_t STATUS_LINE_H = 18;
    constexpr int16_t STATUS_START_Y = 260;
    constexpr uint8_t MAX_STATUS_LINES = 3;

    // Reiner Flavour-Text (kein echter Statuswert wie bei den SPLASH_*-
    // Strings oben) - Reihenfolge muss zur Reihenfolge von BOOT_SEQ_1..6 in
    // i18n.h passen.
    constexpr StringId BOOT_LINES[] = {
        StringId::BOOT_SEQ_1, StringId::BOOT_SEQ_2, StringId::BOOT_SEQ_3,
        StringId::BOOT_SEQ_4, StringId::BOOT_SEQ_5, StringId::BOOT_SEQ_6,
    };
    constexpr uint8_t BOOT_LINE_COUNT = sizeof(BOOT_LINES) / sizeof(BOOT_LINES[0]);
    // Timing bewusst so gewaehlt, dass jede Zeile einzeln lesbar
    // "reinklappt" statt alle sofort dazustehen, aber die Gesamtsequenz
    // trotzdem klar unter 5s bleibt (Titel-Pause + 6 Zeilen a 550ms +
    // Schlusspause = ca. 4.1s).
    constexpr uint32_t BOOT_TITLE_PAUSE_MS = 400;
    constexpr uint32_t BOOT_LINE_DELAY_MS = 550;
    constexpr uint32_t BOOT_FINAL_PAUSE_MS = 400;
    constexpr int16_t BOOT_TITLE_Y = 34;
    constexpr int16_t BOOT_START_X = 14;
    constexpr int16_t BOOT_RIGHT_MARGIN = 10;
    constexpr int16_t BOOT_START_Y = 150;
    constexpr int16_t BOOT_LINE_H = 22;

    // Lokale Kopie des layoutWrapped()-Musters (siehe location_presets_screen.cpp/
    // wifi_manage_screen.cpp, dort bewusst dupliziert statt geteilt) - bricht
    // EINE Boot-Zeile wortweise um, falls sie nicht in die Bildschirmbreite
    // passt, statt sie wie bisher per drawString() einfach ueber den rechten
    // Rand hinauslaufen und abschneiden zu lassen.
    int16_t drawWrappedBootLine(TFT_eSPI& tft, const String& text, int16_t startY) {
        int16_t maxWidth = tft.width() - BOOT_START_X - BOOT_RIGHT_MARGIN;
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

            tft.setCursor(BOOT_START_X, y);
            tft.print(line);
            y += BOOT_LINE_H;
            start += line.length();
        }
        return y;
    }
}

void playBootSequence(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);

    // Grosser, zentrierter Titel oben - macht den Screen als eigenstaendigen
    // Ladebildschirm erkennbar, statt "nackt" nur mit den Statuszeilen.
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString(I18n::t(StringId::BOOT_TITLE), tft.width() / 2, BOOT_TITLE_Y);
    delay(BOOT_TITLE_PAUSE_MS);

    // Boot-Zeilen bauen sich nacheinander auf ("Split-Flap"-Optik ohne
    // echte Hoch-/Runterschiebe-Animation, siehe Bahnhofstafel-Vorbild) -
    // jede Zeile wird einzeln gezeichnet und bekommt danach eine spuerbare
    // Pause, bevor die naechste erscheint.
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setTextSize(1);

    int16_t y = BOOT_START_Y;
    for (uint8_t i = 0; i < BOOT_LINE_COUNT; i++) {
        String line = String("> ") + I18n::t(BOOT_LINES[i]);
        y = drawWrappedBootLine(tft, line, y);
        delay(BOOT_LINE_DELAY_MS);
    }
    delay(BOOT_FINAL_PAUSE_MS);
}

void begin(TFT_eSPI& tft) {
    startMs = millis();
    MenuStars::reset();

    int16_t cx = tft.width() / 2;

    tft.fillScreen(TFT_BLACK);
    RadarLogo::draw(tft, cx, 174);

    tft.setTextDatum(MC_DATUM);

    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Eiswolfs", cx, 28);
    tft.drawString("Flightradar", cx, 60);

    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
}

void setStatusLine(TFT_eSPI& tft, uint8_t slot, const String& text, uint16_t color) {
    if (slot >= MAX_STATUS_LINES) return;

    int16_t y = STATUS_START_Y + slot * STATUS_LINE_H;
    tft.fillRect(0, y, tft.width(), STATUS_LINE_H, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(text, tft.width() / 2, y + STATUS_LINE_H / 2);
    tft.setTextDatum(TL_DATUM);
}

void waitRemaining(TFT_eSPI& tft) {
    uint32_t elapsed = millis() - startMs;
    while (elapsed < MIN_DISPLAY_MS) {
        MenuStars::update(tft);
        delay(20);
        elapsed = millis() - startMs;
    }
}

}