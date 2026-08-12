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
#include <time.h>

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
    // Zusaetzliche Hoehe fuer die zweite Legenden-Zeile (Bodenfahrzeug-
    // Marker-Erklaerung), nur reserviert, wenn Bodenfahrzeuge tatsaechlich
    // angezeigt werden (SettingsStore::hideGroundVehicles() == false) -
    // ansonsten bleibt die Infoleiste genau so hoch wie bisher und der
    // Radarkreis behaelt seine gewohnte Groesse.
    constexpr int16_t INFO_BAR_GROUND_ROW_H = 18;

    int16_t infoBarHeight() {
        return SettingsStore::hideGroundVehicles() ? INFO_BAR_H : (int16_t)(INFO_BAR_H + INFO_BAR_GROUND_ROW_H);
    }

    Layout computeLayout(int16_t top) {
        Layout L;
        L.infoTop = Config::SCREEN_HEIGHT - infoBarHeight();
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
        bool isGroundVehicle;
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

    // "Leerer Himmel"-Timer: merkt sich, wann zuletzt mindestens ein
    // Flugzeug sichtbar war (nach allen Filtern) - namespace-weit statt
    // lokal in render(), da tick() (siehe dort) den Wert bei jedem Tick
    // (alle 80ms) braucht, um den Sekundenzaehler fluessig hochzuzaehlen,
    // statt nur alle paar Sekunden bei einem render()-Aufruf.
    uint32_t lastAircraftSeenMs = millis();

    // Laufschrift fuer die Info-Zeile unten (Tap-Hinweis bzw. "Leerer
    // Himmel"-Timer) - gleiches Grundprinzip wie der Naechster-Flughafen-
    // Text in location_presets_screen.cpp (bewusst dupliziert, kein
    // gemeinsames Modul, siehe CLAUDE.md-Konvention "jeder Screen
    // unabhaengig lauffaehig"). Wird bei jedem tick()-Aufruf (alle 80ms)
    // statt nur bei Aenderung neu gezeichnet, damit der Text fluessig und
    // endlos durchlaeuft.
    struct InfoMarquee {
        String text;
        String ring;          // text + Luecke, doppelt aneinandergehaengt
        bool needsScroll = false;
        int32_t charOffset = 0;
        uint32_t lastStepMs = 0;
    };
    InfoMarquee infoMarquee;

    enum class InfoMsgKind { None, TapForDetails, EmptySky };
    InfoMsgKind infoMarqueeKind = InfoMsgKind::None;

    constexpr uint32_t INFO_MARQUEE_STEP_MS = 200; // alle 200ms ein Zeichen weiter

    // Baut Text + Ring-Puffer neu auf und setzt den Scroll-Fortschritt
    // zurueck - nur aufrufen, wenn sich die ART der Nachricht aendert
    // (Tap-Hinweis <-> Leerer-Himmel-Timer), siehe updateInfoMarqueeText()
    // fuer den Fall, dass sich nur der Sekundenwert aendert.
    void setupInfoMarquee(TFT_eSPI& tft, const String& text, int16_t viewportW) {
        tft.setTextSize(1);
        infoMarquee.text = text;
        infoMarquee.needsScroll = tft.textWidth(text) > viewportW;
        String withGap = text + "   "; // 3 Leerzeichen Luecke vor der Wiederholung
        infoMarquee.ring = withGap + withGap;
        infoMarquee.charOffset = 0;
        infoMarquee.lastStepMs = millis();
    }

    // Aktualisiert nur den angezeigten Text (z.B. weil die Sekundenzahl des
    // "Leerer Himmel"-Timers weitergezaehlt hat), OHNE den Scroll-
    // Fortschritt zurueckzusetzen - so laeuft die Laufschrift trotz des
    // sich staendig aendernden Zaehlers fluessig weiter, statt bei jedem
    // Tick neu am Anfang zu beginnen. needsScroll wird trotzdem bei JEDEM
    // Aufruf neu bestimmt (nicht nur einmalig in setupInfoMarquee) - sonst
    // blieb der "Leerer Himmel"-Timer dauerhaft ohne Scrollen stehen, weil
    // der allererste Text ("...seit 0s") noch kurz genug war und die
    // Laufschrift-Funktion spaeter (z.B. bei "...seit 5min") nie erneut
    // geprueft hat, ob der inzwischen laengere Text nun doch scrollen muss.
    void updateInfoMarqueeText(TFT_eSPI& tft, const String& text, int16_t viewportW) {
        tft.setTextSize(1);
        infoMarquee.text = text;
        infoMarquee.needsScroll = tft.textWidth(text) > viewportW;
        String withGap = text + "   ";
        infoMarquee.ring = withGap + withGap;
    }

    // Liefert den laengsten Ausschnitt ab startIdx, der noch in maxWidth
    // passt - OHNE "..." anzuhaengen.
    String infoMarqueeWindow(TFT_eSPI& tft, const String& src, int32_t startIdx, int16_t maxWidth) {
        String s = src.substring(startIdx);
        while (s.length() > 1 && tft.textWidth(s) > maxWidth) {
            s.remove(s.length() - 1);
        }
        return s;
    }

    // Zeichnet die Info-Zeile neu - wenn der Text nicht scrollen muss, wird
    // er einfach normal (fest) angezeigt.
    void drawInfoMarquee(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w) {
        if (infoMarquee.text.length() == 0) return;

        constexpr int16_t CLEAR_TOP = 16;
        constexpr int16_t CLEAR_H = 20;
        tft.fillRect(x, y - CLEAR_TOP, w, CLEAR_H, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(x, y);

        if (!infoMarquee.needsScroll) {
            tft.print(infoMarquee.text);
            return;
        }

        uint32_t now = millis();
        if (now - infoMarquee.lastStepMs >= INFO_MARQUEE_STEP_MS) {
            infoMarquee.lastStepMs = now;
            infoMarquee.charOffset++;
            // Zurueck an den Anfang, sobald der erste (nicht doppelte)
            // Text+Luecke-Block durchgelaufen ist - so entsteht die
            // Endlosschleife.
            int32_t singleLen = (int32_t)infoMarquee.text.length() + 3;
            if (infoMarquee.charOffset >= singleLen) infoMarquee.charOffset = 0;
        }

        tft.print(infoMarqueeWindow(tft, infoMarquee.ring, infoMarquee.charOffset, w));
    }

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

    // Gleiches Nachtdimm-Fenster wie main.cpp::isNightDimHours() (22:00-06:00,
    // ueber Mitternacht hinweg gerechnet). Hier bewusst dupliziert statt
    // geteilt, siehe CLAUDE.md-Konvention ("jeder Screen unabhaengig
    // lauffaehig, kein gemeinsames Modul fuer solche Kleinigkeiten").
    bool isNightHours() {
        time_t now = time(nullptr);
        if (now <= 8 * 3600 * 2) return false; // Uhrzeit noch nicht per NTP synchronisiert
        struct tm tmNow;
        localtime_r(&now, &tmNow);
        int hour = tmNow.tm_hour;
        return (hour >= 22 || hour < 6);
    }

    // Nur aktiv, wenn der Nutzer die Nachtdimmung (Menue > System) eingeschaltet
    // hat UND wir gerade im Nachtfenster sind - dieselbe Einstellung, die sonst
    // nur die Hintergrundbeleuchtung dimmt, dimmt jetzt zusaetzlich auch die
    // Radar-Farben (Flugzeug-Marker + Sweep-Linie), damit das Display nachts
    // insgesamt weniger blendet.
    bool nightDimActiveNow() {
        return SettingsStore::nightDimmingEnabled() && isNightHours();
    }

    // Farbe je nach Flughoehe - gedaempft (dunkleres Gruen/Oliv) waehrend der
    // Nachtdimmung.
    //
    // Die rote Hoehenstufe wird bewusst NICHT gedaempft: der bisherige
    // Dimm-Ton (160,30,0) sah auf dem Display eher orange-braun statt rot
    // aus und war dadurch nicht mehr klar von der gelben Stufe zu
    // unterscheiden. Rot bleibt deshalb Tag und Nacht der reine TFT_RED-Ton.
    uint16_t colorForAltitude(TFT_eSPI& gfx, int32_t altFt) {
        bool dim = nightDimActiveNow();
        if (altFt < Config::COLOR_LOW_ALT_THRESHOLD_FT)
            return dim ? gfx.color565(0, 160, 0) : TFT_GREEN;
        if (altFt < Config::COLOR_MID_ALT_THRESHOLD_FT)
            return dim ? gfx.color565(160, 160, 0) : TFT_YELLOW;
        return TFT_RED;
    }

    uint16_t sweepLineColor(TFT_eSPI& gfx) {
        return nightDimActiveNow() ? gfx.color565(0, 110, 0) : TFT_GREEN;
    }

    // Feste Blau-Farbe fuer Bodenfahrzeug-Marker (ADS-B-Emitter-Kategorie
    // "C*", z.B. Flughafen-Fahrzeuge) - bewusst unabhaengig von
    // colorForAltitude(), da Bodenfahrzeuge praktisch immer auf 0ft stehen
    // und sonst dieselbe Farbe wie niedrig fliegende Flugzeuge haetten,
    // was die optische Unterscheidung (Quadrat vs. Kreis) unnoetig
    // erschweren wuerde. Bei Nachtdimmung gedaempft, gleiches Muster wie
    // colorForAltitude().
    uint16_t colorForGroundVehicle(TFT_eSPI& gfx) {
        return nightDimActiveNow() ? gfx.color565(0, 0, 160) : TFT_BLUE;
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

    // Eigener Marker fuer Bodenfahrzeuge (ADS-B-Kategorie "C*") - ein
    // gefuelltes Quadrat statt Kreis+Pfeilkopf, damit sie auf dem Radar
    // klar von echten Flugzeugen zu unterscheiden sind (Heading/Kurs ist
    // bei Bodenfahrzeugen ausserdem meist nicht aussagekraeftig).
    void drawGroundVehicleMarker(TFT_eSPI& gfx, int16_t x, int16_t y, uint16_t color) {
        constexpr int16_t HALF = 4;
        gfx.fillRect((int16_t)(x - HALF), (int16_t)(y - HALF), (int16_t)(2 * HALF), (int16_t)(2 * HALF), color);
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

        // Dieselbe colorForAltitude()-Funktion wie fuer die Flugzeug-Marker
        // selbst verwenden (mit einer typischen Hoehe je Band) - sonst zeigt
        // die Legende bei aktiver Nachtdimmung (22-6 Uhr) die HELLEN Farben,
        // waehrend die Marker/Beschriftungen auf dem Radar bereits gedaempft
        // sind. Das fuehrte dazu, dass z.B. ein rotes Flugzeug nachts eher
        // dunkelorange wirkte, obwohl die Legende noch reines Rot zeigte.
        struct { uint16_t color; const char* label; } items[3] = {
            {colorForAltitude(gfx, 0),                                   lowLabel},
            {colorForAltitude(gfx, Config::COLOR_LOW_ALT_THRESHOLD_FT),  midLabel},
            {colorForAltitude(gfx, Config::COLOR_MID_ALT_THRESHOLD_FT),  highLabel},
        };
        int16_t segW = Config::SCREEN_WIDTH / 3;
        gfx.setTextColor(TFT_WHITE, TFT_BLACK);

        // Aussenspalten (gruen/rot) bleiben auf ihrer festen 1/3-Raster-
        // Position (x0 = i*segW+6). Die mittlere (gelbe) Spalte wurde
        // bisher genauso starr positioniert, sass dadurch aber optisch zu
        // weit rechts: ihr Label ("3000-9100") ist deutlich laenger als
        // die Aussenlabels, wodurch rechts kaum noch Luft zum roten
        // Eintrag blieb, waehrend links viel Freiraum zum kurzen gruenen
        // Label uebrig war. Jetzt wird die gelbe Spalte stattdessen mittig
        // in die Luecke zwischen Ende des gruenen Textes und Anfang des
        // roten Punkts gesetzt.
        int16_t x0Low = 0 * segW + 6;
        int16_t x0High = 2 * segW + 6;

        gfx.fillCircle(x0Low, y - 5, 3, items[0].color);
        gfx.setCursor(x0Low + 7, y);
        gfx.print(items[0].label);

        gfx.fillCircle(x0High, y - 5, 3, items[2].color);
        gfx.setCursor(x0High + 7, y);
        gfx.print(items[2].label);

        int16_t lowTextEndX = x0Low + 7 + gfx.textWidth(items[0].label);
        int16_t highDotStartX = x0High - 3;
        int16_t midBlockW = 10 + gfx.textWidth(items[1].label); // Punktdurchmesser (6) + 4px Abstand + Textbreite
        int16_t gap = highDotStartX - lowTextEndX;
        int16_t x0Mid = lowTextEndX + 3 + (gap > midBlockW ? (gap - midBlockW) / 2 : 0);

        gfx.fillCircle(x0Mid, y - 5, 3, items[1].color);
        gfx.setCursor(x0Mid + 7, y);
        gfx.print(items[1].label);

        // Zweite Legenden-Zeile fuer die blauen Bodenfahrzeug-Quadrate - nur
        // wenn sie ueberhaupt sichtbar sind (Flugoptionen >
        // "Bodenfahrzeuge ausblenden" == aus). infoBarHeight() reserviert
        // den dafuer noetigen zusaetzlichen Platz nur in diesem Fall, siehe
        // computeLayout().
        if (!SettingsStore::hideGroundVehicles()) {
            int16_t gy = (int16_t)(y + INFO_BAR_GROUND_ROW_H);
            gfx.fillRect(3, (int16_t)(gy - 8), 6, 6, colorForGroundVehicle(gfx));
            gfx.setCursor(13, gy);
            gfx.print(I18n::t(StringId::LEGEND_GROUND_VEHICLE));
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
        // Ring-Beschriftungen (Zwischenabstaende) respektieren jetzt die
        // Einheiten-Einstellung (Menue > Einheiten) - vorher immer in km,
        // auch wenn Imperial (nm) eingestellt war. Gleiches Umrechnungs-
        // Muster wie beim Range-Button unten und der Legende oben.
        bool metric = LocationManager::useMetricUnits();
        float displayRange = metric ? rangeKm : Units::kmToNm(rangeKm);
        char ringLabel[8];
        snprintf(ringLabel, sizeof(ringLabel), "%.0f", displayRange / 3);
        gfx.drawString(ringLabel, L.cx, L.cy - L.radius / 3);
        snprintf(ringLabel, sizeof(ringLabel), "%.0f", displayRange * 2 / 3);
        gfx.drawString(ringLabel, L.cx, L.cy - L.radius * 2 / 3);
        gfx.setTextDatum(TL_DATUM);
    }

    // Laufschrift-Zustand fuer EINE Detail-Panel-Zeile - gleiches
    // Grundprinzip wie InfoMarquee oben, aber das Detail-Panel hat bis zu
    // neun solcher Zeilen gleichzeitig (Airline, Modell, Typ, Hoehe,
    // Geschw., Steig-/Sinkrate, Distanz/Peilung, Squawk, Sitzplaetze),
    // deshalb ein eigener Zustand pro Zeile statt einer einzigen globalen
    // Instanz. y/h/maxWidth/fg merken sich die Zeilen-Geometrie, damit
    // tickDetailPanelMarquees() (siehe unten) ohne Zusatzparameter weiss,
    // wo/wie jede Zeile neu zu zeichnen ist.
    struct LineMarquee {
        String text;
        String ring;
        bool needsScroll = false;
        int32_t charOffset = 0;
        uint32_t lastStepMs = 0;
        int16_t y = 0, h = 0, maxWidth = 0;
        uint16_t fg = TFT_GREEN;
    };

    struct PanelState {
        bool valid = false;
        char hex[7] = {0};
        String callsignText;
        LineMarquee airline, model, type, alt, speed, climb, distHeading, squawk, seats;
    };
    PanelState lastPanel;

    // Zeichnet eine Detail-Panel-Zeile NEU, wenn sich ihr Text geaendert
    // hat (oder das Panel komplett neu aufgebaut wird) - merkt sich dabei
    // auch, ob der Text zu breit fuer die verfuegbare Breite ist
    // (needsScroll) und baut bei Bedarf den Ring-Puffer fuer die
    // Laufschrift auf (gleiches Muster wie infoMarqueeWindow() oben).
    // Passt zu breite Zeilen ("Modell: ...", "Distanz: ...") NICHT mehr
    // mit "..." ab, sondern laesst sie horizontal durchscrollen - siehe
    // tickDetailPanelMarquees() weiter unten fuer den Teil, der das
    // tatsaechliche Weiterscrollen zwischen zwei Datenaktualisierungen
    // uebernimmt (render() liefert i.d.R. nur alle ~300ms neue Werte,
    // das waere fuer eine fluessige Laufschrift viel zu selten).
    void updateMarqueeLine(TFT_eSPI& gfx, int16_t y, int16_t h, int16_t maxWidth,
                            uint16_t fg, LineMarquee& m, const String& newText, bool forceFull) {
        // Geometrie/Farbe IMMER aktualisieren (auch ohne Textaenderung) -
        // tickDetailPanelMarquees() braucht diese Werte, um beim naechsten
        // Scroll-Schritt an der richtigen Stelle neu zu zeichnen.
        m.y = y;
        m.h = h;
        m.maxWidth = maxWidth;
        m.fg = fg;

        if (!forceFull && m.text == newText) return;

        m.text = newText;
        m.needsScroll = gfx.textWidth(newText) > maxWidth;
        String withGap = newText + "   "; // 3 Leerzeichen Luecke vor der Wiederholung
        m.ring = withGap + withGap;
        m.charOffset = 0;
        m.lastStepMs = millis();

        gfx.fillRect(0, y - 14, Config::SCREEN_WIDTH, h, TFT_BLACK);
        gfx.setTextColor(fg, TFT_BLACK);
        gfx.setCursor(8, y);
        gfx.print(m.needsScroll ? infoMarqueeWindow(gfx, m.ring, 0, maxWidth) : newText);
    }

    // Laesst eine einzelne Detail-Panel-Zeile weiterscrollen, falls sie zu
    // breit ist (needsScroll) und genug Zeit seit dem letzten Schritt
    // vergangen ist - sonst passiert nichts (kein unnoetiges Neuzeichnen
    // fuer Zeilen, die ohnehin komplett passen). Wird von
    // tickDetailPanelMarquees() fuer alle neun Zeilen aufgerufen.
    void advanceAndDrawMarqueeLine(TFT_eSPI& gfx, LineMarquee& m) {
        if (!m.needsScroll) return;

        uint32_t now = millis();
        if (now - m.lastStepMs < INFO_MARQUEE_STEP_MS) return;
        m.lastStepMs = now;

        m.charOffset++;
        int32_t singleLen = (int32_t)m.text.length() + 3;
        if (m.charOffset >= singleLen) m.charOffset = 0;

        gfx.fillRect(0, m.y - 14, Config::SCREEN_WIDTH, m.h, TFT_BLACK);
        gfx.setTextColor(m.fg, TFT_BLACK);
        gfx.setCursor(8, m.y);
        gfx.print(infoMarqueeWindow(gfx, m.ring, m.charOffset, m.maxWidth));
    }

    // Laesst alle Detail-Panel-Zeilen weiterscrollen - waehrend das Panel
    // offen ist, ruft tick() (alle ~80ms) das hier auf, DAZWISCHEN also
    // viel oefter als render() (das nur bei neuen Flugzeug-Daten,
    // ~alle 300ms, feuert) neue Werte liefert. Ohne diesen zusaetzlichen
    // Aufruf wuerde eine lange Zeile bei jedem render()-Aufruf zwar einen
    // Schritt weiterspringen, aber nicht fluessig durchlaufen.
    void tickDetailPanelMarquees(TFT_eSPI& gfx) {
        if (!lastPanel.valid) return;
        advanceAndDrawMarqueeLine(gfx, lastPanel.airline);
        advanceAndDrawMarqueeLine(gfx, lastPanel.model);
        advanceAndDrawMarqueeLine(gfx, lastPanel.type);
        advanceAndDrawMarqueeLine(gfx, lastPanel.alt);
        advanceAndDrawMarqueeLine(gfx, lastPanel.speed);
        advanceAndDrawMarqueeLine(gfx, lastPanel.climb);
        advanceAndDrawMarqueeLine(gfx, lastPanel.distHeading);
        advanceAndDrawMarqueeLine(gfx, lastPanel.squawk);
        advanceAndDrawMarqueeLine(gfx, lastPanel.seats);
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

        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.airline,
                   String(a.airlineName), forceFull);
        y += LINE_H;

        String modelLine = details.loading
            ? String(I18n::t(StringId::DETAIL_MODEL)) + I18n::t(StringId::DETAIL_LOADING_DOTS)
            : String(I18n::t(StringId::DETAIL_MODEL)) + (details.model[0] ? details.model : I18n::t(StringId::DETAIL_UNKNOWN));
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.model, modelLine, forceFull);
        y += LINE_H;

        String typeLine = String(I18n::t(StringId::DETAIL_TYPE)) + (a.typeCode[0] ? a.typeCode : I18n::t(StringId::DETAIL_UNKNOWN));
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.type, typeLine, forceFull);
        y += LINE_H;

        char buf[48];
        snprintf(buf, sizeof(buf), "%s%.0fm / %.0fft", I18n::t(StringId::DETAIL_ALT),
                 Units::feetToMeters((float)a.altBaroFt), (float)a.altBaroFt);
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.alt, String(buf), forceFull);
        y += LINE_H;

        snprintf(buf, sizeof(buf), "%s%.0fkm/h / %.0fkt", I18n::t(StringId::DETAIL_SPEED),
                 Units::ktToKmh(a.groundSpeedKt), a.groundSpeedKt);
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.speed, String(buf), forceFull);
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
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.climb, climbLine, forceFull);
        y += LINE_H;

        // Zusaetzlich zu km/nm auch Meilen (mi) - nm allein war fuer
        // Laien-Nutzer aus Ländern mit imperialen Einheiten (v.a. USA)
        // nicht selbsterklaerend, "Meilen" sind dort die gebraeuchlichere
        // Alltags-Distanzeinheit (nm bleibt trotzdem stehen, da es zur
        // Geschwindigkeit in kt passt: 1 kt = 1 nm/h).
        snprintf(buf, sizeof(buf), "%s%.0fkm / %.0fnm / %.0fmi  %s%.0f", I18n::t(StringId::DETAIL_DIST),
                 a.distanceKm, Units::kmToNm(a.distanceKm), Units::kmToMi(a.distanceKm),
                 I18n::t(StringId::DETAIL_HDG), a.headingDeg);
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.distHeading, String(buf), forceFull);
        y += LINE_H;

        String squawkLine = String(I18n::t(StringId::DETAIL_SQUAWK)) + (a.squawk[0] ? a.squawk : I18n::t(StringId::DETAIL_UNKNOWN));
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.squawk, squawkLine, forceFull);
        y += LINE_H;

        String seatsLine = a.estSeats > 0
            ? String(I18n::t(StringId::DETAIL_SEATS_EST)) + a.estSeats
            : String(I18n::t(StringId::DETAIL_SEATS_UNKNOWN));
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, TFT_GREEN, lastPanel.seats, seatsLine, forceFull);
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

    // Hintergrund-Sterne AUSSERHALB des Radarkreises (auf Wunsch: dasselbe
    // dekorative Sternenfunkeln wie im Detail-Panel/Menue, aber ohne mit
    // dem staendig neu gezeichneten Kreisinhalt zu kollidieren - genau DAS
    // wuerde "ruckeln" verursachen, da Sweep-Linie/Blips/Ringe innerhalb
    // des Kreises bei jedem tick() sowieso neu gezeichnet werden und dabei
    // die Sterne dort staendig wieder verschlucken wuerden). Ausserhalb
    // des Kreises bleibt der Hintergrund dauerhaft schwarz und wird von
    // sonst niemandem angefasst - dort koennen die Sterne frei vor sich
    // hin twinkeln, voellig unabhaengig von Sweep-Linie/Blip-Redraws.
    constexpr uint8_t BG_STAR_COUNT = 20;
    struct BgStar {
        int16_t x, y;
        uint8_t phase;
        uint8_t speed;
    };
    BgStar bgStars[BG_STAR_COUNT];
    bool bgStarsInitialized = false;

    // Platziert jeden Stern per Rejection-Sampling irgendwo im Bereich
    // [top..L.infoTop) x [0..SCREEN_WIDTH), aber nur wenn der Punkt
    // ausserhalb von Radius+MARGIN um den Kreismittelpunkt liegt (MARGIN
    // haelt zusaetzlich Abstand zum Ring und zum "N"-Kompasslabel knapp
    // ausserhalb des Kreises). Findet ein Versuch nach 25 Anlaeufen keinen
    // passenden Punkt (praktisch nie, da links/rechts vom Kreis immer
    // reichlich Platz ist), wird einfach der letzte Versuchspunkt
    // uebernommen - rein kosmetisches Feature, kein Grund fuer eine
    // Endlosschleife.
    void initBgStarsIfNeeded(const Layout& L, int16_t top) {
        if (bgStarsInitialized) return;
        randomSeed((uint32_t)esp_random());
        constexpr int16_t MARGIN = 6;
        int32_t minDist = L.radius + MARGIN;
        int32_t minDistSq = minDist * minDist;
        for (uint8_t i = 0; i < BG_STAR_COUNT; i++) {
            int16_t x = 0, y = 0;
            for (uint8_t attempt = 0; attempt < 25; attempt++) {
                x = (int16_t)random(2, Config::SCREEN_WIDTH - 2);
                y = (int16_t)random(top + 2, L.infoTop - 2);
                int32_t dx = x - L.cx, dy = y - L.cy;
                if (dx * dx + dy * dy > minDistSq) break;
            }
            bgStars[i].x = x;
            bgStars[i].y = y;
            bgStars[i].phase = (uint8_t)random(0, 256);
            bgStars[i].speed = (uint8_t)(1 + random(0, 3));
        }
        bgStarsInitialized = true;
    }

    void updateBgStars(TFT_eSPI& gfx, const Layout& L, int16_t top) {
        initBgStarsIfNeeded(L, top);
        for (uint8_t i = 0; i < BG_STAR_COUNT; i++) {
            bgStars[i].phase += bgStars[i].speed;
            uint8_t bright = (bgStars[i].phase < 128)
                ? (uint8_t)(bgStars[i].phase * 2)
                : (uint8_t)((255 - bgStars[i].phase) * 2);
            uint16_t color = gfx.color565(0, bright, 0);
            gfx.drawPixel(bgStars[i].x, bgStars[i].y, color);
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

    drawSweepLine(tft, L, sweepAngleDeg, sweepLineColor(tft));
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

    for (uint8_t i = 0; i < count && i < MAX_HIT_POINTS; i++) {
        Aircraft& a = snapshot[i];
        if (a.distanceKm > rangeKm * 1.05f) continue;

        if (SettingsStore::hideGroundVehicles() && a.category[0] == 'C') continue;

        if (AirlineFilter::isHidden(a.callsign)) continue;

        visibleCount++;

        RadarMath::PolarCoord polar{a.distanceKm, a.bearingDeg};
        RadarMath::ScreenPoint pt = RadarMath::toScreen(polar, L.cx, L.cy, L.radius, rangeKm);

        // ADS-B-Emitter-Kategorie "C*" = Bodenfahrzeug (nur ueberhaupt
        // sichtbar, wenn "Bodenfahrzeuge ausblenden" aus ist, siehe Filter
        // oben) - eigene Farbe/Marker statt der hoehenbasierten Flugzeug-
        // Darstellung, siehe drawGroundVehicleMarker()/colorForGroundVehicle().
        bool isGroundVehicle = a.category[0] == 'C';
        uint16_t color = isGroundVehicle ? colorForGroundVehicle(tft) : colorForAltitude(tft, a.altBaroFt);
        bool isSelected = selectedHex[0] && strcmp(a.hex, selectedHex) == 0;
        bool isEmergency = SettingsStore::emergencyAlertEnabled() && isEmergencySquawk(a.squawk);
        bool isWatched = SettingsStore::watchlistAlertEnabled() && AircraftWatchlist::isWatched(a.callsign);

        if (isSelected) {
            tft.drawCircle(pt.x, pt.y, 9, TFT_WHITE);
            selectionStillPresent = true;
            selected = a;
            drawBearingIndicator(tft, L, a.bearingDeg);
        }
        if (isGroundVehicle) {
            drawGroundVehicleMarker(tft, pt.x, pt.y, color);
        } else {
            drawAircraftMarker(tft, pt.x, pt.y, a.headingDeg, color);
        }

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
        hitPoints[i].isGroundVehicle = isGroundVehicle;
        strncpy(hitPoints[i].hex, a.hex, sizeof(hitPoints[i].hex) - 1);
        strncpy(hitPoints[i].callsign, a.callsign, sizeof(hitPoints[i].callsign) - 1);
    }

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
        // Der eigentliche Text/Scroll-Fortschritt der Info-Zeile (Tap-
        // Hinweis bzw. "Leerer Himmel"-Timer) wird weiterhin laufend in
        // tick() aktualisiert (siehe dort) - HIER aber trotzdem sofort mit
        // dem aktuellen Stand neu gezeichnet (statt bis zum naechsten
        // tick() zu warten). Der obige fillRect() ganz oben in render()
        // hat die Zeile naemlich gerade erst geloescht, und render() laeuft
        // nur bei jedem Fetch-Zyklus (ca. alle 8s, zeitgleich mit dem
        // Heartbeat-Blinken) - ohne dieses sofortige Nachzeichnen blieb die
        // Zeile bis zu 80ms lang leer, sichtbar als kurzes Blinken im
        // Heartbeat-Takt.
        constexpr int16_t INFO_TEXT_X = 8;
        constexpr int16_t INFO_TEXT_GAP = 6;
        int16_t infoTextY = infoTop + 20;
        int16_t infoTextW = L.rangeBtn.x - INFO_TEXT_X - INFO_TEXT_GAP;
        drawInfoMarquee(tft, INFO_TEXT_X, infoTextY, infoTextW);

        // Respektiert jetzt die Einheiten-Einstellung (Menue > Einheiten) -
        // vorher immer "XXkm", auch bei Imperial (dort jetzt "XXnm").
        char rangeLabel[8];
        bool rangeMetric = LocationManager::useMetricUnits();
        if (rangeMetric) {
            snprintf(rangeLabel, sizeof(rangeLabel), "%.0fkm", rangeKm);
        } else {
            snprintf(rangeLabel, sizeof(rangeLabel), "%.0fnm", Units::kmToNm(rangeKm));
        }
        drawButton(tft, L.rangeBtn, rangeLabel);

        drawLegend(tft, infoTop + 44);
    }

    tft.endWrite();
}

