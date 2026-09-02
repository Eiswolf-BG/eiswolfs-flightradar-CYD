#include "stats_history_screen.h"
#include "flight_logbook.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "menu_screen.h"
#include "config.h"
#include "i18n.h"
#include "ui_theme.h"
#include <time.h>

namespace StatsHistoryScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool danger = false) {
        uint16_t accent = danger ? TFT_RED : UiTheme::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Grosszuegiger als 7 gescannt, damit die letzten 7 Kalendertage auch
    // dann sicher in "days[]" landen, wenn zwischendurch viele weitere,
    // aeltere Logbuch-Dateien existieren.
    constexpr uint8_t MAX_DAYS_QUERIED = 40;
    constexpr uint8_t SLOT_COUNT = 7;

    constexpr int16_t CHART_TOP = 60;
    constexpr int16_t CHART_BOTTOM = 210;
    constexpr int16_t LABEL_RESERVE_TOP = 16;
    constexpr int16_t USABLE_BAR_HEIGHT = (CHART_BOTTOM - CHART_TOP) - LABEL_RESERVE_TOP;
    constexpr int16_t DAY_LABEL_Y = CHART_BOTTOM + 14;

    // Nutzt den gemeinsamen, bereits bewaehrten Info-Screen aus
    // menu_screen.cpp (MenuScreen::showInfoScreen()) statt einer eigenen
    // Scroll-/Layout-Implementierung - der Titel wird darueber automatisch
    // umgebrochen bzw. verkleinert, wenn er in einer Sprache nicht in eine
    // Zeile passt (vorher: festes tft.println() ohne Breitenpruefung).
    void runInfoScreen(TFT_eSPI& tft) {
        MenuScreen::showInfoScreen(tft, I18n::t(StringId::STATS_HISTORY_INFO_TITLE),
                                    I18n::t(StringId::STATS_HISTORY_INFO_PARA1),
                                    UiTheme::accentColor(tft), I18n::t(StringId::BACK));
    }
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::STATS_HISTORY_TITLE));
    tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
    tft.setCursor(10, 30);
    tft.print(I18n::t(StringId::LOADING));

    FlightLogbook::DayEntry days[MAX_DAYS_QUERIED];
    // listDaySummaries() statt listDays(): mehrere Sitzungs-Dateien am
    // selben Kalendertag (z.B. nach erneutem Einschalten) sollen hier
    // weiterhin als EIN Balken pro Tag zaehlen statt als mehrere.
    uint8_t count = FlightLogbook::listDaySummaries(days, MAX_DAYS_QUERIED);

    // Immer 7 feste Kalendertag-Slots (heute bis vor 6 Tagen), aeltester
    // zuerst - dadurch werden IMMER 7 einzelne, gleich breite Balken
    // gezeichnet, auch fuer Tage ohne eigene Logbuch-Datei (dann Zaehler
    // 0). Vorher wurden nur so viele Balken gezeichnet, wie tatsaechlich
    // Logbuch-Dateien existierten - bei nur 1-2 aktiven Tagen ergab das
    // 1-2 extrem breite, aneinanderklebende Balken, die wie eine einzige
    // durchgehende Flaeche statt 7 Saeulen aussahen (genau der von Alex
    // gemeldete Bug). Die Balkenwerte selbst kommen weiterhin aus den auf
    // der SD-Karte persistierten Logbuch-CSV-Dateien (siehe
    // FlightLogbook::listDaySummaries()/listDays()) - das war schon vorher
    // SD-persistiert, nicht RAM-only. Ein 8. Tag verdraengt den aeltesten
    // Slot automatisch, da hier bei jedem Aufruf neu ab "heute" gerechnet
    // wird (klassisches rollierendes Fenster ueber die Anzeige, ohne dass
    // dafuer aeltere Logbuch-Dateien geloescht werden muessten - die
    // werden fuer Top-Aircraft/Gesamtstatistik weiterhin gebraucht).
    struct Slot {
        char date[11];
        char dayLabel[3];
        uint32_t count;
    };
    Slot slots[SLOT_COUNT];

    time_t now = time(nullptr);
    // Gleicher Schwellwert wie in adsb_client.cpp/flight_logbook.cpp, um
    // "Uhrzeit noch nicht per NTP synchronisiert" zu erkennen.
    bool timeValid = now > 8 * 3600 * 2;

    if (timeValid) {
        struct tm todayTm;
        localtime_r(&now, &todayTm);
        // Mittag statt aktueller Uhrzeit - vermeidet Randfaelle bei der
        // Tagesarithmetik rund um eine DST-Umstellung.
        todayTm.tm_hour = 12;
        todayTm.tm_min = 0;
        todayTm.tm_sec = 0;

        for (uint8_t i = 0; i < SLOT_COUNT; i++) {
            struct tm t = todayTm;
            t.tm_mday -= (SLOT_COUNT - 1 - i); // i=0 -> vor 6 Tagen, ..., i=6 -> heute
            time_t adjusted = mktime(&t); // normalisiert Monats-/Jahresuebergaenge automatisch
            struct tm norm;
            localtime_r(&adjusted, &norm);
            snprintf(slots[i].date, sizeof(slots[i].date), "%04d-%02d-%02d",
                     norm.tm_year + 1900, norm.tm_mon + 1, norm.tm_mday);
            snprintf(slots[i].dayLabel, sizeof(slots[i].dayLabel), "%02d", norm.tm_mday);
            slots[i].count = 0;
            for (uint8_t j = 0; j < count; j++) {
                if (strncmp(days[j].date, slots[i].date, 10) == 0) {
                    slots[i].count = days[j].count;
                    break;
                }
            }
        }
    } else {
        // Uhrzeit noch nicht synchronisiert (z.B. sehr kurz nach dem
        // Booten, bevor NTP durchgelaufen ist) - echte Kalendertage lassen
        // sich noch nicht sicher berechnen. Fallback: die zuletzt
        // gefundenen Logbuch-Tage unveraendert anzeigen (wie vor diesem
        // Fix), damit der Screen trotzdem nutzbar bleibt statt leer oder
        // falsch beschriftet zu sein.
        uint8_t barCount = (count > SLOT_COUNT) ? SLOT_COUNT : count;
        uint8_t startIdx = count - barCount;
        for (uint8_t i = 0; i < SLOT_COUNT; i++) {
            slots[i].date[0] = 0;
            strncpy(slots[i].dayLabel, "--", sizeof(slots[i].dayLabel) - 1);
            slots[i].dayLabel[sizeof(slots[i].dayLabel) - 1] = 0;
            slots[i].count = 0;
        }
        for (uint8_t i = 0; i < barCount; i++) {
            const FlightLogbook::DayEntry& d = days[startIdx + i];
            strncpy(slots[i].date, d.date, sizeof(slots[i].date) - 1);
            slots[i].date[sizeof(slots[i].date) - 1] = 0;
            size_t dl = strlen(d.date);
            const char* lbl = dl >= 2 ? d.date + dl - 2 : d.date;
            strncpy(slots[i].dayLabel, lbl, sizeof(slots[i].dayLabel) - 1);
            slots[i].dayLabel[sizeof(slots[i].dayLabel) - 1] = 0;
            slots[i].count = d.count;
        }
    }

    uint32_t maxCount = 0;
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        if (slots[i].count > maxCount) maxCount = slots[i].count;
    }

    Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
    Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::STATS_HISTORY_TITLE));
        drawButton(tft, infoBtn, "?");

        if (count == 0) {
            tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
            tft.setCursor(10, 40);
            tft.println(I18n::t(StringId::STATS_HISTORY_EMPTY));
        } else {
            tft.drawFastHLine(10, CHART_BOTTOM, (int16_t)(Config::SCREEN_WIDTH - 20), UiTheme::accentColorDimmed(tft, 0.5f));

            // Immer SLOT_COUNT (7) gleich breite, durch Luecken klar
            // getrennte Balken - siehe Kommentar bei der Slot-Berechnung
            // oben, das war der eigentliche Bug (vorher variable, teils
            // extrem breite Balkenzahl je nach vorhandenen Logbuch-Dateien).
            int16_t barW = (int16_t)((Config::SCREEN_WIDTH - 20 - (SLOT_COUNT - 1) * 6) / SLOT_COUNT);
            int16_t x = 10;

            for (uint8_t i = 0; i < SLOT_COUNT; i++) {
                uint32_t dayCount = slots[i].count;

                // Auch Tage ohne Sichtungen bekommen einen sichtbaren,
                // eigenstaendigen Mini-Balken MIT "0"-Beschriftung statt
                // (wie vorher) nur einer unbeschrifteten Grundlinie - sonst
                // wirkt so ein Tag wie eine Luecke statt wie ein eigener,
                // ausgewerteter Balken.
                int16_t barH = (dayCount == 0)
                    ? 2
                    : (maxCount > 0)
                        ? (int16_t)((uint32_t)USABLE_BAR_HEIGHT * dayCount / maxCount)
                        : 0;
                if (barH < 2) barH = 2;
                int16_t barTop = CHART_BOTTOM - barH;

                uint16_t barColor = (dayCount == 0) ? UiTheme::accentColorDimmed(tft, 0.35f) : UiTheme::accentColor(tft);
                tft.fillRoundRect(x, barTop, barW, barH, 2, barColor);

                tft.setTextDatum(BC_DATUM);
                tft.setTextColor(dayCount == 0 ? UiTheme::accentColorDimmed(tft, 0.5f) : UiTheme::accentColor(tft), TFT_BLACK);
                tft.drawString(String(dayCount), x + barW / 2, barTop - 2);
                tft.setTextDatum(TL_DATUM);

                tft.setTextDatum(TC_DATUM);
                tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.5f), TFT_BLACK);
                tft.drawString(slots[i].dayLabel, x + barW / 2, DAY_LABEL_Y);
                tft.setTextDatum(TL_DATUM);

                x += barW + 6;
            }
        }

        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }

        if (infoBtn.contains(tap.x, tap.y)) {
            runInfoScreen(tft);
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
