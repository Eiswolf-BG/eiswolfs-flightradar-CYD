#include "radar_screen.h"
#include "config.h"
#include "aircraft.h"
#include "aircraft_table.h"
#include "airline_lookup.h"
#include "aircraft_details.h"
#include "radar_math.h"
#include "units.h"
#include "settings_store.h"
#include "led_alert.h"
#include "location_manager.h"
#include "world_map.h"
#include "airline_filter.h"
#include "aircraft_watchlist.h"
#include "i18n.h"
#include <math.h>

namespace RadarScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    struct Layout {
        int16_t cx, cy, radius;
        Rect rangeBtn;
        int16_t infoTop;
    };

    constexpr int16_t INFO_BAR_H = 64;

    Layout computeLayout(int16_t top) {
        Layout L;
        L.infoTop = Config::SCREEN_HEIGHT - INFO_BAR_H;
        constexpr int16_t TOP_LABEL_MARGIN = 10;
        int16_t maxRadiusByWidth = Config::SCREEN_WIDTH / 2 - 6;
        int16_t maxRadiusByHeight = (L.infoTop - top - TOP_LABEL_MARGIN) / 2 - 6;
        L.radius = min(maxRadiusByWidth, maxRadiusByHeight);
        L.cx = Config::SCREEN_WIDTH / 2;
        L.cy = top + L.radius + 6 + TOP_LABEL_MARGIN;
        L.rangeBtn = {(int16_t)(Config::SCREEN_WIDTH - 70), (int16_t)(L.infoTop + 4), 62, 22};
        return L;
    }

    constexpr int16_t DETAIL_PANEL_H = 264;

    struct HitPoint {
        int16_t x, y;
        bool valid;
        char hex[7];
        char callsign[9];
        uint16_t color;
        float headingDeg;
        float distanceKm;
        bool isEmergency;
        bool isWatched;
    };
    constexpr uint8_t MAX_HIT_POINTS = Config::MAX_TRACKED_AIRCRAFT;
    HitPoint hitPoints[MAX_HIT_POINTS];

    char selectedHex[7] = {0};

    uint32_t lastEmptyTapMs = 0;
    int16_t lastEmptyTapX = -1000;
    int16_t lastEmptyTapY = -1000;
    constexpr uint32_t DOUBLE_TAP_MS = 400;
    constexpr int16_t DOUBLE_TAP_RADIUS = 25;

    bool ledBlinkOn = true;

    bool isEmergencySquawk(const char* squawk) {
        if (!squawk[0]) return false;
        for (uint8_t i = 0; i < Config::EMERGENCY_SQUAWK_COUNT; i++) {
            if (strcmp(squawk, Config::EMERGENCY_SQUAWKS[i]) == 0) return true;
        }
        return false;
    }

    float sweepAngleDeg = 0.0f;
    float prevSweepAngleDeg = -1.0f;
    constexpr float SWEEP_DEGREES_PER_SEC = 45.0f;

    uint16_t colorForAltitude(int32_t altFt) {
        if (altFt < Config::COLOR_LOW_ALT_THRESHOLD_FT) return TFT_GREEN;
        if (altFt < Config::COLOR_MID_ALT_THRESHOLD_FT) return TFT_YELLOW;
        return TFT_RED;
    }

    // Zeigt die PEILUNG (nicht zu verwechseln mit der Flugrichtung/heading)
    // zum aktuell ausgewaehlten Flugzeug: eine duenne, gepunktete Linie vom
    // Radar-Zentrum zum Kreisrand in Richtung bearingDeg, plus die Gradzahl
    // direkt ausserhalb des Rings. Hilft dabei, das Flugzeug tatsaechlich am
    // Himmel zu finden ("in diese Richtung schauen"). Nur fuer den Moment
    // des Zeichnens relevant - beim naechsten Sweep-Tick/Redraw wird sie vom
    // normalen Hintergrund-Redraw automatisch mit geloescht und neu gesetzt.
    void drawBearingIndicator(TFT_eSPI& gfx, const Layout& L, float bearingDeg) {
        double rad = bearingDeg * PI / 180.0;
        double s = sin(rad), c = cos(rad);

        // Gepunktete Linie: kurze Segmente statt einer durchgezogenen Linie,
        // damit sie sich optisch von den Kompass-Kreuzlinien und Ringen
        // unterscheidet und nicht wie ein staendiges UI-Element wirkt.
        constexpr uint8_t DOT_COUNT = 10;
        for (uint8_t i = 0; i < DOT_COUNT; i++) {
            if (i % 2 != 0) continue; // nur jedes zweite Segment zeichnen
            float t0 = (float)i / DOT_COUNT;
            float t1 = (float)(i + 1) / DOT_COUNT;
            int16_t x0 = L.cx + (int16_t)lround(t0 * L.radius * s);
            int16_t y0 = L.cy - (int16_t)lround(t0 * L.radius * c);
            int16_t x1 = L.cx + (int16_t)lround(t1 * L.radius * s);
            int16_t y1 = L.cy - (int16_t)lround(t1 * L.radius * c);
            gfx.drawLine(x0, y0, x1, y1, TFT_WHITE);
        }

        // Gradzahl knapp ausserhalb des Rings, an der Stelle wo die Peilung
        // den Kreisrand durchstoesst. Der Radius-Kreis nutzt fast die volle
        // Bildschirmbreite (siehe computeLayout(): nur 6px Rand) - ein Label
        // ausserhalb des Rings wuerde bei Ost/West-Peilungen ueber den
        // sichtbaren Bereich hinausragen. Deshalb: Label-Position an die
        // Bildschirmgrenzen clampen statt stur dem Winkel zu folgen.
        int16_t labelX = L.cx + (int16_t)lround((L.radius + 10) * s);
        int16_t labelY = L.cy - (int16_t)lround((L.radius + 10) * c);
        labelX = constrain(labelX, (int16_t)14, (int16_t)(Config::SCREEN_WIDTH - 14));
        labelY = constrain(labelY, (int16_t)10, (int16_t)(L.cy + L.radius + 8));
        char buf[6];
        snprintf(buf, sizeof(buf), "%.0f", bearingDeg);
        gfx.setTextDatum(MC_DATUM);
        gfx.setTextColor(TFT_WHITE, TFT_BLACK);
        gfx.drawString(buf, labelX, labelY);
        gfx.setTextDatum(TL_DATUM);
    }

    // kurzer Kursstrich in Flugrichtung (headingDeg) MIT Pfeilspitze am Ende -
    // eine reine Linie ohne Spitze war nicht eindeutig lesbar (nicht erkennbar,
    // welches Ende "vorne" ist). Die Spitze besteht aus zwei kurzen Strichen,
    // die knapp vor der Linienspitze schraeg nach aussen abzweigen (klassisches
    // Chevron-/Pfeilkopf-Symbol, wie bei ATC-Radardarstellungen ueblich).
    void drawAircraftMarker(TFT_eSPI& gfx, int16_t x, int16_t y, float headingDeg, uint16_t color) {
        gfx.fillCircle(x, y, 5, color);

        double rad = headingDeg * PI / 180.0;
        int16_t dx = (int16_t)(sin(rad) * 10);
        int16_t dy = (int16_t)(-cos(rad) * 10);
        int16_t tipX = x + dx;
        int16_t tipY = y + dy;
        gfx.drawLine(x, y, tipX, tipY, color);

        // Pfeilspitze: zwei kurze Striche von der Spitze aus, je 150 Grad
        // zur Kurslinie zurueckgeklappt (also leicht "nach hinten" zeigend,
        // wie bei einem Pfeilkopf "^" ueblich).
        constexpr double WING_ANGLE_RAD = 150.0 * PI / 180.0;
        constexpr int16_t WING_LEN = 4;
        double wing1 = rad + WING_ANGLE_RAD;
        double wing2 = rad - WING_ANGLE_RAD;
        int16_t w1x = tipX + (int16_t)(sin(wing1) * WING_LEN);
        int16_t w1y = tipY + (int16_t)(-cos(wing1) * WING_LEN);
        int16_t w2x = tipX + (int16_t)(sin(wing2) * WING_LEN);
        int16_t w2y = tipY + (int16_t)(-cos(wing2) * WING_LEN);
        gfx.drawLine(tipX, tipY, w1x, w1y, color);
        gfx.drawLine(tipX, tipY, w2x, w2y, color);
    }

    uint16_t dimColorForAltitude(int32_t altFt) {
        if (altFt < Config::COLOR_LOW_ALT_THRESHOLD_FT) return TFT_DARKGREEN;
        if (altFt < Config::COLOR_MID_ALT_THRESHOLD_FT) return TFT_OLIVE;
        return TFT_MAROON;
    }

    constexpr uint8_t TRAIL_LEN = 4;
    constexpr uint32_t TRAIL_STALE_MS = Config::FETCH_INTERVAL_MS * 3;

    struct TrailEntry {
        char hex[7] = {0};
        bool active = false;
        int16_t xs[TRAIL_LEN] = {0};
        int16_t ys[TRAIL_LEN] = {0};
        uint8_t count = 0;
        uint32_t lastUpdateMs = 0;
        uint16_t dimColor = TFT_DARKGREEN;
    };
    constexpr uint8_t MAX_TRAILS = Config::MAX_TRACKED_AIRCRAFT;
    TrailEntry trails[MAX_TRAILS];

    void pruneStaleTrails() {
        uint32_t now = millis();
        for (uint8_t i = 0; i < MAX_TRAILS; i++) {
            if (trails[i].active && now - trails[i].lastUpdateMs > TRAIL_STALE_MS) {
                trails[i] = TrailEntry{};
            }
        }
    }

    TrailEntry* findOrCreateTrail(const char* hex) {
        int16_t freeIdx = -1;
        for (uint8_t i = 0; i < MAX_TRAILS; i++) {
            if (trails[i].active && strcmp(trails[i].hex, hex) == 0) return &trails[i];
            if (!trails[i].active && freeIdx < 0) freeIdx = (int16_t)i;
        }
        if (freeIdx < 0) return nullptr;
        trails[freeIdx] = TrailEntry{};
        strncpy(trails[freeIdx].hex, hex, sizeof(trails[freeIdx].hex) - 1);
        trails[freeIdx].active = true;
        return &trails[freeIdx];
    }

    void pushTrailPoint(TrailEntry* t, int16_t x, int16_t y) {
        if (!t) return;
        if (t->count < TRAIL_LEN) {
            t->xs[t->count] = x;
            t->ys[t->count] = y;
            t->count++;
        } else {
            for (uint8_t i = 1; i < TRAIL_LEN; i++) {
                t->xs[i - 1] = t->xs[i];
                t->ys[i - 1] = t->ys[i];
            }
            t->xs[TRAIL_LEN - 1] = x;
            t->ys[TRAIL_LEN - 1] = y;
        }
        t->lastUpdateMs = millis();
    }

    void drawTrail(TFT_eSPI& gfx, const TrailEntry* t, uint16_t color) {
        if (!t || t->count < 2) return;
        for (uint8_t i = 1; i < t->count; i++) {
            gfx.drawLine(t->xs[i - 1], t->ys[i - 1], t->xs[i], t->ys[i], color);
        }
    }

    void printLineTruncated(TFT_eSPI& gfx, int16_t x, int16_t y, int16_t maxWidth, const String& text) {
        String s = text;
        if (gfx.textWidth(s) > maxWidth) {
            while (s.length() > 1 && gfx.textWidth(s + "...") > maxWidth) {
                s.remove(s.length() - 1);
            }
            s += "...";
        }
        gfx.setCursor(x, y);
        gfx.print(s);
    }

    void drawButton(TFT_eSPI& gfx, const Rect& r, const String& label) {
        gfx.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        gfx.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        gfx.setTextDatum(MC_DATUM);
        gfx.setTextColor(TFT_GREEN, TFT_BLACK);
        gfx.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        gfx.setTextDatum(TL_DATUM);
    }

    void drawLegend(TFT_eSPI& gfx, int16_t y) {
        bool metric = LocationManager::useMetricUnits();

        char lowLabel[10], midLabel[10], highLabel[10];
        if (metric) {
            int lowM = (int)(Units::feetToMeters(Config::COLOR_LOW_ALT_THRESHOLD_FT) / 100) * 100;
            int midM = (int)(Units::feetToMeters(Config::COLOR_MID_ALT_THRESHOLD_FT) / 100) * 100;
            snprintf(lowLabel, sizeof(lowLabel), "<%dm", lowM);
            snprintf(midLabel, sizeof(midLabel), "%d-%dm", lowM, midM);
            snprintf(highLabel, sizeof(highLabel), ">%dm", midM);
        } else {
            snprintf(lowLabel, sizeof(lowLabel), "<10k ft");
            snprintf(midLabel, sizeof(midLabel), "10-30k");
            snprintf(highLabel, sizeof(highLabel), ">30k ft");
        }

        struct { uint16_t color; const char* label; } items[3] = {
            {TFT_GREEN,  lowLabel},
            {TFT_YELLOW, midLabel},
            {TFT_RED,    highLabel},
        };
        int16_t segW = Config::SCREEN_WIDTH / 3;
        gfx.setTextColor(TFT_WHITE, TFT_BLACK);
        for (uint8_t i = 0; i < 3; i++) {
            int16_t x0 = i * segW + 6;
            gfx.fillCircle(x0, y - 5, 3, items[i].color);
            gfx.setCursor(x0 + 7, y);
            gfx.print(items[i].label);
        }
    }

    void drawWorldMap(TFT_eSPI& gfx, const Layout& L) {
        constexpr uint16_t dim = 0x0320;
        float scaleX = (2.0f * L.radius) / WorldMap::GRID_W;
        float scaleY = (2.0f * L.radius) / WorldMap::GRID_H;
        int32_t radiusSq = (int32_t)(L.radius - 2) * (L.radius - 2);

        for (uint8_t row = 0; row < WorldMap::GRID_H; row++) {
            uint64_t bits = WorldMap::ROWS[row];
            if (bits == 0) continue;
            for (uint8_t col = 0; col < WorldMap::GRID_W; col++) {
                if (!(bits & (1ULL << (WorldMap::GRID_W - 1 - col)))) continue;

                int16_t px = L.cx - L.radius + (int16_t)((col + 0.5f) * scaleX);
                int16_t py = L.cy - L.radius + (int16_t)((row + 0.5f) * scaleY);

                int16_t dx = px - L.cx;
                int16_t dy = py - L.cy;
                if ((int32_t)dx * dx + (int32_t)dy * dy > radiusSq) continue;

                gfx.drawPixel(px, py, dim);
            }
        }
    }

    void drawSweepLine(TFT_eSPI& gfx, const Layout& L, float angleDeg, uint16_t color) {
        double rad = angleDeg * DEG_TO_RAD;
        int16_t x2 = L.cx + (int16_t)(L.radius * sin(rad));
        int16_t y2 = L.cy - (int16_t)(L.radius * cos(rad));
        gfx.drawLine(L.cx, L.cy, x2, y2, color);
    }

    void drawStaticBackground(TFT_eSPI& gfx, const Layout& L, float rangeKm) {
        gfx.drawCircle(L.cx, L.cy, L.radius, TFT_DARKGREY);
        gfx.drawCircle(L.cx, L.cy, L.radius * 2 / 3, TFT_DARKGREY);
        gfx.drawCircle(L.cx, L.cy, L.radius / 3, TFT_DARKGREY);
        gfx.drawFastHLine(L.cx - L.radius, L.cy, L.radius * 2, TFT_DARKGREY);
        gfx.drawFastVLine(L.cx, L.cy - L.radius, L.radius * 2, TFT_DARKGREY);

        gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
        gfx.setTextDatum(MC_DATUM);
        gfx.drawString("N", L.cx, L.cy - L.radius - 8);
        gfx.drawString("S", L.cx, L.cy + L.radius - 10);
        gfx.drawString("E", L.cx + L.radius - 10, L.cy);
        gfx.drawString("W", L.cx - L.radius + 10, L.cy);
        char ringLabel[8];
        snprintf(ringLabel, sizeof(ringLabel), "%.0f", rangeKm / 3);
        gfx.drawString(ringLabel, L.cx, L.cy - L.radius / 3);
        snprintf(ringLabel, sizeof(ringLabel), "%.0f", rangeKm * 2 / 3);
        gfx.drawString(ringLabel, L.cx, L.cy - L.radius * 2 / 3);
        gfx.setTextDatum(TL_DATUM);
    }

    struct PanelState {
        bool valid = false;
        char hex[7] = {0};
        String callsignText, airlineText, modelText, typeText,
               altText, speedText, climbText, distHeadingText, squawkText, seatsText;
    };
    PanelState lastPanel;

    void updateLine(TFT_eSPI& gfx, int16_t y, int16_t h, int16_t maxWidth,
                     uint16_t fg, String& cached, const String& newText, bool forceFull) {
        if (!forceFull && cached == newText) return;
        gfx.fillRect(0, y - 14, Config::SCREEN_WIDTH, h, TFT_BLACK);
        gfx.setTextColor(fg, TFT_BLACK);
        printLineTruncated(gfx, 8, y, maxWidth, newText);
        cached = newText;
    }

    void drawDetailPanel(TFT_eSPI& gfx, Aircraft& a) {
        AirlineLookup::resolve(a);
        AircraftDetails::Info details = AircraftDetails::get(a.hex);

        int16_t panelTop = Config::SCREEN_HEIGHT - DETAIL_PANEL_H;
        constexpr int16_t textMaxWidth = Config::SCREEN_WIDTH - 16;
        constexpr int16_t LINE_H = 22;

        bool forceFull = !lastPanel.valid || strcmp(lastPanel.hex, a.hex) != 0;
        if (forceFull) {
            gfx.fillRect(0, panelTop, Config::SCREEN_WIDTH, DETAIL_PANEL_H, TFT_BLACK);
            gfx.drawRect(0, panelTop, Config::SCREEN_WIDTH, DETAIL_PANEL_H, TFT_GREEN);
            lastPanel = PanelState{};
            strncpy(lastPanel.hex, a.hex, sizeof(lastPanel.hex) - 1);
        }

        int16_t y = panelTop + 26;

        {
            String txt = a.callsign[0] ? a.callsign : a.hex;
            if (forceFull || lastPanel.callsignText != txt) {
                gfx.fillRect(0, y - 22, Config::SCREEN_WIDTH, 28, TFT_BLACK);
                gfx.setTextColor(TFT_GREEN, TFT_BLACK);
                gfx.setTextSize(2);
                printLineTruncated(gfx, 8, y, textMaxWidth, txt);
                gfx.setTextSize(1);
                lastPanel.callsignText = txt;
            }
        }
        y += 30;

        updateLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.airlineText,
                   String(a.airlineName), forceFull);
        y += LINE_H;

        String modelLine = details.loading
            ? String(I18n::t(StringId::DETAIL_MODEL)) + I18n::t(StringId::DETAIL_LOADING_DOTS)
            : String(I18n::t(StringId::DETAIL_MODEL)) + (details.model[0] ? details.model : I18n::t(StringId::DETAIL_UNKNOWN));
        updateLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.modelText, modelLine, forceFull);
        y += LINE_H;

        String typeLine = String(I18n::t(StringId::DETAIL_TYPE)) + (a.typeCode[0] ? a.typeCode : I18n::t(StringId::DETAIL_UNKNOWN));
        updateLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.typeText, typeLine, forceFull);
        y += LINE_H;

        char buf[48];
        snprintf(buf, sizeof(buf), "%s%.0fm / %.0fft", I18n::t(StringId::DETAIL_ALT),
                 Units::feetToMeters((float)a.altBaroFt), (float)a.altBaroFt);
        updateLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.altText, String(buf), forceFull);
        y += LINE_H;

        snprintf(buf, sizeof(buf), "%s%.0fkm/h / %.0fkt", I18n::t(StringId::DETAIL_SPEED),
                 Units::ktToKmh(a.groundSpeedKt), a.groundSpeedKt);
        updateLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.speedText, String(buf), forceFull);
        y += LINE_H;

        String climbLine;
        if (a.vertRateFtMin > 100) {
            snprintf(buf, sizeof(buf), "%s+%dft/min", I18n::t(StringId::DETAIL_CLIMB), a.vertRateFtMin);
            climbLine = buf;
        } else if (a.vertRateFtMin < -100) {
            snprintf(buf, sizeof(buf), "%s%dft/min", I18n::t(StringId::DETAIL_DESCENT), a.vertRateFtMin);
            climbLine = buf;
        } else {
            climbLine = I18n::t(StringId::DETAIL_LEVEL);
        }
        updateLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.climbText, climbLine, forceFull);
        y += LINE_H;

        snprintf(buf, sizeof(buf), "%s%.0fkm / %.0fnm   %s%.0f", I18n::t(StringId::DETAIL_DIST),
                 a.distanceKm, Units::kmToNm(a.distanceKm), I18n::t(StringId::DETAIL_HDG), a.headingDeg);
        updateLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.distHeadingText, String(buf), forceFull);
        y += LINE_H;

        String squawkLine = String(I18n::t(StringId::DETAIL_SQUAWK)) + (a.squawk[0] ? a.squawk : I18n::t(StringId::DETAIL_UNKNOWN));
        updateLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.squawkText, squawkLine, forceFull);
        y += LINE_H;

        String seatsLine = a.estSeats > 0
            ? String(I18n::t(StringId::DETAIL_SEATS_EST)) + a.estSeats
            : String(I18n::t(StringId::DETAIL_SEATS_UNKNOWN));
        updateLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.seatsText, seatsLine, forceFull);
        y += 22;

        if (forceFull) {
            gfx.setTextColor(TFT_DARKGREEN, TFT_BLACK);
            gfx.setCursor(8, y);
            gfx.print(I18n::t(StringId::DETAIL_TAP_CLOSE));
        }

        lastPanel.valid = true;
    }

    constexpr uint8_t STAR_COUNT = 28;
    struct Star {
        int16_t x, y;
        uint8_t phase;
        uint8_t speed;
    };
    Star stars[STAR_COUNT];
    bool starsInitialized = false;

    void initStarsIfNeeded() {
        if (starsInitialized) return;
        int16_t panelTop = Config::SCREEN_HEIGHT - DETAIL_PANEL_H;
        randomSeed((uint32_t)esp_random());
        for (uint8_t i = 0; i < STAR_COUNT; i++) {
            stars[i].x = 6 + random(0, Config::SCREEN_WIDTH - 12);
            stars[i].y = panelTop + 10 + random(0, DETAIL_PANEL_H - 20);
            stars[i].phase = (uint8_t)random(0, 256);
            stars[i].speed = 1 + random(0, 3);
        }
        starsInitialized = true;
    }

    void updateDetailPanelStars(TFT_eSPI& gfx) {
        initStarsIfNeeded();
        for (uint8_t i = 0; i < STAR_COUNT; i++) {
            stars[i].phase += stars[i].speed;
            uint8_t bright = (stars[i].phase < 128)
                ? (uint8_t)(stars[i].phase * 2)
                : (uint8_t)((255 - stars[i].phase) * 2);
            uint16_t color = gfx.color565(0, bright, 0);
            gfx.drawPixel(stars[i].x, stars[i].y, color);
        }
    }
}

