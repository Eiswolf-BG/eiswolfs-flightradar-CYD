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
#include <cmath>

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

    // Gleiche Hoehen-Bucket-Beschriftung wie altitudeLegendLabels() in
    // radar_screen.cpp (dort lokal/privat, hier dupliziert - CLAUDE.md-
    // Konvention) - identische Grenzwerte/Rundung, damit die Zahlen auf
    // diesem Screen exakt zur Hoehen-Farblegende auf dem Radar passen.
    void altitudeBucketLabels(char* lowLabel, size_t lowSz, char* midLabel, size_t midSz,
                               char* highLabel, size_t highSz) {
        bool metric = LocationManager::useMetricUnits();
        if (metric) {
            int lowM = (int)(Units::feetToMeters(Config::COLOR_LOW_ALT_THRESHOLD_FT) / 100) * 100;
            int midM = (int)(Units::feetToMeters(Config::COLOR_MID_ALT_THRESHOLD_FT) / 100) * 100;
            snprintf(lowLabel, lowSz, "<%dm", lowM);
            snprintf(midLabel, midSz, "%d-%dm", lowM, midM);
            snprintf(highLabel, highSz, ">%dm", midM);
        } else {
            snprintf(lowLabel, lowSz, "<10k ft");
            snprintf(midLabel, midSz, "10-30k");
            snprintf(highLabel, highSz, ">30k ft");
        }
    }

    struct Chip { String text; };

    // Zeichnet (oder vermisst nur, draw=false) ein Chip-Raster - EIN- oder
    // ZWEIspaltig, je nachdem ob der laengste Chip-Text bei zwei Spalten
    // ueberhaupt noch passt (gleiches Prinzip wie zuvor direkt in run()
    // fuer die Typ-Aufschluesselung, jetzt als gemeinsame Funktion fuer
    // Typ- UND Richtungs-Chips). Gleiches Dual-Zweck-Muster wie
    // layoutWrapped() (draw=false liefert nur die End-Y-Position fuer eine
    // vorherige Hoehenberechnung, ohne irgendetwas zu zeichnen) - so kann
    // die Gesamthoehe des scrollbaren Inhalts VOR dem eigentlichen
    // Zeichnen ermittelt werden, fuer maxScroll unten in run().
    int16_t drawChipGrid(TFT_eSPI& tft, const Chip* chips, uint8_t chipCount, int16_t x, int16_t topY,
                          int16_t maxWidth, int16_t rowH, int16_t scrollY, int16_t viewTop,
                          int16_t viewBottom, bool draw) {
        if (chipCount == 0) return topY;
        int16_t halfColW = (int16_t)((maxWidth - 10) / 2);
        bool twoColumns = true;
        for (uint8_t i = 0; i < chipCount; i++) {
            if (tft.textWidth(chips[i].text) > halfColW) { twoColumns = false; break; }
        }
        uint8_t cols = twoColumns ? 2 : 1;
        int16_t colW = twoColumns ? (int16_t)(halfColW + 10) : maxWidth;
        for (uint8_t i = 0; i < chipCount; i++) {
            uint8_t col = i % cols;
            uint8_t row = i / cols;
            int16_t cx = (int16_t)(x + col * colW);
            int16_t cy = (int16_t)(topY + row * rowH);
            if (draw) {
                int16_t screenY = cy - scrollY;
                if (screenY >= viewTop && screenY <= viewBottom) {
                    tft.setCursor(cx, screenY);
                    tft.println(chips[i].text);
                }
            }
        }
        uint8_t rows = (uint8_t)((chipCount + cols - 1) / cols);
        return (int16_t)(topY + rows * rowH);
    }

    struct Stats {
        uint16_t total = 0;
        uint16_t airliner = 0, privateJet = 0, turboprop = 0, unknownType = 0;
        uint16_t helicopters = 0, heavy = 0;

        // Verkehrsrichtung (Alex' Wunsch) - Verteilung nach 8 Himmels-
        // richtungs-Sektoren, basierend auf Aircraft::headingDeg (der
        // tatsaechliche Kurs des Flugzeugs, NICHT bearingDeg - das ist die
        // Peilung VOM Heimatstandort ZUM Flugzeug, siehe aircraft.h). Reihen-
        // folge N/NE/E/SE/S/SW/W/NW passt exakt zur StringId-Reihenfolge
        // COMPASS_N..COMPASS_NW (i18n.h) - direkte Index-Zuordnung moeglich.
        uint16_t dirCount[8] = {0, 0, 0, 0, 0, 0, 0, 0};

        // Hoehenverteilung (Alex' Wunsch) - dieselben drei Bereiche wie die
        // Hoehen-Farbcodierung (Config::COLOR_LOW_ALT_THRESHOLD_FT/
        // COLOR_MID_ALT_THRESHOLD_FT, siehe radar_screen.cpp::
        // colorForAltitude()) - keine neue Kategorisierung, nur ein neuer
        // Blickwinkel auf bereits bestehende Grenzwerte.
        uint16_t altLow = 0, altMid = 0, altHigh = 0;

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

        // Durchschnittshoehe/-geschwindigkeit (Alex' Wunsch) - einfache
        // Mittelwerte ueber ALLE sichtbaren Flugzeuge (gleiche Grundmenge
        // wie total, keine zusaetzliche Gueltigkeitspruefung - Bodenfahr-
        // zeuge mit 0ft/0kt ziehen den Schnitt bewusst mit nach unten,
        // genau wie sie auch in altLow/die uebrigen Zaehler einfliessen).
        float sumAltFt = 0;
        float sumSpeedKt = 0;
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

            // Gleiche Sektor-Formel wie windCompassLabel() in main.cpp/
            // compassLabel() in radar_screen.cpp (dort lokal dupliziert,
            // hier ebenso - CLAUDE.md-Konvention "jeder Screen dupliziert
            // seine eigenen kleinen Helfer").
            int sector = ((int)lround(a.headingDeg + 22.5f) / 45) % 8;
            if (sector < 0) sector += 8;
            s.dirCount[sector]++;

            if (a.altBaroFt < Config::COLOR_LOW_ALT_THRESHOLD_FT) s.altLow++;
            else if (a.altBaroFt < Config::COLOR_MID_ALT_THRESHOLD_FT) s.altMid++;
            else s.altHigh++;

            s.sumAltFt += a.altBaroFt;
            s.sumSpeedKt += a.groundSpeedKt;

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

    // Verkehrstrend (Alex' Auftrag): kleiner Ringpuffer im RAM, keine
    // Persistierung ueber einen Neustart hinweg noetig. 6 Slots x 5 Minuten
    // Abstand = bis zu 25 Minuten Spanne zwischen aeltestem und neuestem
    // Sample (liegt damit innerhalb der gewuenschten ~15-30 Minuten).
    // recordTrendSample() wird von net_task.cpp auf Core 0 aufgerufen,
    // computeTrend()/run() lesen auf Core 1 - bewusst OHNE eigenes Lock:
    // im denkbar ungluecklichsten Fall liest run() einen gerade erst zur
    // Haelfte geschriebenen Ringpuffer-Slot und zeigt den Trend fuer einen
    // Zyklus lang minimal ungenau an - fuer eine rein informative Anzeige
    // (kein Alarm, keine sicherheitsrelevante Logik) ein bewusst in Kauf
    // genommener Kompromiss, der ein zusaetzliches Lock/eine weitere
    // Cross-Core-Synchronisation fuer diesen Zweck unnoetig macht.
    constexpr uint8_t TREND_SAMPLE_CAPACITY = 6;
    constexpr uint32_t TREND_SAMPLE_INTERVAL_MS = 5UL * 60UL * 1000UL;

    uint16_t trendSamples[TREND_SAMPLE_CAPACITY] = {0};
    uint8_t trendSampleCount = 0;
    uint8_t trendSampleHead = 0;
    uint32_t lastTrendSampleMs = 0;
    bool trendSampleStarted = false;

    // Liefert true, wenn genug Samples fuer einen sinnvollen Trend
    // vorliegen (mind. 2), sonst false - der Aufrufer zeigt die Trend-Zeile
    // dann einfach gar nicht an (gleiches Muster wie "hasNearest" etc. oben,
    // kein erfundener Wert statt echter Daten).
    bool computeTrend(float& percentChange, bool& rising) {
        if (trendSampleCount < 2) return false;
        uint8_t oldestIdx = (uint8_t)((trendSampleHead + TREND_SAMPLE_CAPACITY - trendSampleCount) % TREND_SAMPLE_CAPACITY);
        uint8_t newestIdx = (uint8_t)((trendSampleHead + TREND_SAMPLE_CAPACITY - 1) % TREND_SAMPLE_CAPACITY);
        uint16_t oldest = trendSamples[oldestIdx];
        uint16_t newest = trendSamples[newestIdx];
        rising = newest >= oldest;
        if (oldest == 0) {
            // Division durch 0 vermeiden: kein Verkehr -> jetzt Verkehr wird
            // als "+100%" dargestellt statt als undefinierter Wert; blieb
            // es bei 0, gibt es schlicht keine Aenderung (0%).
            percentChange = (newest == 0) ? 0.0f : 100.0f;
        } else {
            percentChange = ((float)newest - (float)oldest) / (float)oldest * 100.0f;
        }
        return true;
    }
}

