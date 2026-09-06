#include "live_traffic_screen.h"
#include "aircraft_table.h"
#include "aircraft.h"
#include "airline_filter.h"
#include "radar_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "units.h"
#include "location_manager.h"
#include "ui_theme.h"
#include <cstring>

namespace LiveTrafficScreen {

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

    // Zeichnet eine Zeile linksbuendig bei (x,y), faellt aber automatisch
    // auf eine kuerzere Variante zurueck, falls "full" bei der aktuellen
    // Sprache nicht in maxWidth passt (siehe CLAUDE.md-Pflichtpruefung
    // Textbreite) - erst ohne Einheit ("fallback"), im (praktisch nie
    // erreichten) Extremfall hart zeichenweise gekuerzt. Damit muessen die
    // Uebersetzungen der Extremwert-Praefixe nicht auf eine feste
    // Zeichenzahl zusammengekuerzt werden, die in mancher Sprache trotzdem
    // knapp geworden waere.
    void printFittingLine(TFT_eSPI& tft, int16_t x, int16_t y, int16_t maxWidth,
                           const String& full, const String& fallback) {
        if (tft.textWidth(full) <= maxWidth) {
            tft.setCursor(x, y);
            tft.println(full);
            return;
        }
        if (tft.textWidth(fallback) <= maxWidth) {
            tft.setCursor(x, y);
            tft.println(fallback);
            return;
        }
        String truncated = fallback;
        while (truncated.length() > 1 && tft.textWidth(truncated) > maxWidth) {
            truncated.remove(truncated.length() - 1);
        }
        tft.setCursor(x, y);
        tft.println(truncated);
    }

    struct Stats {
        uint16_t total = 0;
        uint16_t airliner = 0, privateJet = 0, turboprop = 0, unknownType = 0;
        uint16_t helicopters = 0, heavy = 0;

        bool hasNearest = false;
        char nearestLabel[9] = {0};
        float nearestKm = 0;

        bool hasHighest = false;
        char highestLabel[9] = {0};
        int32_t highestFt = 0;

        bool hasLowest = false;
        char lowestLabel[9] = {0};
        int32_t lowestFt = 0;

        bool hasFastest = false;
        char fastestLabel[9] = {0};
        float fastestKt = 0;
    };

    // Einziger Durchlauf ueber die bereits vorhandene AircraftTable - reine
    // Aggregation, KEIN zusaetzlicher Netzwerk-/SD-Zugriff (Alex' Vorgabe).
    // Respektiert dieselben Filter wie Radar/Flugzeugliste (Reichweite,
    // Bodenfahrzeuge, Nur-Helikopter, Airline-Filter), damit die
    // Zusammenfassung exakt zu dem passt, was gerade auf dem Radar zu sehen
    // ist.
    Stats computeStats() {
        Stats s;
        float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];

        AircraftTable::lock();
        Aircraft* table = AircraftTable::raw();
        for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
            Aircraft& a = table[i];
            if (!a.valid) continue;
            if (a.distanceKm > rangeKm * 1.05f) continue;
            if (SettingsStore::hideGroundVehicles() && a.category[0] == 'C') continue;
            bool rotorcraft = RadarScreen::isRotorcraftCategory(a.category);
            if (SettingsStore::onlyHelicopters() && !rotorcraft) continue;
            if (AirlineFilter::isHidden(a.callsign)) continue;

            s.total++;

            // Ein Flugzeug zaehlt entweder als Hubschrauber ODER in eine der
            // vier Typ-Silhouetten-Kategorien (nie beides - ein Hubschrauber
            // hat ohnehin keine passende Silhouette in dieser Tabelle),
            // Heavy dagegen ist eine eigene, unabhaengige Zusatz-Info (ein
            // Heavy-Flugzeug ist z.B. trotzdem ein "Airliner").
            if (rotorcraft) {
                s.helicopters++;
            } else {
                switch (RadarScreen::classifyAircraftType(a.typeCode)) {
                    case RadarScreen::AircraftCategory::Airliner:   s.airliner++; break;
                    case RadarScreen::AircraftCategory::PrivateJet: s.privateJet++; break;
                    case RadarScreen::AircraftCategory::Turboprop:  s.turboprop++; break;
                    default:                                        s.unknownType++; break;
                }
            }
            if (RadarScreen::isHeavyAircraftCategory(a.category)) s.heavy++;