void render(TFT_eSPI& tft, int16_t top) {
    Layout L = computeLayout(top);
    float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];

    bool panelAlreadyOpen = selectedHex[0] && lastPanel.valid &&
                            strcmp(lastPanel.hex, selectedHex) == 0;

    if (panelAlreadyOpen) {
        AircraftTable::lock();
        Aircraft* table = AircraftTable::raw();
        Aircraft selected{};
        bool found = false;
        for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
            if (table[i].valid && strcmp(table[i].hex, selectedHex) == 0) {
                selected = table[i];
                found = true;
                break;
            }
        }
        AircraftTable::unlock();

        if (found && selected.distanceKm <= rangeKm * 1.05f) {
            tft.startWrite();
            drawDetailPanel(tft, selected);
            tft.endWrite();
            return;
        }
        selectedHex[0] = 0;
        lastPanel.valid = false;
    }

    tft.startWrite();

    tft.fillRect(0, top, Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT - top, TFT_BLACK);

    drawWorldMap(tft, L);
    drawStaticBackground(tft, L, rangeKm);

    drawSweepLine(tft, L, sweepAngleDeg, TFT_GREEN);
    prevSweepAngleDeg = sweepAngleDeg;

    tft.fillCircle(L.cx, L.cy, 3, TFT_WHITE);

    static Aircraft snapshot[Config::MAX_TRACKED_AIRCRAFT];
    uint8_t count = 0;

    AircraftTable::lock();
    Aircraft* table = AircraftTable::raw();
    for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
        if (table[i].valid) snapshot[count++] = table[i];
    }
    AircraftTable::unlock();

    bool selectionStillPresent = false;
    Aircraft selected{};
    uint8_t visibleCount = 0; // nach allen Filtern (Reichweite, Bodenfahrzeuge, Airline-Filter)

    for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) hitPoints[i].valid = false;

    pruneStaleTrails();

    for (uint8_t i = 0; i < count && i < MAX_HIT_POINTS; i++) {
        Aircraft& a = snapshot[i];
        if (a.distanceKm > rangeKm * 1.05f) continue;

        if (SettingsStore::hideGroundVehicles() && a.category[0] == 'C') continue;

        if (AirlineFilter::isHidden(a.callsign)) continue;

        visibleCount++;

        RadarMath::PolarCoord polar{a.distanceKm, a.bearingDeg};
        RadarMath::ScreenPoint pt = RadarMath::toScreen(polar, L.cx, L.cy, L.radius, rangeKm);

        uint16_t color = colorForAltitude(a.altBaroFt);
        bool isSelected = selectedHex[0] && strcmp(a.hex, selectedHex) == 0;
        bool isEmergency = SettingsStore::emergencyAlertEnabled() && isEmergencySquawk(a.squawk);
        bool isWatched = SettingsStore::watchlistAlertEnabled() && AircraftWatchlist::isWatched(a.callsign);

        TrailEntry* trail = findOrCreateTrail(a.hex);
        uint16_t trailColor = dimColorForAltitude(a.altBaroFt);
        drawTrail(tft, trail, trailColor);
        pushTrailPoint(trail, pt.x, pt.y);
        if (trail) trail->dimColor = trailColor;

        if (isSelected) {
            tft.drawCircle(pt.x, pt.y, 9, TFT_WHITE);
            selectionStillPresent = true;
            selected = a;
            drawBearingIndicator(tft, L, a.bearingDeg);
        }
        drawAircraftMarker(tft, pt.x, pt.y, a.headingDeg, color);

        if (isEmergency) {
            tft.drawCircle(pt.x, pt.y, 12, TFT_RED);
        } else if (isWatched) {
            tft.drawCircle(pt.x, pt.y, 12, TFT_CYAN);
        }

        tft.setTextColor(color);
        tft.setTextDatum(BC_DATUM);
        const char* label = a.callsign[0] ? a.callsign : a.hex;
        tft.drawString(label, pt.x, pt.y - 8);
        tft.setTextDatum(TL_DATUM);

        hitPoints[i].x = pt.x;
        hitPoints[i].y = pt.y;
        hitPoints[i].valid = true;
        hitPoints[i].color = color;
        hitPoints[i].headingDeg = a.headingDeg;
        hitPoints[i].distanceKm = a.distanceKm;
        hitPoints[i].isEmergency = isEmergency;
        hitPoints[i].isWatched = isWatched;
        strncpy(hitPoints[i].hex, a.hex, sizeof(hitPoints[i].hex) - 1);
        strncpy(hitPoints[i].callsign, a.callsign, sizeof(hitPoints[i].callsign) - 1);
    }

    // "Leerer Himmel"-Timer: merkt sich, wann zuletzt mindestens ein
    // Flugzeug sichtbar war (nach allen Filtern). Statisch, damit der Wert
    // über mehrere render()-Aufrufe hinweg erhalten bleibt.
    static uint32_t lastAircraftSeenMs = millis();
    if (visibleCount > 0) lastAircraftSeenMs = millis();

    if (selectedHex[0] && !selectionStillPresent) {
        selectedHex[0] = 0;
    }

    if (selectionStillPresent) {
        tft.endWrite();
        drawDetailPanel(tft, selected);
        tft.startWrite();
    } else {
        lastPanel.valid = false;
        int16_t infoTop = L.infoTop;
        tft.drawFastHLine(0, infoTop, Config::SCREEN_WIDTH, TFT_DARKGREY);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(8, infoTop + 20);
        if (visibleCount == 0) {
            // Kein Flugzeug in Reichweite - statt des "Fuer Details
            // antippen"-Hinweises (der hier ohnehin ins Leere liefe) zeigen
            // wir an, wie lange der Himmel schon leer ist.
            uint32_t emptySec = (millis() - lastAircraftSeenMs) / 1000;
            char buf[16];
            if (emptySec < 60) {
                snprintf(buf, sizeof(buf), "%lus", (unsigned long)emptySec);
            } else {
                // Ab der ersten vollen Minute NUR noch Minuten anzeigen (ohne
                // Sekunden) - "1min 05s" war zu lang und stiess an den
                // Reichweiten-Button rechts daneben. Abgerundet (60-119s =
                // "1min", 120-179s = "2min", usw.) - intuitiv wie eine
                // normale Stoppuhr-Minutenanzeige.
                unsigned long minutes = emptySec / 60;
                snprintf(buf, sizeof(buf), "%lumin", minutes);
            }
            tft.print(String(I18n::t(StringId::RADAR_EMPTY_SKY_PREFIX)) + buf);
        } else {
            tft.print(I18n::t(StringId::RADAR_TAP_FOR_DETAILS));
        }

        char rangeLabel[8];
        snprintf(rangeLabel, sizeof(rangeLabel), "%.0fkm", rangeKm);
        drawButton(tft, L.rangeBtn, rangeLabel);

        drawLegend(tft, infoTop + 44);
    }

    tft.endWrite();
}

