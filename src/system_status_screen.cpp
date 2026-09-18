#include "system_status_screen.h"
#include "aircraft_table.h"
#include "menu_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "settings_store.h"
#include "perf_tuner.h"
#include "config.h"
#include "i18n.h"
#include "ui_theme.h"
#include <WiFi.h>

namespace SystemStatusScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Wortweiser Zeilenumbruch anhand echter Pixelbreite (CLAUDE.md-Pflicht
    // fuer Text variabler Laenge) - dupliziert statt geteilt, gleiches
    // Muster wie in mehreren anderen Screens (z.B. airline_filter_screen.cpp).
    // Die vier Zeilen hier sind zwar in der Praxis kurz genug, um immer in
    // eine Zeile zu passen, aber layoutWrapped() faengt trotzdem ab, falls
    // eine Uebersetzung + der jeweilige Messwert in Kombination doch mal
    // zu breit wird.
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
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();

    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::SYSTEM_STATUS_TITLE));

        Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
        drawButton(tft, infoBtn, "?");

        constexpr int16_t LINE_X = 10;
        constexpr int16_t LINE_H = 24;
        constexpr int16_t LINE_MAX_W = Config::SCREEN_WIDTH - 20;
        int16_t y = 44;

        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);

        // WLAN-Signalstaerke - nur aussagekraeftig, solange tatsaechlich
        // verbunden (sonst liefert WiFi.RSSI() einen bedeutungslosen
        // Altwert bzw. 0).
        String wifiValue;
        if (WiFi.status() == WL_CONNECTED) {
            wifiValue = String(WiFi.RSSI()) + " dBm";
        } else {
            wifiValue = I18n::t(StringId::CONNECTION_STATUS_RESULT_NONE);
        }
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, LINE_H,
                           String(I18n::t(StringId::SYSTEM_STATUS_WIFI_PREFIX)) + wifiValue);

        // Freier Heap UND groesster zusammenhaengender freier Block - beide
        // in KB (eine Nachkommastelle), da die Rohwerte in Bytes bei einem
        // 320KB-RAM-Chip sonst unhandlich viele Ziffern haetten. Bewusst
        // beide Werte nebeneinander auf dem Screen (nicht nur der Heap-
        // Gesamtwert) - siehe Kommentar in i18n.h zu SYSTEM_STATUS_MAX_ALLOC_PREFIX:
        // ein grosser freier Heap kann trotzdem stark fragmentiert sein,
        // was der einfache Gesamtwert allein nicht zeigt.
        float freeHeapKb = ESP.getFreeHeap() / 1024.0f;
        char freeHeapBuf[16];
        snprintf(freeHeapBuf, sizeof(freeHeapBuf), "%.1f KB", freeHeapKb);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, LINE_H,
                           String(I18n::t(StringId::SYSTEM_STATUS_FREE_HEAP_PREFIX)) + freeHeapBuf);

        float maxAllocKb = ESP.getMaxAllocHeap() / 1024.0f;
        char maxAllocBuf[16];
        snprintf(maxAllocBuf, sizeof(maxAllocBuf), "%.1f KB", maxAllocKb);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, LINE_H,
                           String(I18n::t(StringId::SYSTEM_STATUS_MAX_ALLOC_PREFIX)) + maxAllocBuf);

        // Dauer des letzten ADS-B-Abrufversuchs (net_task.cpp, siehe
        // AircraftTable::recordFetchOutcome()) - unabhaengig davon, ob er
        // erfolgreich war, damit z.B. ein lang haengender, am Ende doch
        // erfolgreicher Versuch sichtbar wird.
        AircraftTable::FetchOutcome outcome = AircraftTable::lastFetchOutcome();
        String durationValue = outcome.hasResult
            ? (String(outcome.durationMs) + " ms")
            : String(I18n::t(StringId::CONNECTION_STATUS_RESULT_NONE));
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, LINE_H,
                           String(I18n::t(StringId::SYSTEM_STATUS_FETCH_DURATION_PREFIX)) + durationValue);

        // Aktuelle PerfTuner-Stufe (perf_tuner.h, Alex' Wunsch: unauffaelliger
        // Hinweis hier statt eines eigenen Panels) - nur bei Stufe > 0 die
        // Stufenzahl einblenden, sonst schlicht "Normal".
        uint8_t perfLevel = PerfTuner::currentLevel();
        String perfValue;
        if (perfLevel == 0) {
            perfValue = I18n::t(StringId::SYSTEM_STATUS_PERF_NORMAL);
        } else {
            char perfBuf[24];
            snprintf(perfBuf, sizeof(perfBuf), I18n::t(StringId::SYSTEM_STATUS_PERF_REDUCED), perfLevel);
            perfValue = perfBuf;
        }
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, LINE_H,
                           String(I18n::t(StringId::SYSTEM_STATUS_PERF_PREFIX)) + perfValue);

        Rect backBtn = {LINE_X, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        // Live-Aktualisierung alle ~1s, auch ohne Antippen (Alex' Wunsch) -
        // gleiches Grundprinzip wie connection_status_screen.cpp.
        TouchInput::Point tap;
        bool tapped = false;
        uint32_t waitStartMs = millis();
        while (true) {
            if (TouchInput::wasTapped(tap)) { tapped = true; break; }
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
            if (millis() - waitStartMs >= 1000) break;
            MenuStars::update(tft);
            delay(20);
        }
        if (done) break;
        if (!tapped) continue;

        if (infoBtn.contains(tap.x, tap.y)) {
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::SYSTEM_STATUS_INFO_TITLE),
                                        I18n::t(StringId::SYSTEM_STATUS_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