            const char* label = a.callsign[0] ? a.callsign : a.hex;

            if (!s.hasNearest || a.distanceKm < s.nearestKm) {
                s.hasNearest = true;
                s.nearestKm = a.distanceKm;
                strncpy(s.nearestLabel, label, sizeof(s.nearestLabel) - 1);
            }
            if (!s.hasHighest || a.altBaroFt > s.highestFt) {
                s.hasHighest = true;
                s.highestFt = a.altBaroFt;
                strncpy(s.highestLabel, label, sizeof(s.highestLabel) - 1);
            }
            if (!s.hasLowest || a.altBaroFt < s.lowestFt) {
                s.hasLowest = true;
                s.lowestFt = a.altBaroFt;
                strncpy(s.lowestLabel, label, sizeof(s.lowestLabel) - 1);
            }
            if (!s.hasFastest || a.groundSpeedKt > s.fastestKt) {
                s.hasFastest = true;
                s.fastestKt = a.groundSpeedKt;
                strncpy(s.fastestLabel, label, sizeof(s.fastestLabel) - 1);
            }
        }
        AircraftTable::unlock();
        return s;
    }
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();

    while (!done) {
        Stats s = computeStats();
        bool metric = LocationManager::useMetricUnits();
        constexpr int16_t LINE_X = 10;
        constexpr int16_t LINE_MAX_W = Config::SCREEN_WIDTH - 20;

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(LINE_X, 14);
        tft.println(I18n::t(StringId::MENU_LIVE_TRAFFIC));

        if (s.total == 0) {
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setCursor(LINE_X, 50);
            tft.println(I18n::t(StringId::AIRCRAFT_LIST_EMPTY));
        } else {
            // Gesamtzahl-Zeile bewusst in DERSELBEN Groesse (Size 1) wie
            // der Rest des Screens, statt wie urspruenglich in Size 2 -
            // bei Size 2 lief die deutsche Uebersetzung "Flugzeuge in
            // Reichweite: " (und vermutlich auch einige andere Sprachen)
            // bereits bei einstelligen Zahlen ueber die Bildschirmbreite
            // und brach per eingebautem Auto-Wrap in eine zweite Zeile um,
            // die direkt in die Typ-Aufschluesselung darunter hineinlief
            // (Alex' Meldung/Foto). Gleiches Prinzip wie im Statistik-
            // Screen (siehe CLAUDE.md: einheitliche Groesse ist robuster
            // als zwei unterschiedliche Groessen dicht nebeneinander) -
            // "prominent" kommt hier stattdessen durch eine eigene Zeile
            // mit etwas Abstand nach oben/unten, nicht durch groessere
            // Schrift.
            String totalLine = String(I18n::t(StringId::LIVE_TRAFFIC_TOTAL_PREFIX)) + s.total;
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(LINE_X, 38);
            tft.println(totalLine);
            int16_t chipTop = 58;

            // Typ-Aufschluesselung: NUR Kategorien mit count>0 aufnehmen
            // und dann luecken-frei einsortieren (Alex' Vorgabe: eine leere
            // Kategorie, z.B. gerade keine Hubschrauber sichtbar, soll
            // weder als stoerende "0" auftauchen noch eine leere Luecke im
            // Raster hinterlassen - sie wird einfach komplett weggelassen,
            // die uebrigen Eintraege ruecken luecken-frei nach).
            struct Chip { String text; };
            Chip chips[6];
            uint8_t chipCount = 0;
            auto addChip = [&](StringId id, uint16_t count) {
                if (count == 0) return;
                chips[chipCount++].text = String(I18n::t(id)) + ": " + count;
            };
            addChip(StringId::LIVE_TRAFFIC_TYPE_AIRLINER, s.airliner);
            addChip(StringId::LIVE_TRAFFIC_TYPE_PRIVATE_JET, s.privateJet);
            addChip(StringId::LIVE_TRAFFIC_TYPE_TURBOPROP, s.turboprop);
            addChip(StringId::LIVE_TRAFFIC_TYPE_UNKNOWN, s.unknownType);
            addChip(StringId::RADAR_FILTER_NAME_HELICOPTERS, s.helicopters);
            addChip(StringId::LEGEND_HEAVY, s.heavy);

            // Zweispaltig NUR wenn der laengste vorhandene Chip-Text auch
            // wirklich in eine halbe Zeilenbreite passt (Alex' Vorgabe:
            // "ggf. zweispaltig wenn Platz reicht") - sonst einspaltig,
            // damit lange Uebersetzungen (z.B. "Avion de ligne: 3") nie
            // abgeschnitten werden.
            int16_t halfColW = (int16_t)((LINE_MAX_W - 10) / 2);
            bool twoColumns = true;
            for (uint8_t i = 0; i < chipCount; i++) {
                if (tft.textWidth(chips[i].text) > halfColW) { twoColumns = false; break; }
            }

            constexpr int16_t CHIP_ROW_H = 20;
            uint8_t chipCols = twoColumns ? 2 : 1;
            int16_t chipColW = twoColumns ? (int16_t)(halfColW + 10) : LINE_MAX_W;
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            for (uint8_t i = 0; i < chipCount; i++) {
                uint8_t col = i % chipCols;
                uint8_t row = i / chipCols;
                int16_t x = (int16_t)(LINE_X + col * chipColW);
                int16_t y = (int16_t)(chipTop + row * CHIP_ROW_H);
                tft.setCursor(x, y);
                tft.println(chips[i].text);
            }

            uint8_t chipRows = chipCount == 0 ? 0 : (uint8_t)((chipCount + chipCols - 1) / chipCols);
            int16_t y = (int16_t)(chipTop + chipRows * CHIP_ROW_H + 16);
            constexpr int16_t EXTREME_ROW_H = 22;

            // Vier Extremwerte - jede Zeile nur, wenn ein gueltiger Wert
            // vorliegt (bei total>0 hier immer der Fall, defensiv trotzdem
            // geprueft). printFittingLine() garantiert, dass die Zeile in
            // JEDER der 8 Sprachen innerhalb LINE_MAX_W bleibt (siehe
            // Kommentar dort).
            if (s.hasNearest) {
                String prefix = I18n::t(StringId::LIVE_TRAFFIC_NEAREST_PREFIX);
                String fallback = prefix + s.nearestLabel;
                String full = metric
                    ? fallback + " (" + String(s.nearestKm, 0) + "km)"
                    : fallback + " (" + String(Units::kmToNm(s.nearestKm), 0) + "nm)";
                printFittingLine(tft, LINE_X, y, LINE_MAX_W, full, fallback);
                y += EXTREME_ROW_H;
            }
            if (s.hasHighest) {
                String prefix = I18n::t(StringId::LIVE_TRAFFIC_HIGHEST_PREFIX);
                String fallback = prefix + s.highestLabel;
                String full = metric
                    ? fallback + " (" + String(Units::feetToMeters((float)s.highestFt), 0) + "m)"
                    : fallback + " (" + String((long)s.highestFt) + "ft)";
                printFittingLine(tft, LINE_X, y, LINE_MAX_W, full, fallback);
                y += EXTREME_ROW_H;
            }
            if (s.hasLowest) {
                String prefix = I18n::t(StringId::LIVE_TRAFFIC_LOWEST_PREFIX);
                String fallback = prefix + s.lowestLabel;
                String full = metric
                    ? fallback + " (" + String(Units::feetToMeters((float)s.lowestFt), 0) + "m)"
                    : fallback + " (" + String((long)s.lowestFt) + "ft)";
                printFittingLine(tft, LINE_X, y, LINE_MAX_W, full, fallback);
                y += EXTREME_ROW_H;
            }
            if (s.hasFastest) {
                String prefix = I18n::t(StringId::LIVE_TRAFFIC_FASTEST_PREFIX);
                String fallback = prefix + s.fastestLabel;
                String full = metric
                    ? fallback + " (" + String(Units::ktToKmh(s.fastestKt), 0) + "km/h)"
                    : fallback + " (" + String(s.fastestKt, 0) + "kt)";
                printFittingLine(tft, LINE_X, y, LINE_MAX_W, full, fallback);
                y += EXTREME_ROW_H;
            }
        }

        Rect backBtn = {LINE_X, (int16_t)(Config::SCREEN_HEIGHT - 50), LINE_MAX_W, 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }
        if (backBtn.contains(tap.x, tap.y)) done = true;
    }
}

}