void tick(TFT_eSPI& tft, int16_t top, uint32_t deltaMs) {
    if (selectedHex[0]) {
        tft.startWrite();
        updateDetailPanelStars(tft);
        tft.endWrite();
        return;
    }

    Layout L = computeLayout(top);
    float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];

    tft.startWrite();

    if (prevSweepAngleDeg >= 0.0f) {
        drawSweepLine(tft, L, prevSweepAngleDeg, TFT_BLACK);
        drawStaticBackground(tft, L, rangeKm);
        tft.fillCircle(L.cx, L.cy, 3, TFT_WHITE);
    }

    sweepAngleDeg += SWEEP_DEGREES_PER_SEC * (deltaMs / 1000.0f);
    if (sweepAngleDeg >= 360.0f) sweepAngleDeg -= 360.0f;

    drawSweepLine(tft, L, sweepAngleDeg, TFT_GREEN);
    prevSweepAngleDeg = sweepAngleDeg;

    for (uint8_t i = 0; i < MAX_TRAILS; i++) {
        if (!trails[i].active) continue;

        bool stillVisible = false;
        for (uint8_t j = 0; j < MAX_HIT_POINTS; j++) {
            if (hitPoints[j].valid && strcmp(hitPoints[j].hex, trails[i].hex) == 0) {
                stillVisible = true;
                break;
            }
        }
        if (stillVisible) {
            drawTrail(tft, &trails[i], trails[i].dimColor);
        }
    }

    for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
        if (!hitPoints[i].valid) continue;
        const HitPoint& hp = hitPoints[i];

        bool inAlertRange = hp.distanceKm <= Config::LED_ALERT_RADIUS_KM;
        if (inAlertRange && !ledBlinkOn) {
            tft.fillRect(hp.x - 20, hp.y - 18, 40, 30, TFT_BLACK);
            continue;
        }

        drawAircraftMarker(tft, hp.x, hp.y, hp.headingDeg, hp.color);

        if (hp.isEmergency) {
            tft.drawCircle(hp.x, hp.y, 12, TFT_RED);
        } else if (hp.isWatched) {
            tft.drawCircle(hp.x, hp.y, 12, TFT_CYAN);
        }

        tft.setTextColor(hp.color);
        tft.setTextDatum(BC_DATUM);
        const char* label = hp.callsign[0] ? hp.callsign : hp.hex;
        tft.drawString(label, hp.x, hp.y - 8);
        tft.setTextDatum(TL_DATUM);
    }

    tft.endWrite();
}