void recordTrendSample(uint16_t visibleTotal) {
    uint32_t now = millis();
    if (!trendSampleStarted || now - lastTrendSampleMs >= TREND_SAMPLE_INTERVAL_MS) {
        trendSampleStarted = true;
        lastTrendSampleMs = now;
        trendSamples[trendSampleHead] = visibleTotal;
        trendSampleHead = (uint8_t)((trendSampleHead + 1) % TREND_SAMPLE_CAPACITY);
        if (trendSampleCount < TREND_SAMPLE_CAPACITY) trendSampleCount++;
    }
}

void run(TFT_eSPI& tft) {
    constexpr int16_t LINE_X = 10;
    constexpr int16_t LINE_MAX_W = Config::SCREEN_WIDTH - 20;
    constexpr int16_t CHIP_ROW_H = 20;
    constexpr int16_t EXTREME_ROW_H = 22;
    constexpr int16_t VIEW_TOP = 58;
    constexpr int16_t BACK_BTN_H = 40;
    constexpr int16_t BOTTOM_MARGIN = 10;
    constexpr int16_t SCROLL_ROW_H = 36;
    constexpr int16_t BTN_GAP = 8;

    bool done = false;
    MenuStars::reset();
    // BUGFIX/VERBESSERUNG (Alex' Auftrag): mit den zwei neuen Anzeigen
    // (Verkehrsrichtung, Hoehenverteilung) kann der Inhalt bei vielfaeltigem
    // Verkehr (viele Typen + viele Richtungssektoren + alle vier
    // Extremwerte) mehr Platz brauchen, als auf den Bildschirm passt -
    // gleiches Scroll-Prinzip wie bei den vier Wachlisten-Screens (▲/▼-
    // Tasten, feste Zurueck-Position), hier aber in PIXELN statt ganzen
    // Zeilen, da der Inhalt aus unterschiedlich hohen Abschnitten besteht
    // (Chip-Raster + Einzelzeilen) - gleiches Grundprinzip wie der
    // bestehende Info-Popup-Scroll. Lebt AUSSERHALB der while-Schleife,
    // damit er bei jedem Neuzeichnen erhalten bleibt.
    int16_t scrollY = 0;

    while (!done) {
        Stats s = computeStats();
        bool metric = LocationManager::useMetricUnits();

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(LINE_X, 14);
        tft.println(I18n::t(StringId::MENU_LIVE_TRAFFIC));

        Rect backBtn = {LINE_X, (int16_t)(Config::SCREEN_HEIGHT - BOTTOM_MARGIN - BACK_BTN_H),
                         LINE_MAX_W, BACK_BTN_H};
        Rect upBtn, downBtn;
        bool scrollable = false;
        int16_t maxScroll = 0;

        if (s.total == 0) {
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setCursor(LINE_X, 50);
            tft.println(I18n::t(StringId::AIRCRAFT_LIST_EMPTY));
        } else {
            // Gesamtzahl-Zeile bewusst in DERSELBEN Groesse (Size 1) wie
            // der Rest des Screens - siehe historischer Bugfix-Kommentar
            // (vorher Size 2, brach bei laengeren Uebersetzungen um). Titel
            // + Gesamtzahl bleiben als fester Kopf immer sichtbar, nur der
            // Inhalt DARUNTER (ab VIEW_TOP) scrollt bei Bedarf.
            String totalLine = String(I18n::t(StringId::LIVE_TRAFFIC_TOTAL_PREFIX)) + s.total;
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(LINE_X, 38);
            tft.println(totalLine);

            // Typ-Aufschluesselung (bestehend): NUR Kategorien mit count>0
            // aufnehmen und luecken-frei einsortieren (Alex' Vorgabe: eine
            // leere Kategorie soll weder als stoerende "0" auftauchen noch
            // eine leere Luecke im Raster hinterlassen).
            Chip typeChips[6];
            uint8_t typeChipCount = 0;
            auto addTypeChip = [&](StringId id, uint16_t count) {
                if (count == 0) return;
                typeChips[typeChipCount++].text = String(I18n::t(id)) + ": " + count;
            };
            addTypeChip(StringId::LIVE_TRAFFIC_TYPE_AIRLINER, s.airliner);
            addTypeChip(StringId::LIVE_TRAFFIC_TYPE_PRIVATE_JET, s.privateJet);
            addTypeChip(StringId::LIVE_TRAFFIC_TYPE_TURBOPROP, s.turboprop);
            addTypeChip(StringId::LIVE_TRAFFIC_TYPE_UNKNOWN, s.unknownType);
            addTypeChip(StringId::RADAR_FILTER_NAME_HELICOPTERS, s.helicopters);
            addTypeChip(StringId::LEGEND_HEAVY, s.heavy);

            // NEU (Alex' Auftrag): Verkehrsrichtung - gleiches Chip-Raster-
            // Prinzip wie oben, nur mit den 8 Himmelsrichtungs-Sektoren
            // (s.dirCount, siehe computeStats()) statt Flugzeug-Typen.
            Chip dirChips[8];
            uint8_t dirChipCount = 0;
            for (uint8_t sec = 0; sec < 8; sec++) {
                if (s.dirCount[sec] == 0) continue;
                StringId label = (StringId)((int)StringId::COMPASS_N + sec);
                dirChips[dirChipCount++].text = String(I18n::t(label)) + ": " + s.dirCount[sec];
            }

            // NEU: Hauptflussrichtung - der Sektor mit den meisten Treffern,
            // angezeigt als "Von <Gegenrichtung> nach <Hauptrichtung>" (z.B.
            // "SW -> NE") - bei einem Ueberflug leichter verstaendlich als
            // eine einzelne Richtungsangabe.
            uint8_t dominantSector = 0;
            uint16_t dominantCount = 0;
            for (uint8_t sec = 0; sec < 8; sec++) {
                if (s.dirCount[sec] > dominantCount) {
                    dominantCount = s.dirCount[sec];
                    dominantSector = sec;
                }
            }
            uint8_t oppositeSector = (uint8_t)((dominantSector + 4) % 8);
            String dominantLine = String(I18n::t(StringId::LIVE_TRAFFIC_DOMINANT_PREFIX)) +
                I18n::t((StringId)((int)StringId::COMPASS_N + oppositeSector)) + " -> " +
                I18n::t((StringId)((int)StringId::COMPASS_N + dominantSector));

            // NEU: Hoehenverteilung - dieselben drei Bereiche wie die
            // Hoehen-Farblegende, Einheit folgt Metrisch/Imperial wie
            // ueberall im Projekt (altitudeBucketLabels() oben). Als
            // Chip-Raster (wie Typ/Richtung), NICHT als eine einzige
            // konkatenierte Zeile mit printFittingLine() - Bugfix: bei
            // zweistelligen Zaehlern oder laengeren Sprachen passte die
            // komplette Zeile nicht in LINE_MAX_W, und weil "full" und
            // "fallback" identisch waren, kuerzte printFittingLine()
            // zeichenweise vom ENDE ab - genau dort stand die dritte
            // Kategorie (">9100m"/">30k ft"), die dadurch unsichtbar
            // verschwand, ohne dass die Summe je zur Gesamtzahl passte.
            // Ein Chip-Raster kann das nicht: jede Kategorie ist ein
            // eigener Chip und wird nie abgeschnitten.
            char lowLabel[10], midLabel[10], highLabel[10];
            altitudeBucketLabels(lowLabel, sizeof(lowLabel), midLabel, sizeof(midLabel),
                                  highLabel, sizeof(highLabel));
            Chip altChips[3];
            uint8_t altChipCount = 0;
            auto addAltChip = [&](const char* label, uint16_t count) {
                if (count == 0) return;
                altChips[altChipCount++].text = String(label) + ": " + count;
            };
            addAltChip(lowLabel, s.altLow);
            addAltChip(midLabel, s.altMid);
            addAltChip(highLabel, s.altHigh);

            // NEU: Durchschnittshoehe/-geschwindigkeit - als Chip-Paar
            // (gleiches robustes Raster-Muster wie oben), damit auch hier
            // nichts abgeschnitten werden kann.
            float avgAltFt = s.total > 0 ? s.sumAltFt / s.total : 0;
            float avgSpeedKt = s.total > 0 ? s.sumSpeedKt / s.total : 0;
            Chip avgChips[2];
            avgChips[0].text = String(I18n::t(StringId::LIVE_TRAFFIC_AVG_ALT_PREFIX)) +
                (metric ? String(Units::feetToMeters(avgAltFt), 0) + "m"
                        : String((long)avgAltFt) + "ft");
            avgChips[1].text = String(I18n::t(StringId::LIVE_TRAFFIC_AVG_SPEED_PREFIX)) +
                (metric ? String(Units::ktToKmh(avgSpeedKt), 0) + "km/h"
                        : String(avgSpeedKt, 0) + "kt");
            uint8_t avgChipCount = 2;

            // NEU: Verkehrsdichte-Index - normiert auf Flugzeuge pro 100km
            // Radar-Reichweite (Radius, nicht Flaeche - Alex' Vorgabe
            // "Flugzeuge pro 100km"), damit die Einstufung nicht allein von
            // der aktuell eingestellten Reichweite abhaengt (sonst waere
            // "LOW" bei 10km und "HIGH" bei 100km mit identischem echten
            // Verkehrsaufkommen quasi garantiert). Schwellwerte 10/25 bewusst
            // so gewaehlt, dass ein normaler Tagesverkehr in dicht befahrenem
            // europaeischem Luftraum (z.B. Sueddeutschland) bei 50-100km
            // Reichweite typischerweise MED erreicht, nur bei spuerbar
            // ueberdurchschnittlichem Aufkommen (z.B. mehrere parallele
            // Luftstrassen/Anflugsektoren gleichzeitig aktiv) HIGH - und ein
            // leerer bis duenn befahrener Himmel (einzelne Flugzeuge bei
            // kleiner Reichweite) klar als LOW auffaellt.
            float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];
            float density = rangeKm > 0 ? (float)s.total * 100.0f / rangeKm : 0;
            StringId densityLevelId = StringId::LIVE_TRAFFIC_DENSITY_LOW;
            if (density >= 25.0f) densityLevelId = StringId::LIVE_TRAFFIC_DENSITY_HIGH;
            else if (density >= 10.0f) densityLevelId = StringId::LIVE_TRAFFIC_DENSITY_MED;
            String densityLevelText = I18n::t(densityLevelId);
            String densityLine = String(I18n::t(StringId::LIVE_TRAFFIC_DENSITY_PREFIX)) + densityLevelText;

            // NEU: Verkehrstrend - nur anzeigen, wenn der Ringpuffer schon
            // mind. 2 Samples hat (siehe computeTrend()) - in den ersten
            // ~5 Minuten nach dem Einschalten absichtlich keine Zeile statt
            // eines erfundenen Werts.
            float trendPercent = 0;
            bool trendRising = false;
            bool hasTrend = computeTrend(trendPercent, trendRising);
            String trendLine, trendFallback;
            if (hasTrend) {
                trendFallback = (trendRising ? "^ +" : "v -") + String(fabsf(trendPercent), 0) + "%";
                trendLine = String(I18n::t(StringId::LIVE_TRAFFIC_TREND_PREFIX)) + trendFallback;
            }

            // --- Schritt 1: Hoehe des GESAMTEN scrollbaren Inhalts
            // ermitteln (draw=false fuer beide Chip-Raster, feste
            // Zeilenhoehen fuer den Rest) - exakt dasselbe Dual-Zweck-
            // Prinzip wie layoutWrapped() in den anderen Screens: einmal
            // nur vermessen, um maxScroll zu kennen, BEVOR ueberhaupt etwas
            // gezeichnet wird.
            int16_t contentEndY = VIEW_TOP;
            contentEndY = drawChipGrid(tft, typeChips, typeChipCount, LINE_X, contentEndY,
                                        LINE_MAX_W, CHIP_ROW_H, 0, 0, 0, false);
            contentEndY += 16;
            contentEndY += EXTREME_ROW_H; // "Richtung:"-Kopfzeile
            contentEndY = drawChipGrid(tft, dirChips, dirChipCount, LINE_X, contentEndY,
                                        LINE_MAX_W, CHIP_ROW_H, 0, 0, 0, false);
            contentEndY += 16;
            contentEndY += EXTREME_ROW_H; // Hauptflussrichtung-Zeile
            contentEndY += EXTREME_ROW_H; // "Hoehe:"-Kopfzeile
            contentEndY = drawChipGrid(tft, altChips, altChipCount, LINE_X, contentEndY,
                                        LINE_MAX_W, CHIP_ROW_H, 0, 0, 0, false);
            contentEndY += 8;
            contentEndY = drawChipGrid(tft, avgChips, avgChipCount, LINE_X, contentEndY,
                                        LINE_MAX_W, CHIP_ROW_H, 0, 0, 0, false);
            contentEndY += 8;
            if (s.hasNearest) contentEndY += EXTREME_ROW_H;
            if (s.hasHighest) contentEndY += EXTREME_ROW_H;
            if (s.hasLowest) contentEndY += EXTREME_ROW_H;
            if (s.hasFastest) contentEndY += EXTREME_ROW_H;
            contentEndY += 8;
            contentEndY += EXTREME_ROW_H; // Verkehrsdichte-Zeile
            if (hasTrend) contentEndY += EXTREME_ROW_H;

            // --- Schritt 2: feste Anker fuer Zurueck/Scroll-Pfeile (gleiches
            // Muster wie bei den vier Wachlisten-Screens) - erst jetzt, mit
            // der bekannten Gesamthoehe, entscheiden, ob ueberhaupt
            // gescrollt werden muss.
            int16_t viewBottomNoScroll = (int16_t)(backBtn.y - BTN_GAP);
            scrollable = contentEndY > viewBottomNoScroll;
            int16_t viewBottom = viewBottomNoScroll;
            if (scrollable) {
                int16_t arrowRowY = (int16_t)(backBtn.y - BTN_GAP - SCROLL_ROW_H);
                upBtn = {LINE_X, arrowRowY, 100, SCROLL_ROW_H};
                downBtn = {(int16_t)(Config::SCREEN_WIDTH - 110), arrowRowY, 100, SCROLL_ROW_H};
                viewBottom = (int16_t)(arrowRowY - BTN_GAP);
            }
            maxScroll = (int16_t)max(0, contentEndY - viewBottom);
            if (scrollY > maxScroll) scrollY = maxScroll;
            if (scrollY < 0) scrollY = 0;

            // --- Schritt 3: eigentliches Zeichnen, jetzt mit Scroll-
            // Versatz/Clipping (gleiche screenY>=viewTop && <=viewBottom-
            // Pruefung wie in layoutWrapped()).
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            int16_t y = VIEW_TOP;
            y = drawChipGrid(tft, typeChips, typeChipCount, LINE_X, y, LINE_MAX_W, CHIP_ROW_H,
                              scrollY, VIEW_TOP, viewBottom, true);
            y += 16;

            // Kleine Kopfzeile ueber dem Richtungs-Raster, sonst waeren
            // Kompass-Kuerzel ("N: 3" etc.) ohne Kontext leicht mit der
            // Typ-Aufschluesselung zu verwechseln.
            {
                int16_t screenY = y - scrollY;
                if (screenY >= VIEW_TOP && screenY <= viewBottom) {
                    tft.setCursor(LINE_X, screenY);
                    tft.println(I18n::t(StringId::LIVE_TRAFFIC_DIRECTION_HEADER));
                }
            }
            y += EXTREME_ROW_H;
            y = drawChipGrid(tft, dirChips, dirChipCount, LINE_X, y, LINE_MAX_W, CHIP_ROW_H,
                              scrollY, VIEW_TOP, viewBottom, true);
            y += 16;

            // printFittingLine() garantiert, dass jede Zeile in JEDER der 8
            // Sprachen innerhalb LINE_MAX_W bleibt (siehe Kommentar dort) -
            // hier zusaetzlich mit Scroll-Versatz/Sichtbarkeits-Pruefung.
            auto drawIfVisible = [&](const String& full, const String& fallback) {
                int16_t screenY = y - scrollY;
                if (screenY >= VIEW_TOP && screenY <= viewBottom) {
                    printFittingLine(tft, LINE_X, screenY, LINE_MAX_W, full, fallback);
                }
                y += EXTREME_ROW_H;
            };

            drawIfVisible(dominantLine, dominantLine);

            // Kopfzeile ueber dem Hoehen-Raster, analog zur Richtungs-
            // Kopfzeile oben.
            {
                int16_t screenY = y - scrollY;
                if (screenY >= VIEW_TOP && screenY <= viewBottom) {
                    tft.setCursor(LINE_X, screenY);
                    tft.println(I18n::t(StringId::LIVE_TRAFFIC_ALTITUDE_HEADER));
                }
            }
            y += EXTREME_ROW_H;
            y = drawChipGrid(tft, altChips, altChipCount, LINE_X, y, LINE_MAX_W, CHIP_ROW_H,
                              scrollY, VIEW_TOP, viewBottom, true);
            y += 8;

            // NEU: Durchschnittshoehe/-geschwindigkeit als Chip-Paar.
            y = drawChipGrid(tft, avgChips, avgChipCount, LINE_X, y, LINE_MAX_W, CHIP_ROW_H,
                              scrollY, VIEW_TOP, viewBottom, true);
            y += 8;

            // Vier Extremwerte (bestehend) - jede Zeile nur, wenn ein
            // gueltiger Wert vorliegt.
            if (s.hasNearest) {
                String prefix = I18n::t(StringId::LIVE_TRAFFIC_NEAREST_PREFIX);
                String fallback = prefix + s.nearestLabel;
                String full = metric
                    ? fallback + " (" + String(s.nearestKm, 0) + "km)"
                    : fallback + " (" + String(Units::kmToNm(s.nearestKm), 0) + "nm)";
                drawIfVisible(full, fallback);
            }
            if (s.hasHighest) {
                String prefix = I18n::t(StringId::LIVE_TRAFFIC_HIGHEST_PREFIX);
                String fallback = prefix + s.highestLabel;
                String full = metric
                    ? fallback + " (" + String(Units::feetToMeters((float)s.highestFt), 0) + "m)"
                    : fallback + " (" + String((long)s.highestFt) + "ft)";
                drawIfVisible(full, fallback);
            }
            if (s.hasLowest) {
                String prefix = I18n::t(StringId::LIVE_TRAFFIC_LOWEST_PREFIX);
                String fallback = prefix + s.lowestLabel;
                String full = metric
                    ? fallback + " (" + String(Units::feetToMeters((float)s.lowestFt), 0) + "m)"
                    : fallback + " (" + String((long)s.lowestFt) + "ft)";
                drawIfVisible(full, fallback);
            }
            if (s.hasFastest) {
                String prefix = I18n::t(StringId::LIVE_TRAFFIC_FASTEST_PREFIX);
                String fallback = prefix + s.fastestLabel;
                String full = metric
                    ? fallback + " (" + String(Units::ktToKmh(s.fastestKt), 0) + "km/h)"
                    : fallback + " (" + String(s.fastestKt, 0) + "kt)";
                drawIfVisible(full, fallback);
            }
            y += 8;

            // NEU: Verkehrsdichte-Index (immer sichtbar, braucht keine
            // "hasX"-Pruefung - s.total ist ja per Definition schon bekannt).
            // Fallback ohne Praefix, analog zu den Extremwert-Zeilen oben -
            // gleiches Prinzip, das den Hoehenverteilungs-Bug behoben hat:
            // NIE dieselbe Zeichenkette als full UND fallback uebergeben.
            drawIfVisible(densityLine, densityLevelText);

            // NEU: Verkehrstrend - nur wenn genug Ringpuffer-Samples da sind.
            if (hasTrend) {
                drawIfVisible(trendLine, trendFallback);
            }
        }

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
