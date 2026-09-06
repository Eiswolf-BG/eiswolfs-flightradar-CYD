#include "connection_status_screen.h"
#include "aircraft_table.h"
#include "config.h"
#include "i18n.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "ui_theme.h"

namespace ConnectionStatusScreen {

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

    // Gelb-Schwelle ist eine neue, eigene Konstante dieses Features (bisher
    // gab es nur die Rot-/Offline-Schwelle) - Rot wird bewusst NICHT neu
    // erfunden, sondern 1:1 Config::STALE_DATA_OFFLINE_THRESHOLD_MS
    // uebernommen (dieselbe Schwelle, die auch den Offline-Modus auf dem
    // Radarschirm selbst ausloest, siehe radar_screen.cpp).
    constexpr uint32_t YELLOW_THRESHOLD_MS = 10000;

    enum class Quality : uint8_t { Good, Fair, Poor };

    Quality qualityForAge(uint32_t ageMs) {
        if (ageMs > Config::STALE_DATA_OFFLINE_THRESHOLD_MS) return Quality::Poor;
        if (ageMs > YELLOW_THRESHOLD_MS) return Quality::Fair;
        return Quality::Good;
    }

    uint16_t colorForQuality(Quality q) {
        switch (q) {
            case Quality::Good: return TFT_GREEN;
            case Quality::Fair: return TFT_YELLOW;
            default:            return TFT_RED;
        }
    }

    const char* labelForQuality(Quality q) {
        switch (q) {
            case Quality::Good: return I18n::t(StringId::CONNECTION_STATUS_QUALITY_GOOD);
            case Quality::Fair: return I18n::t(StringId::CONNECTION_STATUS_QUALITY_FAIR);
            default:            return I18n::t(StringId::CONNECTION_STATUS_QUALITY_POOR);
        }
    }

    // s/min/h statt unbegrenzt wachsender Sekundenzahl - gleiches Prinzip
    // wie RADAR_EMPTY_SKY_PREFIX in radar_screen.cpp (dort schaltet die
    // "leerer Himmel seit"-Anzeige ab 60s ebenfalls auf "Xmin" um), verhindert
    // zusaetzlich, dass die Zeile bei einer sehr lange andauernden
    // Verbindungsunterbrechung durch eine immer laenger werdende Zahl die
    // Bildschirmbreite sprengt.
    String formatAge(uint32_t ms) {
        uint32_t sec = ms / 1000;
        if (sec < 60) return String(sec) + "s";
        uint32_t min = sec / 60;
        if (min < 60) return String(min) + "min";
        return String(min / 60) + "h";
    }

    // Zeichnet "prefix+value" in einer Zeile, WENN das bei der aktuellen
    // Sprache/Uebersetzung in maxW passt (siehe CLAUDE.md-Pflichtpruefung
    // Textbreite) - sonst faellt die Zeile auf zwei Zeilen zurueck (Praefix
    // oben, Wert darunter), statt per eingebautem Auto-Wrap unkontrolliert
    // mitten im Wort umzubrechen. Liefert die tatsaechlich verbrauchte
    // Zeilenhoehe zurueck, damit der Aufrufer die naechste Zeile korrekt
    // darunter platzieren kann.
    int16_t drawPrefixedLine(TFT_eSPI& tft, int16_t x, int16_t y, int16_t maxW, int16_t lineH,
                              const String& prefix, const String& value) {
        String combined = prefix + value;
        if (tft.textWidth(combined) <= maxW) {
            tft.setCursor(x, y);
            tft.println(combined);
            return lineH;
        }
        tft.setCursor(x, y);
        tft.println(prefix);
        tft.setCursor(x, (int16_t)(y + lineH));
        tft.println(value);
        return (int16_t)(lineH * 2);
    }
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();