bool handleTap(int16_t x, int16_t y, int16_t top) {
    Layout L = computeLayout(top);

    if (selectedHex[0]) {
        for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
            if (!hitPoints[i].valid) continue;
            int16_t dx = x - hitPoints[i].x;
            int16_t dy = y - hitPoints[i].y;
            if (dx * dx + dy * dy <= 12 * 12) {
                strncpy(selectedHex, hitPoints[i].hex, sizeof(selectedHex) - 1);
                AircraftDetails::request(hitPoints[i].hex);
                return true;
            }
        }
        selectedHex[0] = 0;
        lastPanel.valid = false;
        return true;
    }

    if (L.rangeBtn.contains(x, y)) {
        uint8_t idx = (SettingsStore::rangeIndex() + 1) % Config::RANGE_STEP_COUNT;
        SettingsStore::setRangeIndex(idx);
        return true;
    }

    for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
        if (!hitPoints[i].valid) continue;
        int16_t dx = x - hitPoints[i].x;
        int16_t dy = y - hitPoints[i].y;
        if (dx * dx + dy * dy <= 12 * 12) {
            strncpy(selectedHex, hitPoints[i].hex, sizeof(selectedHex) - 1);
            AircraftDetails::request(hitPoints[i].hex);
            return true;
        }
    }

    uint32_t nowMs = millis();
    bool isDoubleTap = (nowMs - lastEmptyTapMs <= DOUBLE_TAP_MS) &&
                       (abs((int)x - (int)lastEmptyTapX) <= DOUBLE_TAP_RADIUS) &&
                       (abs((int)y - (int)lastEmptyTapY) <= DOUBLE_TAP_RADIUS);
    lastEmptyTapMs = nowMs;
    lastEmptyTapX = x;
    lastEmptyTapY = y;

    if (isDoubleTap) {
        uint8_t idx = SettingsStore::rangeIndex();
        idx = (idx == 0) ? (Config::RANGE_STEP_COUNT - 1) : (idx - 1);
        SettingsStore::setRangeIndex(idx);
        lastEmptyTapMs = 0;
    }

    return true;
}