void tick(TFT_eSPI& tft, int16_t top, uint32_t deltaMs) {
    if (selectedHex[0]) {
        tft.startWrite();
        updateDetailPanelStars(tft);
        // Laesst zu breite Detail-Panel-Zeilen (z.B. "Modell: ..." oder
        // "Distanz: ...") weiterscrollen, statt wie zuvor mit "..." starr
        // abgeschnitten zu bleiben - render() liefert neue Werte nur alle
        // ~300ms, das reicht fuer eine fluessige Laufschrift nicht aus,
        // deshalb hier zusaetzlich im 80ms-Tick weiterschieben.
        tickDetailPanelMarquees(tft);
        tft.endWrite();
        return;
    }

    Layout L = computeLayout(top);
    float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];

    tft.startWrite();

    // Twinkeln ausserhalb des Radarkreises - laeuft im selben 80ms-Takt wie
    // die Sweep-Linie, damit es fluessig wirkt. render() (bei jedem Aircraft-
    // Update, alle ~300ms) loescht den kompletten Inhaltsbereich per fillRect
    // und zeichnet ihn neu, OHNE die Sterne erneut zu setzen - das ist
    // bewusst so (wie beim Detail-Panel-Sternenfeld auch): die naechste
    // tick()-Runde (spaetestens 80ms spaeter) laesst sie einfach wieder
    // aufblitzen, das ist zu kurz, um als Ruckeln wahrgenommen zu werden.
    updateBgStars(tft, L, top);

    if (prevSweepAngleDeg >= 0.0f) {
        drawSweepLine(tft, L, prevSweepAngleDeg, TFT_BLACK);
        drawStaticBackground(tft, L, rangeKm);
        tft.fillCircle(L.cx, L.cy, 3, TFT_WHITE);
    }

    sweepAngleDeg += SWEEP_DEGREES_PER_SEC * (deltaMs / 1000.0f);
    if (sweepAngleDeg >= 360.0f) sweepAngleDeg -= 360.0f;

    drawSweepLine(tft, L, sweepAngleDeg, sweepLineColor(tft));
    prevSweepAngleDeg = sweepAngleDeg;

    for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
        if (!hitPoints[i].valid) continue;
        const HitPoint& hp = hitPoints[i];

        bool inAlertRange = hp.distanceKm <= Config::LED_ALERT_RADIUS_KM;
        if (inAlertRange && !ledBlinkOn) {
            // Blink-Aus-Phase: frueher wurde hier ein pauschales 40x30px
            // schwarzes Rechteck gezeichnet, um den Marker zu "verstecken" -
            // das hat dabei aber auch die Radar-Ringe/Kompasslinien darunter
            // ueberdeckt (bei jeder Hoehenfarbe gleichermassen, da diese
            // Blink-Logik rein distanzbasiert ist). Stattdessen werden hier
            // jetzt exakt dieselben Formen, die beim normalen Zeichnen weiter
            // unten entstehen (Marker, Notfall-/Beobachtungs-Ring, Label),
            // einfach in Schwarz nachgezeichnet - das macht sie pixelgenau
            // wieder unsichtbar, ohne irgendetwas ausserhalb dieser Formen zu
            // beruehren.
            if (hp.isGroundVehicle) {
                drawGroundVehicleMarker(tft, hp.x, hp.y, TFT_BLACK);
            } else {
                drawAircraftMarker(tft, hp.x, hp.y, hp.headingDeg, TFT_BLACK);
            }
            if (hp.isEmergency || hp.isWatched) {
                tft.drawCircle(hp.x, hp.y, 12, TFT_BLACK);
            }
            tft.setTextColor(TFT_BLACK);
            tft.setTextDatum(BC_DATUM);
            const char* blinkLabel = hp.callsign[0] ? hp.callsign : hp.hex;
            tft.drawString(blinkLabel, hp.x, hp.y - 8);
            tft.setTextDatum(TL_DATUM);
            continue;
        }

        if (hp.isGroundVehicle) {
            drawGroundVehicleMarker(tft, hp.x, hp.y, hp.color);
        } else {
            drawAircraftMarker(tft, hp.x, hp.y, hp.headingDeg, hp.color);
        }

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

    // Info-Zeile unten (Tap-Hinweis bzw. "Leerer Himmel"-Timer) als
    // durchlaufende Laufschrift - siehe InfoMarquee weiter oben.
    // visibleCount wird hier bewusst erneut aus hitPoints[] gezaehlt
    // (statt aus render() uebernommen), da tick() unabhaengig von render()
    // laeuft und so auch den "Leerer Himmel"-Sekundenzaehler fluessig
    // hochzaehlen kann, ohne auf den naechsten render()-Aufruf warten zu
    // muessen.
    uint8_t visibleCountNow = 0;
    for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
        if (hitPoints[i].valid) visibleCountNow++;
    }
    if (visibleCountNow > 0) lastAircraftSeenMs = millis();

    String infoText;
    InfoMsgKind kind;
    if (visibleCountNow == 0) {
        kind = InfoMsgKind::EmptySky;
        uint32_t emptySec = (millis() - lastAircraftSeenMs) / 1000;
        char buf[16];
        if (emptySec < 60) {
            snprintf(buf, sizeof(buf), "%lus", (unsigned long)emptySec);
        } else {
            // Ab der ersten vollen Minute NUR noch Minuten anzeigen (ohne
            // Sekunden) - abgerundet (60-119s = "1min", 120-179s = "2min",
            // usw.) - intuitiv wie eine normale Stoppuhr-Minutenanzeige.
            unsigned long minutes = emptySec / 60;
            snprintf(buf, sizeof(buf), "%lumin", minutes);
        }
        infoText = String(I18n::t(StringId::RADAR_EMPTY_SKY_PREFIX)) + buf;
    } else {
        kind = InfoMsgKind::TapForDetails;
        infoText = I18n::t(StringId::RADAR_TAP_FOR_DETAILS);
    }

    constexpr int16_t INFO_TEXT_X = 8;
    constexpr int16_t INFO_TEXT_GAP = 6;
    int16_t infoTextY = L.infoTop + 20;
    int16_t infoTextW = L.rangeBtn.x - INFO_TEXT_X - INFO_TEXT_GAP;

    if (kind != infoMarqueeKind) {
        // Die ART der Nachricht hat sich geaendert (z.B. letztes Flugzeug
        // verschwunden) - Laufschrift komplett neu aufsetzen und von vorne
        // beginnen.
        infoMarqueeKind = kind;
        setupInfoMarquee(tft, infoText, infoTextW);
    } else {
        // Gleiche Art wie zuvor (z.B. weiterhin "Leerer Himmel", nur die
        // Sekundenzahl hat sich geaendert) - Text aktualisieren, aber NICHT
        // den Scroll-Fortschritt zuruecksetzen, sonst wuerde die
        // Laufschrift bei jedem Sekundenwechsel neu von vorne beginnen statt
        // fluessig durchzulaufen.
        updateInfoMarqueeText(tft, infoText, infoTextW);
    }
    drawInfoMarquee(tft, INFO_TEXT_X, infoTextY, infoTextW);

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

void selectAircraft(const char* hex) {
    strncpy(selectedHex, hex, sizeof(selectedHex) - 1);
    AircraftDetails::request(hex);
    // Erzwingt einen kompletten Neuaufbau des Detail-Panels beim naechsten
    // render() - wichtig, falls der Bildschirm zwischenzeitlich von einem
    // anderen Screen (z.B. der Flugzeugliste) ueberschrieben wurde, sonst
    // wuerden unveraenderte Zeilen faelschlich als "schon da" uebersprungen.
    lastPanel.valid = false;
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