    while (!done) {
        AircraftTable::lock();
        uint8_t tracked = AircraftTable::validCount();
        AircraftTable::unlock();

        uint32_t ageMs = AircraftTable::msSinceLastSuccessfulFetch(millis());
        AircraftTable::FetchOutcome outcome = AircraftTable::lastFetchOutcome();
        Quality quality = qualityForAge(ageMs);

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::MENU_CONNECTION_STATUS));

        constexpr int16_t LINE_X = 10;
        constexpr int16_t LINE_H = 24;
        constexpr int16_t LINE_MAX_W = Config::SCREEN_WIDTH - 20;
        int16_t y = 40;

        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        y += drawPrefixedLine(tft, LINE_X, y, LINE_MAX_W, LINE_H,
                               I18n::t(StringId::CONNECTION_STATUS_TRACKED_PREFIX), String(tracked));

        y += drawPrefixedLine(tft, LINE_X, y, LINE_MAX_W, LINE_H,
                               I18n::t(StringId::CONNECTION_STATUS_LAST_FETCH_PREFIX), formatAge(ageMs));

        // Ergebnis des letzten Abrufversuchs (siehe AircraftTable::
        // recordFetchOutcome() in net_task.cpp) - unabhaengig vom
        // Datenalter oben, das nur ERFOLGREICHE Abrufe verfolgt. Ein
        // negativer httpCode kommt von HTTPClient bei Verbindungs-/
        // Zeitueberschreitungsfehlern (kein HTTP-Statuscode vom Server),
        // ein httpCode ungleich 200 ist ein echter HTTP-Fehlercode vom
        // Server (z.B. 429/500) - beide Faelle sind fuer Alex als Nutzer
        // gleichermassen "Fehlercode: N", nur "kein HTTP-Code" (< 0) wird
        // als verstaendlicheres "Zeitueberschreitung" dargestellt.
        String resultText;
        if (!outcome.hasResult) {
            resultText = I18n::t(StringId::CONNECTION_STATUS_RESULT_NONE);
        } else if (outcome.ok) {
            resultText = I18n::t(StringId::CONNECTION_STATUS_RESULT_SUCCESS);
        } else if (outcome.httpCode <= 0) {
            resultText = I18n::t(StringId::CONNECTION_STATUS_RESULT_TIMEOUT);
        } else {
            resultText = String(I18n::t(StringId::CONNECTION_STATUS_RESULT_ERROR_PREFIX)) + outcome.httpCode;
        }
        y += drawPrefixedLine(tft, LINE_X, y, LINE_MAX_W, LINE_H,
                               I18n::t(StringId::CONNECTION_STATUS_RESULT_PREFIX), resultText);

        // Farbstatus: farbiger Punkt (funktioniert unabhaengig vom
        // aktuellen Radar-Farbthema, da Gruen/Gelb/Rot hier eine feste,
        // eigene Bedeutung haben - gleiches Prinzip wie die Hoehenfarben
        // auf dem Radar, siehe CLAUDE.md "Ausnahmen" bei den UI-
        // Konventionen) PLUS ein uebersetztes Wort (Gut/Mittel/Schlecht),
        // damit die Aussage nicht ausschliesslich von der Farbe abhaengt.
        uint16_t qColor = colorForQuality(quality);
        tft.fillCircle((int16_t)(LINE_X + 6), (int16_t)(y - 4), 6, qColor);
        tft.setTextColor(qColor, TFT_BLACK);
        tft.setCursor((int16_t)(LINE_X + 20), y);
        tft.println(String(I18n::t(StringId::CONNECTION_STATUS_QUALITY_PREFIX)) + labelForQuality(quality));
        y += LINE_H;

        Rect backBtn = {LINE_X, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        // Anders als die meisten anderen Menue-Screens (die nur bei einem
        // Tap neu zeichnen) wird hier alle ~500ms neu gezeichnet, auch ohne
        // Antippen - Datenalter und Farbstatus sollen sichtbar live
        // weiterlaufen, waehrend der Screen offen ist (Alex' Testwunsch:
        // Farbwechsel beim Beobachten einer simulierten Verbindungs-
        // unterbrechung live verfolgen koennen).
        TouchInput::Point tap;
        bool tapped = false;
        uint32_t waitStartMs = millis();
        while (true) {
            if (TouchInput::wasTapped(tap)) { tapped = true; break; }
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
            if (millis() - waitStartMs >= 500) break;
            MenuStars::update(tft);
            delay(20);
        }
        if (done) break;
        if (tapped && backBtn.contains(tap.x, tap.y)) done = true;
    }
}

}