void updateProximityAlert(uint32_t nowMs) {
    bool anyClose = false;
    bool anyEmergency = false;
    bool anyWatched = false;

    bool proximityOn = SettingsStore::proximityAlertEnabled();
    bool emergencyOn = SettingsStore::emergencyAlertEnabled();
    bool watchlistOn = SettingsStore::watchlistAlertEnabled();

    if (proximityOn || emergencyOn || watchlistOn) {
        AircraftTable::lock();
        Aircraft* table = AircraftTable::raw();
        for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
            if (!table[i].valid) continue;
            if (proximityOn && table[i].distanceKm <= Config::LED_ALERT_RADIUS_KM) anyClose = true;
            if (emergencyOn && isEmergencySquawk(table[i].squawk)) anyEmergency = true;
            if (watchlistOn && AircraftWatchlist::isWatched(table[i].callsign)) anyWatched = true;
        }
        AircraftTable::unlock();
    }

    LedAlert::Mode mode = anyEmergency ? LedAlert::Mode::EmergencyRed
                         : anyWatched  ? LedAlert::Mode::WatchlistBlue
                         : anyClose    ? LedAlert::Mode::ProximityGreen
                                       : LedAlert::Mode::Off;

    ledBlinkOn = LedAlert::update(mode, nowMs);
}

EmergencyInfo checkEmergency() {
    EmergencyInfo info;
    if (!SettingsStore::emergencyAlertEnabled()) return info;

    AircraftTable::lock();
    Aircraft* table = AircraftTable::raw();
    for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
        if (table[i].valid && isEmergencySquawk(table[i].squawk)) {
            info.active = true;
            strncpy(info.callsign, table[i].callsign[0] ? table[i].callsign : table[i].hex,
                    sizeof(info.callsign) - 1);
            strncpy(info.squawk, table[i].squawk, sizeof(info.squawk) - 1);
            break;
        }
    }
    AircraftTable::unlock();

    return info;
}

}