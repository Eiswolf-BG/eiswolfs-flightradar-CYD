#include "session_stats_screen.h"
#include "session_stats.h"
#include "daily_sightings.h"
#include "aircraft_table.h"
#include "settings_store.h"
#include "location_manager.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "units.h"
#include "ui_theme.h"
#include <cstring>

namespace SessionStatsScreen {

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
    // fuer Text variabler Laenge) - erweitert (Alex' Auftrag, Session-
    // Highlights) um dasselbe Dual-Zweck-Scroll-Muster wie drawChipGrid() in
    // live_traffic_screen.cpp: draw=false liefert nur die End-Y-Position
    // (fuer eine vorherige Hoehenberechnung), draw=true zeichnet tatsaechlich
    // mit Scroll-Versatz/Sichtbarkeits-Pruefung. Der Screen war bisher NICHT
    // scrollbar (fester Inhalt) - mit den neuen Tagesstatistik-Zeilen reicht
    // der verfuegbare Platz nicht mehr aus.
    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                          int16_t lineHeight, const String& text,
                          int16_t scrollY, int16_t viewTop, int16_t viewBottom, bool draw) {
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

    // Loest einen Hex-Code auf ein Rufzeichen auf, FALLS das Flugzeug noch
    // in der AircraftTable steht - sonst Fallback auf den rohen Hex-Code
    // selbst (gleiches "label = callsign oder hex"-Prinzip wie ueberall im
    // Projekt). Fuer den Pass-by-Zaehler (DailySightings::topReturning()
    // liefert nur einen Hex-Code, kein Rufzeichen, siehe dortiger Kommentar).
    String resolveCallsign(const char* hex) {
        AircraftTable::lock();
        Aircraft* table = AircraftTable::raw();
        String result = hex;
        for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
            if (table[i].valid && strcmp(table[i].hex, hex) == 0) {
                if (table[i].callsign[0]) result = table[i].callsign;
                break;
            }
        }
        AircraftTable::unlock();
        return result;
    }
}

