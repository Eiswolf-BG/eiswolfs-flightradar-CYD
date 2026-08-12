#include "aircraft_list_screen.h"
#include "aircraft_table.h"
#include "aircraft.h"
#include "airline_filter.h"
#include "aircraft_watchlist.h"
#include "radar_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "units.h"
#include "location_manager.h"
#include <algorithm>
#include <cstring>

namespace AircraftListScreen {

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

    // Gleiche Hoehen-Farblogik wie RadarScreen (dort intern, hier bewusst
    // dupliziert - siehe CLAUDE.md-Konvention "jeder Screen unabhaengig").
    // Auf die Nachtdimmung wird hier verzichtet: diese Liste ist ein Menue-
    // Screen wie jeder andere, kein Dauerbild wie das Radar.
    uint16_t colorForAltitude(int32_t altFt) {
        if (altFt < Config::COLOR_LOW_ALT_THRESHOLD_FT) return TFT_GREEN;
        if (altFt < Config::COLOR_MID_ALT_THRESHOLD_FT) return TFT_YELLOW;
        return TFT_RED;
    }

    bool isEmergencySquawk(const char* squawk) {
        if (!squawk[0]) return false;
        for (uint8_t i = 0; i < Config::EMERGENCY_SQUAWK_COUNT; i++) {
            if (strcmp(squawk, Config::EMERGENCY_SQUAWKS[i]) == 0) return true;
        }
        return false;
    }

    enum class SortMode : uint8_t { Distance, Altitude, Callsign };
    // Bleibt bis zum naechsten Neustart erhalten (kein SD-Speichern noetig -
    // das waere fuer eine reine Anzeige-Praeferenz unnoetiger Aufwand).
    SortMode sortMode = SortMode::Distance;

    const char* sortModeLabel() {
        switch (sortMode) {
            case SortMode::Altitude: return I18n::t(StringId::AIRCRAFT_LIST_SORT_ALTITUDE);
            case SortMode::Callsign: return I18n::t(StringId::AIRCRAFT_LIST_SORT_CALLSIGN);
            default:                 return I18n::t(StringId::AIRCRAFT_LIST_SORT_DISTANCE);
        }
    }
}