void run(TFT_eSPI& tft) {
    constexpr int16_t LINE_X = 10;
    constexpr int16_t LINE_MAX_W = Config::SCREEN_WIDTH - 20;
    constexpr int16_t ROW_H = 18;
    constexpr int16_t VIEW_TOP = 14;
    constexpr int16_t BACK_BTN_H = 40;
    constexpr int16_t BOTTOM_MARGIN = 10;
    constexpr int16_t SCROLL_ROW_H = 36;
    constexpr int16_t BTN_GAP = 8;

    bool done = false;
    MenuStars::reset();
    // Scroll-Position lebt AUSSERHALB der while-Schleife, damit sie bei
    // jedem Neuzeichnen erhalten bleibt - gleiches Muster wie
    // live_traffic_screen.cpp/die vier Wachlisten-Screens.
    int16_t scrollY = 0;

    while (!done) {
        bool metric = LocationManager::useMetricUnits();
        SessionStats::Snapshot s = SessionStats::get();
        String noData = I18n::t(StringId::SESSION_STATS_NO_DATA);

        // --- Alle Zeilen-Texte vorab bilden (gleich fuer Vermessung UND
        // Zeichnen gebraucht).
        String closestValue = noData;
        if (s.hasClosest) {
            char buf[24];
            if (metric) {
                snprintf(buf, sizeof(buf), "%s (%.1fkm)", s.closestCallsign, s.closestDistanceKm);
            } else {
                snprintf(buf, sizeof(buf), "%s (%.1fnm)", s.closestCallsign, Units::kmToNm(s.closestDistanceKm));
            }
            closestValue = buf;
        }
        String speedValue = noData;
        if (s.hasMaxSpeed) {
            char buf[16];
            if (metric) {
                snprintf(buf, sizeof(buf), "%.0fkm/h", Units::ktToKmh(s.maxSpeedKt));
            } else {
                snprintf(buf, sizeof(buf), "%.0fkt", s.maxSpeedKt);
            }
            speedValue = buf;
        }
        String altValue = noData;
        if (s.hasMaxAlt) {
            char buf[16];
            if (metric) {
                snprintf(buf, sizeof(buf), "%ldm", (long)Units::feetToMeters((float)s.maxAltFt));
            } else {
                snprintf(buf, sizeof(buf), "%ldft", (long)s.maxAltFt);
            }
            altValue = buf;
        }
        String topTypeValue = noData;
        if (s.hasTopType) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%s (%ux)", s.topType, (unsigned)s.topTypeCount);
            topTypeValue = buf;
        }

        // NEU (Alex' Auftrag, Session-Highlights): tagesbezogene Zeilen.
        // ("Seltenster Typ heute"/"meistgesehene Airline heute" wieder
        // entfernt, siehe Kommentar in session_stats.cpp - DRAM-Grund.)
        String longestTrackedValue = noData;
        if (s.hasLongestTracked) {
            uint32_t mins = s.longestTrackedSec / 60;
            uint32_t secs = s.longestTrackedSec % 60;
            char buf[24];
            snprintf(buf, sizeof(buf), "%s (%um %02us)", s.longestTrackedCallsign, (unsigned)mins, (unsigned)secs);
            longestTrackedValue = buf;
        }
        String maxClimbValue = noData;
        if (s.hasMaxClimb) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%s (+%dft/min)", s.maxClimbCallsign, s.maxClimbFtMin);
            maxClimbValue = buf;
        }
        String maxDescentValue = noData;
        if (s.hasMaxDescent) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%s (%dft/min)", s.maxDescentCallsign, s.maxDescentFtMin);
            maxDescentValue = buf;
        }
        String firstSeenTodayValue = noData;
        if (s.hasFirstSeenToday) {
            firstSeenTodayValue = s.firstSeenTodayCallsign;
        }
        String lastSeenTodayValue = noData;
        if (s.hasLastSeenToday) {
            lastSeenTodayValue = s.lastSeenTodayCallsign;
        }
        String passByValue = noData;
        DailySightings::TopReturning topReturn = DailySightings::topReturning();
        if (topReturn.available) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%s (%ux)", resolveCallsign(topReturn.hex).c_str(), (unsigned)topReturn.count);
            passByValue = buf;
        }

        Rect backBtn = {LINE_X, (int16_t)(Config::SCREEN_HEIGHT - BOTTOM_MARGIN - BACK_BTN_H),
                         LINE_MAX_W, BACK_BTN_H};
        Rect upBtn, downBtn;

        // --- Schritt 1: Gesamthoehe vermessen (draw=false), BEVOR
        // ueberhaupt etwas gezeichnet wird - exakt dasselbe Muster wie
        // live_traffic_screen.cpp.
        int16_t contentEndY = VIEW_TOP;
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, 14,
                                     I18n::t(StringId::SESSION_STATS_TITLE), 0, 0, 0, false);
        contentEndY = layoutWrapped(tft, LINE_X, (int16_t)(contentEndY + 4), LINE_MAX_W, 14,
                                     I18n::t(StringId::SESSION_STATS_SINCE_BOOT_NOTE), 0, 0, 0, false);
        contentEndY += 10;
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_UNIQUE_AIRCRAFT_PREFIX)) + s.uniqueAircraftCount, 0, 0, 0, false);
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_CLOSEST_PREFIX)) + closestValue, 0, 0, 0, false);
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_MAX_SPEED_PREFIX)) + speedValue, 0, 0, 0, false);
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_MAX_ALT_PREFIX)) + altValue, 0, 0, 0, false);
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_TOP_TYPE_PREFIX)) + topTypeValue, 0, 0, 0, false);
        contentEndY += 10;
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, 14,
                                     I18n::t(StringId::SESSION_STATS_TODAY_NOTE), 0, 0, 0, false);
        contentEndY += 4;
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_LONGEST_TRACKED_PREFIX)) + longestTrackedValue, 0, 0, 0, false);
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_MAX_CLIMB_PREFIX)) + maxClimbValue, 0, 0, 0, false);
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_MAX_DESCENT_PREFIX)) + maxDescentValue, 0, 0, 0, false);
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_FIRST_SEEN_TODAY_PREFIX)) + firstSeenTodayValue, 0, 0, 0, false);
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_LAST_SEEN_TODAY_PREFIX)) + lastSeenTodayValue, 0, 0, 0, false);
        contentEndY = layoutWrapped(tft, LINE_X, contentEndY, LINE_MAX_W, ROW_H,
                                     String(I18n::t(StringId::SESSION_STATS_PASS_BY_PREFIX)) + passByValue, 0, 0, 0, false);

        // --- Schritt 2: feste Anker fuer Zurueck/Scroll-Pfeile.
        int16_t viewBottomNoScroll = (int16_t)(backBtn.y - BTN_GAP);
        bool scrollable = contentEndY > viewBottomNoScroll;
        int16_t viewBottom = viewBottomNoScroll;
        int16_t maxScroll = 0;
        if (scrollable) {
            int16_t arrowRowY = (int16_t)(backBtn.y - BTN_GAP - SCROLL_ROW_H);
            upBtn = {LINE_X, arrowRowY, 100, SCROLL_ROW_H};
            downBtn = {(int16_t)(Config::SCREEN_WIDTH - 110), arrowRowY, 100, SCROLL_ROW_H};
            viewBottom = (int16_t)(arrowRowY - BTN_GAP);
        }
        maxScroll = (int16_t)max(0, contentEndY - viewBottom);
        if (scrollY > maxScroll) scrollY = maxScroll;
        if (scrollY < 0) scrollY = 0;

        // --- Schritt 3: tatsaechliches Zeichnen.
        tft.fillScreen(TFT_BLACK);
        int16_t y = VIEW_TOP;
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, 14, I18n::t(StringId::SESSION_STATS_TITLE),
                           scrollY, VIEW_TOP, viewBottom, true);

        tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.55f), TFT_BLACK);
        y = layoutWrapped(tft, LINE_X, (int16_t)(y + 4), LINE_MAX_W, 14,
                           I18n::t(StringId::SESSION_STATS_SINCE_BOOT_NOTE), scrollY, VIEW_TOP, viewBottom, true);
        y += 10;

        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_UNIQUE_AIRCRAFT_PREFIX)) + s.uniqueAircraftCount,
                           scrollY, VIEW_TOP, viewBottom, true);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_CLOSEST_PREFIX)) + closestValue,
                           scrollY, VIEW_TOP, viewBottom, true);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_MAX_SPEED_PREFIX)) + speedValue,
                           scrollY, VIEW_TOP, viewBottom, true);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_MAX_ALT_PREFIX)) + altValue,
                           scrollY, VIEW_TOP, viewBottom, true);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_TOP_TYPE_PREFIX)) + topTypeValue,
                           scrollY, VIEW_TOP, viewBottom, true);
        y += 10;

        tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.55f), TFT_BLACK);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, 14,
                           I18n::t(StringId::SESSION_STATS_TODAY_NOTE), scrollY, VIEW_TOP, viewBottom, true);
        y += 4;

        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_LONGEST_TRACKED_PREFIX)) + longestTrackedValue,
                           scrollY, VIEW_TOP, viewBottom, true);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_MAX_CLIMB_PREFIX)) + maxClimbValue,
                           scrollY, VIEW_TOP, viewBottom, true);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_MAX_DESCENT_PREFIX)) + maxDescentValue,
                           scrollY, VIEW_TOP, viewBottom, true);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_FIRST_SEEN_TODAY_PREFIX)) + firstSeenTodayValue,
                           scrollY, VIEW_TOP, viewBottom, true);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_LAST_SEEN_TODAY_PREFIX)) + lastSeenTodayValue,
                           scrollY, VIEW_TOP, viewBottom, true);
        y = layoutWrapped(tft, LINE_X, y, LINE_MAX_W, ROW_H,
                           String(I18n::t(StringId::SESSION_STATS_PASS_BY_PREFIX)) + passByValue,
                           scrollY, VIEW_TOP, viewBottom, true);

        if (scrollable) {
            drawButton(tft, upBtn, "^");
            drawButton(tft, downBtn, "v");
        }
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }
        if (done) break;

        if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        } else if (scrollable && upBtn.contains(tap.x, tap.y) && scrollY > 0) {
            scrollY -= 40;
            if (scrollY < 0) scrollY = 0;
        } else if (scrollable && downBtn.contains(tap.x, tap.y) && scrollY < maxScroll) {
            scrollY += 40;
            if (scrollY > maxScroll) scrollY = maxScroll;
        }
    }
}

}