bool run(TFT_eSPI& tft) {
    constexpr int16_t ROW_H = 28;
    constexpr int16_t ROW_GAP = 4;
    constexpr int16_t LIST_TOP = 62;
    constexpr int16_t LIST_BOTTOM = Config::SCREEN_HEIGHT - 56;
    constexpr uint8_t ROWS_VISIBLE = (LIST_BOTTOM - LIST_TOP) / (ROW_H + ROW_GAP);

    uint8_t scrollTop = 0; // Index des ersten sichtbaren Eintrags in der sortierten Liste
    bool selected = false;
    bool done = false;
    MenuStars::reset();

    while (!done && !selected) {
        static Aircraft snapshot[Config::MAX_TRACKED_AIRCRAFT];
        uint8_t count = 0;
        float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];

        // Gleiche Filter wie das Radar (Reichweite, Bodenfahrzeuge,
        // Airline-Filter), damit die Liste genau die Flugzeuge zeigt, die
        // gerade auch als Punkte auf dem Radar zu sehen sind.
        AircraftTable::lock();
        Aircraft* table = AircraftTable::raw();
        for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
            if (!table[i].valid) continue;
            if (table[i].distanceKm > rangeKm * 1.05f) continue;
            if (SettingsStore::hideGroundVehicles() && table[i].category[0] == 'C') continue;
            if (AirlineFilter::isHidden(table[i].callsign)) continue;
            snapshot[count++] = table[i];
        }
        AircraftTable::unlock();

        std::sort(snapshot, snapshot + count, [](const Aircraft& a, const Aircraft& b) {
            switch (sortMode) {
                case SortMode::Altitude:
                    return a.altBaroFt < b.altBaroFt;
                case SortMode::Callsign: {
                    const char* ca = a.callsign[0] ? a.callsign : a.hex;
                    const char* cb = b.callsign[0] ? b.callsign : b.hex;
                    return strcmp(ca, cb) < 0;
                }
                default:
                    return a.distanceKm < b.distanceKm;
            }
        });

        if (scrollTop > 0 && scrollTop + ROWS_VISIBLE > count) {
            scrollTop = (count > ROWS_VISIBLE) ? (uint8_t)(count - ROWS_VISIBLE) : 0;
        }

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::AIRCRAFT_LIST_TITLE));

        Rect sortBtn = {10, 26, (int16_t)(Config::SCREEN_WIDTH - 20), 24};
        String sortLabel = String(I18n::t(StringId::AIRCRAFT_LIST_SORT_PREFIX)) + sortModeLabel();
        drawButton(tft, sortBtn, sortLabel);

        Rect rowRects[ROWS_VISIBLE];
        uint8_t visibleRowCount = 0;

        // Einheiten-Einstellung (Menue > Einheiten) einmal vor der Schleife
        // lesen statt pro Zeile - vorher zeigten Hoehe/Distanz hier immer
        // fest ft/km, auch bei Imperial eingestellt.
        bool listMetric = LocationManager::useMetricUnits();

        if (count == 0) {
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setCursor(10, LIST_TOP + 14);
            tft.println(I18n::t(StringId::AIRCRAFT_LIST_EMPTY));
        } else {
            int16_t y = LIST_TOP;
            for (uint8_t i = scrollTop; i < count && visibleRowCount < ROWS_VISIBLE; i++) {
                Aircraft& a = snapshot[i];
                Rect r = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
                rowRects[visibleRowCount] = r;

                bool emergency = isEmergencySquawk(a.squawk);
                bool watched = AircraftWatchlist::isWatched(a.callsign);
                uint16_t borderColor = emergency ? TFT_RED : (watched ? TFT_CYAN : TFT_GREEN);
                tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, borderColor);

                // WICHTIG (siehe CLAUDE.md-Falle "Font ist Baseline-verankert"):
                // Alle drei Textfelder ueber drawString() mit einem MITTIGEN
                // Datum auf die Zeilenmitte zentrieren, statt Baseline- und
                // Top-verankerte Aufrufe zu mischen - genau das hatte vorher
                // dazu gefuehrt, dass Hoehe/Distanz unten aus der Box liefen,
                // waehrend das Rufzeichen (Baseline-verankert) korrekt sass.
                int16_t midY = r.y + r.h / 2;

                const char* label = a.callsign[0] ? a.callsign : a.hex;
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.setTextDatum(ML_DATUM);
                tft.drawString(label, r.x + 6, midY);

                char buf[24];
                if (listMetric) {
                    snprintf(buf, sizeof(buf), "%.0fm", Units::feetToMeters((float)a.altBaroFt));
                } else {
                    snprintf(buf, sizeof(buf), "%.0fft", (float)a.altBaroFt);
                }
                tft.setTextColor(colorForAltitude(a.altBaroFt), TFT_BLACK);
                tft.setTextDatum(MR_DATUM);
                tft.drawString(buf, r.x + r.w - 62, midY);

                if (listMetric) {
                    snprintf(buf, sizeof(buf), "%.0fkm", a.distanceKm);
                } else {
                    snprintf(buf, sizeof(buf), "%.0fnm", Units::kmToNm(a.distanceKm));
                }
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.setTextDatum(MR_DATUM);
                tft.drawString(buf, r.x + r.w - 6, midY);

                tft.setTextDatum(TL_DATUM);

                y += ROW_H + ROW_GAP;
                visibleRowCount++;
            }
        }

        bool canUp = scrollTop > 0;
        bool canDown = (uint16_t)(scrollTop + ROWS_VISIBLE) < count;
        Rect upBtn   = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), 60, 40};
        Rect downBtn = {76, (int16_t)(Config::SCREEN_HEIGHT - 50), 60, 40};
        Rect backBtn = {146, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 156), 40};
        drawButton(tft, upBtn, "^");
        drawButton(tft, downBtn, "v");
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            MenuStars::update(tft);
            delay(20);
        }

        if (sortBtn.contains(tap.x, tap.y)) {
            sortMode = static_cast<SortMode>((static_cast<uint8_t>(sortMode) + 1) % 3);
            scrollTop = 0;
        } else if (canUp && upBtn.contains(tap.x, tap.y)) {
            scrollTop--;
        } else if (canDown && downBtn.contains(tap.x, tap.y)) {
            scrollTop++;
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        } else {
            for (uint8_t i = 0; i < visibleRowCount; i++) {
                if (rowRects[i].contains(tap.x, tap.y)) {
                    RadarScreen::selectAircraft(snapshot[scrollTop + i].hex, snapshot[scrollTop + i].callsign);
                    selected = true;
                    break;
                }
            }
        }
    }

    return selected;
}

}
