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
#include "ota_update.h"
#include "location_manager.h"
#include "sun_times.h"
#include "world_map.h"
#include "airline_filter.h"
#include "aircraft_watchlist.h"
#include "squawk_watchlist.h"
#include "aircraft_watchlist_screen.h"
#include "i18n.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "menu_screen.h"
#include "iss_tracker.h"
#include "weather.h"
#include "ui_theme.h"
#include <qrcode.h>
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

    // +22px gegenueber frueher (264) fuer die neue Route-Zeile (siehe
    // drawDetailPanel()) - beeinflusst NUR die Groesse des Detail-Panel-
    // Overlays, nicht das normale Radar-Layout (computeLayout() oben
    // richtet sich ausschliesslich nach infoBarHeight(), nicht nach
    // DETAIL_PANEL_H).
    constexpr int16_t DETAIL_PANEL_H = 286;

    // Flugzeugtyp-Silhouette nach ICAO-Typcode - Deklaration bewusst hier
    // vorgezogen (die eigentliche Zuordnungstabelle/classifyTypeSilhouette()
    // stehen weiter unten bei isHeavyCategory()), da HitPoint (gleich
    // darunter) diesen Typ bereits als Feld braucht.
    enum class TypeSilhouette { Unknown, Airliner, PrivateJet, Turboprop };

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
        // Militaer-/Behoerdenflug-Erkennung (isMilitaryGovSquawk()) bzw.
        // Rufzeichen-Praefix (isNotableCallsign()) - oranger Ring, siehe
        // tick()-Redraw-Block unten (gleiches Fortlauf-Prinzip wie
        // isEmergency/isWatched, sonst wuerde der Ring nur einen einzigen
        // render()-Aufruf lang sichtbar bleiben und dann durch den
        // naechsten tick()-Sweep-Restore wieder verschwinden).
        bool isNotable;
        bool isGroundVehicle;
        bool isRotorcraft;
        bool isHeavy;
        // Nur relevant, wenn keiner der drei Faelle oben zutrifft (siehe
        // drawTypedMarker()) - Flugzeugtyp-Silhouette aus dem ICAO-Typcode.
        TypeSilhouette typeSilhouette;
        // Fuer den CRT-Phosphor-Effekt (siehe crtPhosphorColor() unten) -
        // bearingDeg wird fuer die Sweep-Treffer-Erkennung in tick()
        // gebraucht, crtFadeEligible ist jetzt fuer JEDE Kategorie true
        // (frueher nur fuer niedrig fliegende, gruene Flugzeuge). baseColor
        // ist die kategorie-eigene, UNGEFAEDETE Farbe (Gruen/Gelb/Rot/Blau) -
        // tick() braucht sie als Fade-Ziel, da "color" zwischenzeitlich
        // bereits eine gefaedete Zwischenfarbe enthalten kann.
        float bearingDeg;
        bool crtFadeEligible;
        uint16_t baseColor;
    };
    constexpr uint8_t MAX_HIT_POINTS = Config::MAX_TRACKED_AIRCRAFT;
    HitPoint hitPoints[MAX_HIT_POINTS];

    // Phosphor-Nachleucht-Zustand fuer den CRT-Phosphor-Effekt, hex-basiert
    // statt ueber den hitPoints-Index verknuepft - der Index aendert sich bei
    // jedem render() durch die entfernungsbasierte Neusortierung in
    // AircraftTable::postFetchUpdate(), ein Flugzeug muesste sonst bei jeder
    // Neusortierung seinen Nachleucht-Fortschritt verlieren. Laeuft
    // kontinuierlich im Hintergrund mit (unabhaengig vom aktiven
    // Farbschema), damit beim Umschalten mitten im Betrieb sofort ein
    // konsistenter Zustand vorliegt - siehe render()/tick().
    struct PhosphorEntry {
        char hex[7] = {0};
        bool used = false;
        bool seenThisRender = false;
        bool everSwept = false;
        uint32_t lastSweptMs = 0;
    };
    PhosphorEntry phosphorEntries[MAX_HIT_POINTS];

    PhosphorEntry* findOrCreatePhosphor(const char* hex) {
        for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
            if (phosphorEntries[i].used && strcmp(phosphorEntries[i].hex, hex) == 0) {
                return &phosphorEntries[i];
            }
        }
        for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
            if (!phosphorEntries[i].used) {
                phosphorEntries[i] = PhosphorEntry{};
                phosphorEntries[i].used = true;
                strncpy(phosphorEntries[i].hex, hex, sizeof(phosphorEntries[i].hex) - 1);
                return &phosphorEntries[i];
            }
        }
        return nullptr; // sollte nie vorkommen, gleiche Kapazitaet wie hitPoints[]
    }

    char selectedHex[7] = {0};

    // Zeigt "Leerer Himmel"-Zeile gerade einen Filter-Hinweis an (siehe
    // render())? Von handleTap() geprueft, um die Zeile nur dann antippbar
    // zu machen und direkt ins "Anzeigefilter"-Menue springen zu lassen -
    // sonst wuerde ein Tap auf die normale "Leerer Himmel"-Zeile (ohne
    // aktiven Filter) unerwartet das Menue oeffnen.
    bool infoTextHasFilterHint = false;

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
        // Roter Abschnitt innerhalb von "text" (z.B. das Wort "Hinweis" im
        // Filter-Hinweis, siehe RADAR_FILTER_HINT_WORD) - redLen<=0 heisst
        // "kein roter Abschnitt, ganze Zeile normalfarbig" (bisheriges
        // Verhalten). Position statt nur Laenge, da das rote Wort NICHT
        // zwingend am Zeilenanfang steht (z.B. nach "X Flugzeuge - ...").
        int32_t redStart = 0;
        int32_t redLen = 0;
    };
    InfoMarquee infoMarquee;

    enum class InfoMsgKind { None, TapForDetails, EmptySky };
    InfoMsgKind infoMarqueeKind = InfoMsgKind::None;

    constexpr uint32_t INFO_MARQUEE_STEP_MS = 200; // alle 200ms ein Zeichen weiter

    // Baut Text + Ring-Puffer neu auf und setzt den Scroll-Fortschritt
    // zurueck - nur aufrufen, wenn sich die ART der Nachricht aendert
    // (Tap-Hinweis <-> Leerer-Himmel-Timer), siehe updateInfoMarqueeText()
    // fuer den Fall, dass sich nur der Sekundenwert aendert. redStart/
    // redLen: siehe InfoMarquee oben, Default 0/0 = keine Rotfaerbung.
    void setupInfoMarquee(TFT_eSPI& tft, const String& text, int16_t viewportW,
                           int32_t redStart = 0, int32_t redLen = 0) {
        tft.setTextSize(1);
        infoMarquee.text = text;
        infoMarquee.needsScroll = tft.textWidth(text) > viewportW;
        String withGap = text + "   "; // 3 Leerzeichen Luecke vor der Wiederholung
        infoMarquee.ring = withGap + withGap;
        infoMarquee.charOffset = 0;
        infoMarquee.lastStepMs = millis();
        infoMarquee.redStart = redStart;
        infoMarquee.redLen = redLen;
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
    void updateInfoMarqueeText(TFT_eSPI& tft, const String& text, int16_t viewportW,
                                int32_t redStart = 0, int32_t redLen = 0) {
        tft.setTextSize(1);
        infoMarquee.text = text;
        infoMarquee.needsScroll = tft.textWidth(text) > viewportW;
        String withGap = text + "   ";
        infoMarquee.ring = withGap + withGap;
        infoMarquee.redStart = redStart;
        infoMarquee.redLen = redLen;
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

    // Zeichnet einen (Teil-)Text der Info-Zeile in nach Rot/Normal
    // getrennten Abschnitten - "display" ist der gerade sichtbare String
    // (entweder infoMarquee.text komplett, oder das aktuelle Scroll-
    // Fenster daraus), "baseOffset" die Position des ersten Zeichens von
    // "display" innerhalb des periodischen Textes (0 im Nicht-Scroll-Fall,
    // infoMarquee.charOffset beim Scrollen) und "periodLen" die Laenge
    // einer vollen Wiederholung (text.length() bzw. text.length()+3 fuer
    // die Luecke). Ohne roten Abschnitt (redLen<=0) einfach alles normal.
    void printColoredSegment(TFT_eSPI& tft, const String& display, int32_t baseOffset,
                              int32_t periodLen) {
        if (infoMarquee.redLen <= 0 || periodLen <= 0) {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.print(display);
            return;
        }

        int32_t n = (int32_t)display.length();
        int32_t i = 0;
        while (i < n) {
            int32_t g = (baseOffset + i) % periodLen;
            bool red = g >= infoMarquee.redStart && g < infoMarquee.redStart + infoMarquee.redLen;
            int32_t j = i;
            while (j < n) {
                int32_t gg = (baseOffset + j) % periodLen;
                bool r = gg >= infoMarquee.redStart && gg < infoMarquee.redStart + infoMarquee.redLen;
                if (r != red) break;
                j++;
            }
            tft.setTextColor(red ? TFT_RED : TFT_WHITE, TFT_BLACK);
            tft.print(display.substring(i, j));
            i = j;
        }
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
            printColoredSegment(tft, infoMarquee.text, 0, (int32_t)infoMarquee.text.length());
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

        String window = infoMarqueeWindow(tft, infoMarquee.ring, infoMarquee.charOffset, w);
        printColoredSegment(tft, window, infoMarquee.charOffset, (int32_t)infoMarquee.text.length() + 3);
    }

    bool isEmergencySquawk(const char* squawk) {
        if (!squawk[0]) return false;
        for (uint8_t i = 0; i < Config::EMERGENCY_SQUAWK_COUNT; i++) {
            if (strcmp(squawk, Config::EMERGENCY_SQUAWKS[i]) == 0) return true;
        }
        return false;
    }

    // "Auffaellige Flugzeuge" (oranger Ring) - fuer Militaer-/Regierungs-
    // Rufzeichen-Praefixe gedacht. Bleibt aktuell IMMER false: es gibt
    // nirgends im Projekt eine Config::NOTABLE_CALLSIGN_PREFIXES-Liste -
    // dieses Feature wurde nie tatsaechlich spezifiziert/geliefert (mehrfach
    // in verschiedenen Aenderungswuenschen referenziert, aber die
    // eigentliche Praefixliste kam nie an). "Heavy"-Flugzeuge (ADS-B-
    // Emitter-Kategorie "A5", siehe isHeavyCategory() unten) bekommen bereits
    // eine groessere/fettere Version ihrer Typ-Silhouette (siehe
    // drawTypedMarker()) und sind NICHT von diesem Ring betroffen. Sobald
    // eine echte Praefixliste geliefert wird, hier
    // den Vergleich analog zu isEmergencySquawk() ergaenzen.
    bool isNotableCallsign(const char* callsign) {
        (void)callsign;
        return false;
    }

    // "Militaer-/Behoerdenflug-Erkennung" (SettingsStore::
    // militarySquawkDetectionEnabled(), Menue > System > Radar-Darstellung,
    // AUS per Default) - Bereiche recherchiert (WebSearch, siehe Chat-
    // Verlauf) primaer aus der oeffentlich einsehbaren Wikipedia-Seite
    // "List of transponder codes" sowie einschlaegigen Flugsicherungs-/
    // Hobbyisten-Quellen. AUSDRUECKLICH Best-Effort, NICHT offiziell/
    // vollstaendig/weltweit gueltig - stark US-/NORAD-lastig, da dort am
    // besten oeffentlich dokumentiert:
    //   4400-4477: USA - SR-71/U-2/B-57/Hoehenfluege >FL600, NORAD/ADIZ,
    //              Bundes-Strafverfolgungsbehoerden (FAA Order JO 7110.67)
    //   5000:      NORAD (USA/Kanada) bzw. Australien - militaerische
    //              Einsaetze
    //   5400:      NORAD (USA/Kanada)
    //   6000:      Australien - Militaerfluege im Luftraum G
    //   6100:      NORAD (USA/Kanada)
    //   6400:      NORAD (USA/Kanada)
    //   7501-7577: USA - reserviert fuer die Continental NORAD Region (CONR)
    // Bewusst NICHT enthalten: 7500/7600/7700 (echte Notfall-Squawks, siehe
    // Config::EMERGENCY_SQUAWKS/isEmergencySquawk() oben) - beide Bereiche
    // ueberschneiden sich nicht. Der Hilfetext (MILITARY_SQUAWK_INFO_BODY)
    // kommuniziert die Best-Effort-Natur explizit an den Nutzer.
    struct SquawkRange { uint16_t lo, hi; };
    constexpr SquawkRange MILITARY_SQUAWK_RANGES[] = {
        {4400, 4477},
        {5000, 5000},
        {5400, 5400},
        {6000, 6000},
        {6100, 6100},
        {6400, 6400},
        {7501, 7577},
    };
    constexpr uint8_t MILITARY_SQUAWK_RANGE_COUNT =
        sizeof(MILITARY_SQUAWK_RANGES) / sizeof(MILITARY_SQUAWK_RANGES[0]);

    bool isMilitaryGovSquawk(const char* squawk) {
        if (!squawk[0]) return false;
        // Squawk-Code ist ein 4-stelliger String mit Ziffern 0-7 - fuer den
        // Bereichsvergleich reicht die normale Dezimalinterpretation (die
        // Bereichsgrenzen oben nutzen dieselbe 4-stellige Anzeige-Schreib-
        // weise wie Config::EMERGENCY_SQUAWKS, kein echtes Oktalsystem
        // noetig, da nie eine Ziffer > 7 vorkommt).
        uint16_t val = (uint16_t)atoi(squawk);
        for (uint8_t i = 0; i < MILITARY_SQUAWK_RANGE_COUNT; i++) {
            if (val >= MILITARY_SQUAWK_RANGES[i].lo && val <= MILITARY_SQUAWK_RANGES[i].hi) return true;
        }
        return false;
    }

    bool isHeavyCategory(const char* category) {
        return category[0] == 'A' && category[1] == '5';
    }

    // Flugzeugtyp-Silhouette nach ICAO-Typcode (Aircraft::typeCode, kommt
    // direkt aus dem "t"-Feld von adsb.lol, siehe adsb_client.cpp - wird
    // schon seit Projektbeginn abgefragt/gespeichert, aber bisher nirgends
    // fuer die Radar-Darstellung ausgewertet). Grobe Kategorisierung in
    // Airliner/Privatjet/Turboprop fuer ein eigenes Symbol statt der
    // generischen Pfeilform (siehe drawAirlinerMarker()/
    // drawPrivateJetMarker()/drawTurbopropMarker() und drawTypedMarker()
    // unten). AUSDRUECKLICH Best-Effort, gleiches Prinzip wie die Militaer-/
    // Behoerdenflug-Erkennung (isMilitaryGovSquawk() oben): die Liste deckt
    // die gaengigsten Linienflugzeug-/Business-Jet-/Turboprop-Familien ab
    // (aus oeffentlich bekannten ICAO-Typcode-Referenzen), erhebt aber
    // keinen Anspruch auf Vollstaendigkeit - ein unbekannter/neuer Typcode
    // liefert einfach Unknown und faellt in render()/tick() auf die
    // bisherige generische Pfeilform zurueck (kein Symbol-Zwang). Kostet nur
    // wenige hundert Bytes Flash (kleine Praefix-Tabelle, keine RAM-
    // Belastung), bei aktuell 72% Flash-Auslastung also unkritisch.
    // (TypeSilhouette selbst ist bereits weiter oben bei HitPoint deklariert.)
    struct TypePrefixEntry {
        const char* prefix;
        TypeSilhouette cls;
    };

    // Praefix-Vergleich statt exaktem Vergleich, da eine Herstellerfamilie
    // meist einen gemeinsamen Wortstamm teilt (z.B. alle Boeing-
    // Linienflugzeuge 707-787 beginnen mit "B7", alle Airbus A300-A380 mit
    // "A3") - deutlich kompakter als jede einzelne Variante aufzufuehren.
    // Die Praefixe in dieser Tabelle ueberschneiden sich bewusst NICHT
    // ueber Kategorie-Grenzen hinweg (z.B. Beechcraft-Turboprops "BE20"/
    // "BE30"/"B350" vs. der Beechjet-Privatjet "BE40" - unterscheiden sich
    // bereits an Position 3, kein Ueberlappungsrisiko), die Reihenfolge in
    // der Tabelle ist daher nicht sicherheitsrelevant.
    constexpr TypePrefixEntry TYPE_SILHOUETTE_TABLE[] = {
        // -- Privatjets --
        {"LJ",   TypeSilhouette::PrivateJet},   // Learjet
        {"C25",  TypeSilhouette::PrivateJet},   // Citation CJ1/CJ2/CJ3/CJ4
        {"C5",   TypeSilhouette::PrivateJet},   // Citation 500er-Reihe
        {"C6",   TypeSilhouette::PrivateJet},   // Citation 600er-Reihe
        {"C7",   TypeSilhouette::PrivateJet},   // Citation 700er-Reihe
        {"GLF",  TypeSilhouette::PrivateJet},   // Gulfstream GLF2-GLF6
        {"G280", TypeSilhouette::PrivateJet},
        {"G650", TypeSilhouette::PrivateJet},
        {"H25",  TypeSilhouette::PrivateJet},   // Hawker
        {"BE40", TypeSilhouette::PrivateJet},   // Beechjet 400 / Hawker 400
        {"FA7",  TypeSilhouette::PrivateJet},   // Falcon 7X
        {"FA8",  TypeSilhouette::PrivateJet},   // Falcon 8X
        {"F900", TypeSilhouette::PrivateJet},   // Falcon 900
        {"F2TH", TypeSilhouette::PrivateJet},   // Falcon 2000
        {"CL30", TypeSilhouette::PrivateJet},   // Challenger 300/350
        {"CL60", TypeSilhouette::PrivateJet},   // Challenger 600/604/605/650
        {"GLEX", TypeSilhouette::PrivateJet},   // Global Express
        {"GL5T", TypeSilhouette::PrivateJet},
        {"GL6T", TypeSilhouette::PrivateJet},
        {"E50P", TypeSilhouette::PrivateJet},   // Embraer Phenom 100
        {"E55P", TypeSilhouette::PrivateJet},   // Embraer Phenom 300
        {"PC24", TypeSilhouette::PrivateJet},   // Pilatus PC-24 (Jet, siehe PC12 unten)

        // -- Turboprops --
        {"AT4",  TypeSilhouette::Turboprop},    // ATR 42
        {"AT7",  TypeSilhouette::Turboprop},    // ATR 72
        {"DH8",  TypeSilhouette::Turboprop},    // Dash 8 / Q400
        {"SF34", TypeSilhouette::Turboprop},    // Saab 340
        {"SB20", TypeSilhouette::Turboprop},    // Saab 2000
        {"BE20", TypeSilhouette::Turboprop},    // King Air 200
        {"BE30", TypeSilhouette::Turboprop},    // King Air 300
        {"BE9L", TypeSilhouette::Turboprop},    // King Air 90
        {"B350", TypeSilhouette::Turboprop},    // King Air 350
        {"C208", TypeSilhouette::Turboprop},    // Cessna Caravan
        {"PC12", TypeSilhouette::Turboprop},    // Pilatus PC-12 (Turboprop, siehe PC24 oben)
        {"DHC6", TypeSilhouette::Turboprop},    // De Havilland Twin Otter
        {"SW4",  TypeSilhouette::Turboprop},    // Fairchild Metroliner
        {"F50",  TypeSilhouette::Turboprop},    // Fokker 50
        {"L410", TypeSilhouette::Turboprop},    // Let L-410

        // -- Airliner --
        {"A3",   TypeSilhouette::Airliner},     // Airbus A300-A380
        {"B7",   TypeSilhouette::Airliner},     // Boeing 707-787
        {"MD",   TypeSilhouette::Airliner},     // McDonnell Douglas MD-80/-90
        {"CRJ",  TypeSilhouette::Airliner},     // Bombardier CRJ
        {"E1",   TypeSilhouette::Airliner},     // Embraer E170/175/190/195
        {"E29",  TypeSilhouette::Airliner},     // Embraer E190-E2/E195-E2
        {"SU9",  TypeSilhouette::Airliner},     // Sukhoi Superjet 100
        {"BCS",  TypeSilhouette::Airliner},     // Airbus A220 (ex-Bombardier CSeries)
    };
    constexpr uint8_t TYPE_SILHOUETTE_COUNT =
        sizeof(TYPE_SILHOUETTE_TABLE) / sizeof(TYPE_SILHOUETTE_TABLE[0]);

    // Airbus "Neo"-Familie (New Engine Option) - A319neo/A320neo/A321neo
    // haben eigene ICAO-Typcodes A19N/A20N/A21N (NICHT A319/A320/A321 wie
    // die klassische Generation, siehe TYPE_SILHOUETTE_TABLE-Praefix "A3"
    // oben) - reines Praefix-Matching kann dieses Suffix-Muster nicht
    // abbilden (Befund: fehlten bisher komplett, dadurch faelschlich
    // Unknown/Pfeilform trotz 122 Vorkommen allein im 13.08.-Flugbuch).
    // Deshalb ein eigener, generalisierter Check statt sechs fest
    // eingetragener Einzelcodes: "A" + zwei Ziffern (1x/2x/3x, der
    // realistische Airbus-Einstrahlbereich) + "N" - deckt automatisch auch
    // noch nicht existierende kuenftige Neo-Varianten derselben Systematik
    // ab (z.B. ein hypothetisches "A22N"), ohne beliebige andere
    // vierstellige "...N"-Codes zu treffen (kein bekannter Turboprop-/
    // Privatjet-/Helikopter-Code in dieser Tabelle endet auf "N").
    bool isAirbusNeoCode(const char* t) {
        return t[0] == 'A' && t[1] >= '1' && t[1] <= '3' &&
               t[2] >= '0' && t[2] <= '9' && t[3] == 'N' && t[4] == '\0';
    }

    // Boeing 737-MAX-Familie - B37M/B38M/B39M (Ziffer = Baureihe -7/-8/-9,
    // Suffix "M" fuer "MAX", analog zur Neo-Systematik oben - die
    // klassische 737-Generation nutzt weiterhin B737/B738/B739, gedeckt
    // durch den bestehenden Praefix "B7"). "B3" + eine Ziffer + "M" deckt
    // ebenfalls kuenftige Baureihen derselben Systematik ab, ohne z.B. den
    // Turboprop "B350" (King Air 350, siehe TYPE_SILHOUETTE_TABLE oben) zu
    // treffen - der endet auf "0", nicht auf "M".
    bool isBoeingMaxCode(const char* t) {
        return t[0] == 'B' && t[1] == '3' && t[2] >= '0' && t[2] <= '9' &&
               t[3] == 'M' && t[4] == '\0';
    }

    TypeSilhouette classifyTypeSilhouette(const char* typeCode) {
        if (!typeCode[0]) return TypeSilhouette::Unknown;
        size_t len = strlen(typeCode);
        // Neo/MAX-Sondercheck zuerst - beide sind eindeutig 4-stellige
        // Codes mit eigenem Suffix-Muster, ueberschneiden sich nicht mit
        // der Praefix-Tabelle unten (siehe Kommentare bei den beiden
        // Funktionen oben).
        if (len == 4 && (isAirbusNeoCode(typeCode) || isBoeingMaxCode(typeCode))) {
            return TypeSilhouette::Airliner;
        }
        for (uint8_t i = 0; i < TYPE_SILHOUETTE_COUNT; i++) {
            size_t plen = strlen(TYPE_SILHOUETTE_TABLE[i].prefix);
            if (len >= plen && strncmp(typeCode, TYPE_SILHOUETTE_TABLE[i].prefix, plen) == 0) {
                return TYPE_SILHOUETTE_TABLE[i].cls;
            }
        }
        return TypeSilhouette::Unknown;
    }

    float sweepAngleDeg = 0.0f;
    float prevSweepAngleDeg = -1.0f;
    constexpr float SWEEP_DEGREES_PER_SEC = 45.0f;

    // Volle Umdrehung in ms - Fade-Dauer des CRT-Phosphor-Effekts (siehe
    // crtPhosphorColor() unten), an SWEEP_DEGREES_PER_SEC gekoppelt statt
    // fest verdrahtet, damit beides zwangslaeufig synchron bleibt.
    constexpr uint32_t CRT_FADE_MS = (uint32_t)(360.0f / SWEEP_DEGREES_PER_SEC * 1000.0f);

    // Ankreuzbares Extra "Radar-Puls" (SettingsStore::radarPulseEnabled(),
    // siehe radar_theme_screen.cpp) - ein auslaufender Ring vom Radar-
    // Zentrum aus bei jedem frischen ADS-B-Datenabruf, ausgeloest in
    // render() ueber einen Versionsvergleich (siehe lastPulseVersion dort)
    // und animiert in tick() ueber PULSE_DURATION_MS.
    bool pulseActive = false;
    uint32_t pulseStartMs = 0;
    // -1 = im letzten Tick kein Ring gezeichnet, nichts zu loeschen.
    int16_t prevPulseRadius = -1;
    constexpr uint32_t PULSE_DURATION_MS = 1400;
    // Reicht ueber den reinen Radar-Kreis hinaus Richtung Bildschirmrand,
    // ohne die exakten Bildschirmecken auszurechnen (Alex' Wunsch: "ca. das
    // 1.4-fache des Radar-Kreis-Radius").
    constexpr float PULSE_MAX_RADIUS_FACTOR = 1.4f;

    // Merkt sich die zuletzt in render() gesehene AircraftTable::version() -
    // NICHT bei jedem render()-Aufruf pulsen, da viele Aufrufe nur durch
    // UI-bedingtes forceRedraw kommen (siehe main.cpp), nicht durch echte
    // neue Daten. Startwert absichtlich 0xFFFFFFFF (statt UINT32_MAX, um
    // nicht von <cstdint> abhaengig zu sein), damit der allererste
    // render()-Aufruf ebenfalls als "neue Daten" zaehlt.
    uint32_t lastPulseVersion = 0xFFFFFFFFu;

    // "Klassik-Radar", Teil 3: Sonar-Ping bei neuem Kontakt - ein heller
    // Ring, der LOKAL an der Bildschirmposition eines Flugzeugs expandiert
    // und ausfadet, sobald dessen hex-ID zum allerersten Mal (bzw. nach
    // laengerer Abwesenheit, siehe SONAR_KNOWN_GRACE_MS unten)
    // erscheint - im Unterschied zum bestehenden Radar-Puls (der immer vom
    // Zentrum aus kommt und bei JEDER Datenaenderung ausgeloest wird, siehe
    // pulseActive oben). Bis zu MAX_SONAR_PINGS gleichzeitige Ringe, falls
    // mehrere Flugzeuge im selben render()-Durchlauf neu auftauchen.
    struct SonarPing {
        bool active = false;
        int16_t cx = 0, cy = 0;
        uint16_t color = TFT_GREEN;
        uint32_t startMs = 0;
        // -1 = im letzten Tick kein Ring gezeichnet, nichts zu loeschen -
        // gleiches Prinzip wie prevPulseRadius oben.
        int16_t prevRadius = -1;
    };
    constexpr uint8_t MAX_SONAR_PINGS = 6;
    SonarPing sonarPings[MAX_SONAR_PINGS];
    constexpr uint32_t SONAR_PING_DURATION_MS = 900;
    // Bewusst klein (lokaler Effekt am Flugzeug, kein bildschirmfuellender
    // Ring wie beim Radar-Puls) - jeder Punkt wird trotzdem zusaetzlich auf
    // den Radar-Kreis geclippt (siehe tick()), damit ein Ping nahe am
    // Kreisrand nicht in Kopf-/Fusszeile hineinmalt.
    constexpr int16_t SONAR_PING_MAX_RADIUS = 18;

    void triggerSonarPing(int16_t cx, int16_t cy, uint16_t color) {
        for (uint8_t i = 0; i < MAX_SONAR_PINGS; i++) {
            if (!sonarPings[i].active) {
                sonarPings[i] = SonarPing{};
                sonarPings[i].active = true;
                sonarPings[i].cx = cx;
                sonarPings[i].cy = cy;
                sonarPings[i].color = color;
                sonarPings[i].startMs = millis();
                return;
            }
        }
        // Alle Slots belegt (in der Praxis kaum je der Fall) - ein einzelner
        // verpasster Ping ist unkritisch, kein Ueberschreiben eines noch
        // laufenden Rings.
    }

    // "Klassik-Radar", Teil 4: Signal-Rauschen bei neuem Kontakt - ein paar
    // zufaellige, kurz aufblitzende Einzelpixel in unmittelbarer Naehe der
    // neuen Flugzeugposition (deutlich kleinerer Streuradius als der
    // Sonar-Ping-Ring, STATIC_NOISE_SPREAD statt SONAR_PING_MAX_RADIUS), die
    // nach sehr kurzer Zeit wieder komplett verschwinden. Ausgeloest vom
    // GLEICHEN "neuer Kontakt"-Ereignis wie der Sonar-Ping (siehe Aufrufer
    // in render()), laeuft aber als eigener, unabhaengiger Effekt daneben -
    // beide koennen gleichzeitig aktiv sein.
    constexpr uint8_t STATIC_NOISE_POINT_COUNT = 6;
    struct StaticNoise {
        bool active = false;
        int16_t cx = 0, cy = 0;
        uint32_t startMs = 0;
        uint16_t color = TFT_WHITE;
        // Tatsaechlich gezeichnete Punkte des LETZTEN Ticks (nach Clipping,
        // siehe tick()) - fuer sauberes Loeschen im naechsten Tick, gleiches
        // Erase/Redraw-Prinzip wie beim Sonar-Ping/Radar-Puls. Punkte werden
        // JEDEN Tick neu zufaellig gewuerfelt (kein starres Muster), fuer
        // einen echten Flacker-/"Static"-Effekt.
        uint8_t prevPointCount = 0;
        int16_t prevPointsX[STATIC_NOISE_POINT_COUNT];
        int16_t prevPointsY[STATIC_NOISE_POINT_COUNT];
    };
    constexpr uint8_t MAX_STATIC_NOISE = 6;
    StaticNoise staticNoises[MAX_STATIC_NOISE];
    // 300ms liegt in der von Alex vorgegebenen Bandbreite (200-400ms).
    constexpr uint32_t STATIC_NOISE_DURATION_MS = 300;
    // Bewusst deutlich kleiner als SONAR_PING_MAX_RADIUS (18px) - "nur
    // wenige Pixel Streuung", wie von Alex gewuenscht.
    constexpr int16_t STATIC_NOISE_SPREAD = 5;

    void triggerStaticNoise(int16_t cx, int16_t cy) {
        for (uint8_t i = 0; i < MAX_STATIC_NOISE; i++) {
            if (!staticNoises[i].active) {
                staticNoises[i] = StaticNoise{};
                staticNoises[i].active = true;
                staticNoises[i].cx = cx;
                staticNoises[i].cy = cy;
                staticNoises[i].startMs = millis();
                return;
            }
        }
        // Alle Slots belegt - ein einzelner verpasster Effekt ist unkritisch.
    }

    // Animierter Regen-Effekt (testweise IMMER aktiv, kein Schalter - Alex'
    // ausdruecklicher Wunsch, ueber ein Ein/Aus wird erst nach diesem Test
    // entschieden) - mehrere kurze, schraege Linien ("Tropfen"), die
    // kontinuierlich als parallele Sehnen ueber den Radarkreis wandern.
    // Neigungswinkel = Weather::currentWindDirectionDeg() (0=Nord, im
    // Uhrzeigersinn - gleiche Konvention wie die Sweep-Linie). Nur aktiv,
    // wenn Weather::current() Regen/Gewitter zeigt.
    struct RainDrop {
        float x = 0, y = 0;
        float vx = 0, vy = 0; // Einheitsvektor der Bewegungsrichtung
        bool active = false;
        // Fuer sauberes Loeschen im naechsten Tick, gleiches Erase/Redraw-
        // Prinzip wie Sonar-Ping/Radar-Puls oben.
        bool hasPrev = false;
        int16_t prevX1 = 0, prevY1 = 0, prevX2 = 0, prevY2 = 0;
    };
    // Bewusst wenige Tropfen ("nicht zu viele", Alex' Wunsch) - klar
    // erkennbare Einzellinien statt dichtem Gewusel.
    constexpr uint8_t MAX_RAINDROPS = 8;
    RainDrop rainDrops[MAX_RAINDROPS];
    constexpr float RAIN_DROP_SPEED_PX_PER_SEC = 70.0f;
    constexpr int16_t RAIN_DROP_LENGTH = 7;

    // Setzt einen Tropfen an einer zufaelligen Position auf dem Kreisrand
    // neu, gestreut um die Windrichtung-Bearing (±70 Grad) - Bewegung
    // verlaeuft als parallele Sehne quer durch den Kreis in Richtung
    // "windDirDeg + 180". Alle Tropfen bewegen sich parallel (gleicher
    // Bewegungsvektor), nur der Startpunkt auf dem Rand variiert - das
    // ergibt sichtbar "fallenden" Regen statt zufaellig verstreuter Linien.
    void spawnRainDrop(RainDrop& d, const Layout& L, float windDirDeg) {
        double entrySpreadDeg = (double)random(-70, 71);
        double entryAngleRad = ((double)windDirDeg + entrySpreadDeg) * DEG_TO_RAD;
        d.x = L.cx + (float)(L.radius * sin(entryAngleRad));
        d.y = L.cy - (float)(L.radius * cos(entryAngleRad));
        double travelRad = ((double)windDirDeg + 180.0) * DEG_TO_RAD;
        d.vx = (float)sin(travelRad);
        d.vy = -(float)cos(travelRad);
        d.active = true;
        d.hasPrev = false;
    }

    // "Bekannt seit"-Tracking fuer den Sonar-Ping, UNABHAENGIG von
    // phosphorEntries[] oben: phosphorEntries wird bei JEDEM render()-
    // Durchlauf, in dem ein Flugzeug nicht mehr in der Snapshot-Liste
    // auftaucht, sofort freigegeben (siehe Kommentar dort) - ein einziger
    // verpasster Fetch-Zyklus am Rand der Reichweite wuerde dort schon
    // reichen, um beim naechsten Auftauchen faelschlich als "neu" zu gelten.
    // Fuer den Sonar-Ping ist das zu empfindlich (Ping-Spam-Risiko, siehe
    // Alex' Nachfrage) - dieses Array behaelt einen Eintrag stattdessen ueber
    // SONAR_KNOWN_GRACE_MS hinweg, auch wenn das Flugzeug zwischenzeitlich
    // nicht im Snapshot war. Erst wenn seit dem letzten Sichten mehr als die
    // Gnadenfrist vergangen ist, zaehlt ein Wiederauftauchen erneut als
    // "neuer Kontakt".
    struct KnownAircraftEntry {
        char hex[7] = {0};
        bool used = false;
        uint32_t lastSeenMs = 0;
    };
    constexpr uint8_t MAX_KNOWN_AIRCRAFT = MAX_HIT_POINTS;
    KnownAircraftEntry knownAircraft[MAX_KNOWN_AIRCRAFT];
    // 45s = mehrere FETCH_INTERVAL_MS-Zyklen (Config::FETCH_INTERVAL_MS =
    // 10s) Puffer gegen kurze Signalaussetzer/Randschwankungen der
    // Reichweite, ohne ein tatsaechlich abgeflogenes und Stunden spaeter
    // zurueckkehrendes Flugzeug dauerhaft als "schon bekannt" zu behandeln.
    constexpr uint32_t SONAR_KNOWN_GRACE_MS = 45000;

    // Liefert true, wenn "hex" als neuer Kontakt zaehlt (noch nie gesehen,
    // ODER seit mehr als SONAR_KNOWN_GRACE_MS nicht mehr gesehen) - und
    // aktualisiert/erstellt in JEDEM Fall den Eintrag mit dem aktuellen
    // Zeitstempel. Wird bewusst bei JEDEM sichtbaren Flugzeug aufgerufen,
    // unabhaengig vom Klassik-Radar-Schalter (siehe Aufrufer in render()) -
    // sonst wuerde ein Einschalten des Schalters faelschlich fuer bereits
    // laengst bekannte Flugzeuge einen Ping ausloesen.
    bool isNewContactAndTouch(const char* hex, uint32_t nowMs) {
        int8_t freeIdx = -1;
        int8_t staleIdx = -1;
        uint32_t staleAge = 0;
        for (uint8_t i = 0; i < MAX_KNOWN_AIRCRAFT; i++) {
            if (knownAircraft[i].used && strcmp(knownAircraft[i].hex, hex) == 0) {
                bool graceExpired = (nowMs - knownAircraft[i].lastSeenMs) > SONAR_KNOWN_GRACE_MS;
                knownAircraft[i].lastSeenMs = nowMs;
                return graceExpired;
            }
            if (!knownAircraft[i].used) {
                if (freeIdx < 0) freeIdx = i;
                continue;
            }
            uint32_t age = nowMs - knownAircraft[i].lastSeenMs;
            if (age > SONAR_KNOWN_GRACE_MS && age > staleAge) {
                staleAge = age;
                staleIdx = i;
            }
        }
        // Kein bestehender Eintrag gefunden - freien Slot nehmen, sonst den
        // laengst abgelaufenen (aeltesten, > Gnadenfrist) Eintrag wieder
        // verwenden (einfaches LRU), damit das feste Array auch bei vielen
        // unterschiedlichen Flugzeugen ueber eine laengere Sitzung hinweg
        // nicht "voll" bleibt.
        int8_t idx = freeIdx >= 0 ? freeIdx : staleIdx;
        if (idx < 0) return false; // Kapazitaet voll mit aktuell/kuerzlich gesehenen Flugzeugen - kein Ping
        knownAircraft[idx] = KnownAircraftEntry{};
        knownAircraft[idx].used = true;
        strncpy(knownAircraft[idx].hex, hex, sizeof(knownAircraft[idx].hex) - 1);
        knownAircraft[idx].lastSeenMs = nowMs;
        return true;
    }

    // Gleiche Logik wie main.cpp::isNightDimHours() - Nacht = zwischen
    // Sonnenuntergang und Sonnenaufgang am aktiven Standort (siehe
    // sun_times.h), mit Rueckfall auf ein festes 22:00-06:00-Fenster,
    // solange Standort/Uhrzeit noch nicht bekannt sind. Hier bewusst
    // dupliziert statt geteilt, siehe CLAUDE.md-Konvention ("jeder Screen
    // unabhaengig lauffaehig, kein gemeinsames Modul fuer solche
    // Kleinigkeiten").
    bool isNightHours() {
        time_t now = time(nullptr);
        if (now <= 8 * 3600 * 2) return false; // Uhrzeit noch nicht per NTP synchronisiert

        struct tm tmNow;
        localtime_r(&now, &tmNow);

        double lat = 0, lon = 0;
        LocationManager::getHomeLocation(lat, lon);
        if (lat != 0.0 || lon != 0.0) {
            SunTimes::Result sun = SunTimes::compute(lat, lon, tmNow.tm_year + 1900, tmNow.tm_mon + 1,
                                                      tmNow.tm_mday, LocationManager::utcOffsetSeconds());
            if (sun.valid) {
                if (sun.alwaysDay) return false;
                if (sun.alwaysNight) return true;
                float hourNow = tmNow.tm_hour + tmNow.tm_min / 60.0f;
                return (hourNow < sun.sunriseHour) || (hourNow >= sun.sunsetHour);
            }
        }

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

    // Radar-Farbschema (Menue > System > Radar-Farbschema, siehe
    // radar_theme_screen.cpp / SettingsStore::radarThemeIndex()) - retro
    // Phosphor-Radarschirm-Optik (Gruen/Amber/Blau). Frueher NUR fuer den
    // Radar-Screen selbst gedacht, mittlerweile per UiTheme::accentColor()
    // (ui_theme.h/.cpp) auf das GESAMTE Projekt ausgeweitet (Alex'
    // ausdruecklicher Wunsch) - alle Menues/Buttons/Rahmen/Schriften folgen
    // jetzt demselben Farbschema. themeBaseColor() bleibt als duenner
    // Wrapper bestehen (viele Aufrufer hier in dieser Datei), delegiert
    // aber an dieselbe zentrale Funktion, damit es nur EINE Stelle mit der
    // eigentlichen Gruen/Amber/Blau-Zuordnung gibt.
    //
    // AUSGENOMMEN bleibt weiterhin die niedrigste Hoehenstufe in
    // colorForAltitude() unten - die ist jetzt bewusst IMMER TFT_GREEN,
    // unabhaengig vom Thema (frueher folgte sie themeBaseColor(), siehe
    // Git-Historie - auf Alex' ausdruecklichen Wunsch zurueckgestellt,
    // damit die drei Hoehenfarben als Gruppe immer eindeutig bleiben und
    // nicht mit dem UI-Thema verwechselt werden). Gelb/Rot (mittlere/hohe
    // Flughoehe) waren ohnehin nie themenabhaengig.
    uint16_t themeBaseColor(TFT_eSPI& gfx) {
        return UiTheme::accentColor(gfx);
    }

    // Gedaempfte Variante von themeBaseColor() fuer die Nachtdimmung -
    // gleiches Helligkeitsverhaeltnis wie das bisherige TFT_GREEN->
    // color565(0,160,0) fuer die anderen beiden Farbschemata nachgebildet.
    uint16_t themeDimColor(TFT_eSPI& gfx) {
        switch (SettingsStore::radarThemeIndex()) {
            case 1: return gfx.color565(160, 110, 0);  // Amber, gedaempft
            case 2: return gfx.color565(0, 120, 160);  // Blau, gedaempft
            default: return gfx.color565(0, 160, 0);    // Gruen, gedaempft
        }
    }

    // Farbe je nach Flughoehe - gedaempft (dunklerer Grundton) waehrend der
    // Nachtdimmung.
    //
    // Die rote Hoehenstufe wird bewusst NICHT gedaempft: der bisherige
    // Dimm-Ton (160,30,0) sah auf dem Display eher orange-braun statt rot
    // aus und war dadurch nicht mehr klar von der gelben Stufe zu
    // unterscheiden. Rot bleibt deshalb Tag und Nacht der reine TFT_RED-Ton.
    // Ankreuzbares Extra "CRT-Phosphor" (SettingsStore::crtPhosphorEnabled(),
    // siehe radar_theme_screen.cpp) - faedet JEDEN Marker (alle Hoehen-
    // Farbkategorien plus Bodenfahrzeuge) in seiner eigenen Farbe aus,
    // siehe crtPhosphorColor().
    bool crtModeActive() {
        return SettingsStore::crtPhosphorEnabled();
    }

    // Skaliert eine RGB565-Farbe gleichmaessig auf allen drei Kanälen
    // (0.0 = schwarz, 1.0 = unveraendert) - generischer Helper fuer den
    // CRT-Phosphor-Fade UND den Radar-Puls-Ring (siehe unten), damit beide
    // Effekte die jeweils aktive Themefarbe respektieren statt eine fest
    // codierte Farbe zu verwenden.
    uint16_t scaleColorBrightness(uint16_t color565, float fraction) {
        if (fraction < 0.0f) fraction = 0.0f;
        if (fraction > 1.0f) fraction = 1.0f;
        uint16_t r5 = (color565 >> 11) & 0x1F;
        uint16_t g6 = (color565 >> 5) & 0x3F;
        uint16_t b5 = color565 & 0x1F;
        r5 = (uint16_t)(r5 * fraction + 0.5f);
        g6 = (uint16_t)(g6 * fraction + 0.5f);
        b5 = (uint16_t)(b5 * fraction + 0.5f);
        return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
    }

    // Helligkeit des CRT-Phosphor-Markers zum Zeitpunkt "nowMs": TFT_BLACK
    // (unsichtbar), solange der Sweep-Strahl das Flugzeug noch nie erfasst
    // hat (ph == nullptr oder everSwept == false) - taucht ein Flugzeug neu
    // auf, bleibt es also bis zum naechsten Strahl-Treffer verborgen. Danach
    // faedet die Farbe ueber eine volle Umdrehung (CRT_FADE_MS) linear von
    // "baseColor" (voll) auf ein dunkles Minimum (~40/255 = "fast
    // verblasst") aus. WICHTIG (vorher ein Bug): baseColor ist jetzt die
    // TATSAECHLICHE Marker-Farbe der jeweiligen Kategorie (Gruen/Gelb/Rot
    // per colorForAltitude(), Blau per colorForGroundVehicle()) statt fest
    // der Radar-Theme-Grundfarbe - vorher faedete diese Funktion IMMER nur
    // die Theme-Farbe (Gruen/Amber/Blau je nach Farbschema), was zufaellig
    // wie "nur die gruenen Marker faden" aussah, weil colorForAltitude() bei
    // niedriger Flughoehe ebenfalls die Theme-Farbe liefert - gelbe/rote
    // Flugzeuge und blaue Bodenfahrzeug-Marker wurden dabei schlicht nie mit
    // ihrer EIGENEN Farbe gefaedet.
    uint16_t crtPhosphorColor(uint16_t baseColor, const PhosphorEntry* ph, uint32_t nowMs) {
        if (!ph || !ph->everSwept) return TFT_BLACK;
        uint32_t elapsed = nowMs - ph->lastSweptMs;
        float fraction = (float)elapsed / (float)CRT_FADE_MS;
        if (fraction > 1.0f) fraction = 1.0f;
        constexpr float MIN_BRIGHTNESS_FRACTION = 40.0f / 255.0f;
        float brightnessFraction = 1.0f - fraction * (1.0f - MIN_BRIGHTNESS_FRACTION);
        return scaleColorBrightness(baseColor, brightnessFraction);
    }

    // Bogen-Test mit 360/0-Wrap-Behandlung: ist "target" (eine Peilung in
    // Grad) beim Fortschreiten des Sweep-Strahls von oldAngle zu newAngle
    // ueberstrichen worden? Fuer den CRT-Phosphor-Effekt in tick().
    bool sweepCrossedBearing(float oldAngle, float newAngle, float target) {
        if (newAngle >= oldAngle) {
            return target >= oldAngle && target < newAngle;
        }
        // Sweep ist ueber 360/0 gewrappt (z.B. oldAngle=350, newAngle=8).
        return target >= oldAngle || target < newAngle;
    }

    uint16_t colorForAltitude(TFT_eSPI& gfx, int32_t altFt) {
        bool dim = nightDimActiveNow();
        // Niedrigste Hoehenstufe ist bewusst IMMER Gruen, unabhaengig vom
        // gewaehlten Radar-Farbschema (siehe Kommentar bei themeBaseColor()
        // oben) - Alex' ausdruecklicher Wunsch, damit die drei Hoehenfarben
        // als Gruppe eindeutig bleiben und nicht mit dem UI-Thema
        // verwechselt werden.
        if (altFt < Config::COLOR_LOW_ALT_THRESHOLD_FT)
            return dim ? gfx.color565(0, 160, 0) : TFT_GREEN;
        if (altFt < Config::COLOR_MID_ALT_THRESHOLD_FT)
            return dim ? gfx.color565(160, 160, 0) : TFT_YELLOW;
        return TFT_RED;
    }

    uint16_t sweepLineColor(TFT_eSPI& gfx) {
        return nightDimActiveNow() ? themeDimColor(gfx) : themeBaseColor(gfx);
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

    // Ordnet eine Peilung (0-360, 0=Nord, im Uhrzeigersinn, siehe
    // RadarMath::PolarCoord) der ueblichen 8-Punkte-Kompass-Abkuerzung zu
    // (N/NO/O/SO/S/SW/W/NW im Deutschen, siehe COMPASS_*-Eintraege in
    // i18n.h - jede Sprache hat eigene Abkuerzungen). Fuer die textuelle
    // Peilungsanzeige im Detail-Panel (siehe drawDetailPanel() unten) -
    // ergaenzt die bereits vorhandene grafische Anzeige (drawBearingIndicator()
    // oben) um eine auf einen Blick lesbare Himmelsrichtung.
    const char* compassLabel(float bearingDeg) {
        // Jeder der 8 Sektoren ist 45 Grad breit, zentriert auf die jeweilige
        // Haupt-/Zwischenrichtung (z.B. Nord = 337.5-22.5 Grad) - +22.5 und
        // Ganzzahldivision durch 45 bildet das ohne Sonderfall fuer den
        // Nord-Uebergang bei 0/360 Grad ab.
        int sector = ((int)lround(bearingDeg + 22.5f) / 45) % 8;
        if (sector < 0) sector += 8;
        switch (sector) {
            case 0: return I18n::t(StringId::COMPASS_N);
            case 1: return I18n::t(StringId::COMPASS_NE);
            case 2: return I18n::t(StringId::COMPASS_E);
            case 3: return I18n::t(StringId::COMPASS_SE);
            case 4: return I18n::t(StringId::COMPASS_S);
            case 5: return I18n::t(StringId::COMPASS_SW);
            case 6: return I18n::t(StringId::COMPASS_W);
            default: return I18n::t(StringId::COMPASS_NW);
        }
    }

    // Zeichnet eine Linie "fett" durch zusaetzliche, um je 1px versetzte
    // Parallel-Linien (halfWidth Stufen auf JEDER Seite der Mittellinie,
    // also insgesamt (2*halfWidth+1) Linien, effektive Breite ca.
    // (2*halfWidth+1)px) - bewusst NICHT TFT_eSPI::drawWideLine() (das
    // arbeitet mit Anti-Aliasing/Hintergrundmischung, was beim bestehenden
    // Loesch-per-TFT_BLACK-Ueberschreiben-Muster dieser Datei helle
    // Restpixel haette hinterlassen koennen, siehe tick()) - stattdessen
    // dieselbe einfache, AA-freie drawLine()-Technik wie ueberall sonst in
    // dieser Datei, nur additiv mehrfach versetzt. Fuer ALLE Typ-
    // Silhouetten genutzt (siehe drawTypedMarker() unten, halfWidth=1 im
    // Normalfall, halfWidth=2 bei "Heavy") - Alex' Wunsch nach spuerbar
    // kraeftigeren Linien ("von 1px auf 2px") galt fuer alle Kategorien,
    // nicht nur Heavy.
    void drawThickLine(TFT_eSPI& gfx, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color,
                        uint8_t halfWidth) {
        gfx.drawLine(x1, y1, x2, y2, color);
        if (halfWidth == 0) return;
        float ldx = (float)(x2 - x1);
        float ldy = (float)(y2 - y1);
        float len = sqrtf(ldx * ldx + ldy * ldy);
        if (len < 0.5f) return;
        float ux = -ldy / len;
        float uy = ldx / len;
        for (uint8_t i = 1; i <= halfWidth; i++) {
            int16_t ox = (int16_t)lroundf(ux * i);
            int16_t oy = (int16_t)lroundf(uy * i);
            if (ox == 0 && oy == 0) continue;
            gfx.drawLine((int16_t)(x1 + ox), (int16_t)(y1 + oy), (int16_t)(x2 + ox), (int16_t)(y2 + oy), color);
            gfx.drawLine((int16_t)(x1 - ox), (int16_t)(y1 - oy), (int16_t)(x2 - ox), (int16_t)(y2 - oy), color);
        }
    }

    // Generische Flugzeug-Silhouette - Fallback, wenn der ICAO-Typcode
    // unbekannt/nicht zuordenbar ist (classifyTypeSilhouette() liefert
    // Unknown, siehe drawTypedMarker() unten). Gleicher Aufbau wie
    // drawAirlinerMarker() (Rumpflinie + durchgehender Tragflaechenstrich),
    // bewusst NEUTRAL gehalten, da der genaue Typ ja nicht bekannt ist -
    // ersetzt die fruehere Kreis+Pfeilkopf-Form, die laut Karls Analyse bei
    // der kleinen Displaygroesse selbst wie ein Blob wirkte (keine
    // Kreisfuellung mehr, konsequent nur noch Linien wie bei den anderen
    // Typ-Silhouetten). Masse (Stand nach Alex' "deutlich groesser/
    // kraeftiger"-Wunsch): normal 9/7/8px (Nase/Heck/Fluegel), halfWidth=1
    // (~3px Linienstaerke); heavy=true (Heavy-Kategorie mit unbekanntem
    // Typcode) 12/9/10px, halfWidth=2 (~5px Linienstaerke).
    void drawAircraftMarker(TFT_eSPI& gfx, int16_t x, int16_t y, float headingDeg, uint16_t color, bool heavy) {
        double rad = headingDeg * PI / 180.0;
        int16_t noseLen = heavy ? 12 : 9;
        int16_t tailLen = heavy ? 9 : 7;
        int16_t wingLen = heavy ? 10 : 8;
        uint8_t halfWidth = heavy ? 2 : 1;
        int16_t noseX = x + (int16_t)(sin(rad) * noseLen);
        int16_t noseY = y - (int16_t)(cos(rad) * noseLen);
        int16_t tailX = x - (int16_t)(sin(rad) * tailLen);
        int16_t tailY = y + (int16_t)(cos(rad) * tailLen);
        drawThickLine(gfx, tailX, tailY, noseX, noseY, color, halfWidth);

        double wingRad = rad + PI / 2.0;
        int16_t w1x = x + (int16_t)(sin(wingRad) * wingLen);
        int16_t w1y = y - (int16_t)(cos(wingRad) * wingLen);
        int16_t w2x = x - (int16_t)(sin(wingRad) * wingLen);
        int16_t w2y = y + (int16_t)(cos(wingRad) * wingLen);
        drawThickLine(gfx, w1x, w1y, w2x, w2y, color, halfWidth);
    }

    // Eigener Marker fuer Bodenfahrzeuge (ADS-B-Kategorie "C*") - ein
    // gefuelltes Quadrat statt Kreis+Pfeilkopf, damit sie auf dem Radar
    // klar von echten Flugzeugen zu unterscheiden sind (Heading/Kurs ist
    // bei Bodenfahrzeugen ausserdem meist nicht aussagekraeftig).
    void drawGroundVehicleMarker(TFT_eSPI& gfx, int16_t x, int16_t y, uint16_t color) {
        constexpr int16_t HALF = 4;
        gfx.fillRect((int16_t)(x - HALF), (int16_t)(y - HALF), (int16_t)(2 * HALF), (int16_t)(2 * HALF), color);
    }

    // Eigener Marker fuer Hubschrauber (ADS-B-Emitter-Kategorie "A7" =
    // Rotorcraft) - ein gefuellter Kreis mit durchgehendem Rotorkreuz statt
    // Pfeilkopf, da Hubschrauber im Schwebeflug keinen aussagekraeftigen
    // "nach vorne"-Kurs wie ein Flugzeug haben (gleiche Ueberlegung wie beim
    // Bodenfahrzeug-Marker, der aus demselben Grund ebenfalls ohne
    // Kurslinie auskommt).
    void drawHelicopterMarker(TFT_eSPI& gfx, int16_t x, int16_t y, uint16_t color) {
        constexpr int16_t ROTOR_LEN = 8;
        gfx.fillCircle(x, y, 4, color);
        gfx.drawLine((int16_t)(x - ROTOR_LEN), y, (int16_t)(x + ROTOR_LEN), y, color);
        gfx.drawLine(x, (int16_t)(y - ROTOR_LEN), x, (int16_t)(y + ROTOR_LEN), color);
    }

    // Airliner-Silhouette (siehe classifyTypeSilhouette() oben) - gerade
    // Rumpflinie (Bug/Heck) plus ein durchgehender Tragflaechenstrich
    // mittig, quer zur Flugrichtung. Ersetzt drawAircraftMarker() NUR, wenn
    // der Typcode einer Kategorie zugeordnet werden konnte (siehe
    // drawTypedMarker() unten). Masse (Stand nach Alex' "deutlich groesser/
    // kraeftiger"-Wunsch, vorher 7/5/6px bei 1px Linienstaerke): normal
    // 9/7/8px (Nase/Heck/Fluegel), halfWidth=1 (~3px Linienstaerke) - das
    // entspricht +29%/+40%/+33% gegenueber der vorherigen Groesse. heavy=true
    // (ADS-B-Emitter-Kategorie "A5", siehe isHeavyCategory()) 12/9/10px,
    // halfWidth=2 (~5px Linienstaerke) - ersetzt den frueheren separaten
    // Kreis-Marker drawHeavyMarker(), der laut Alex' Wunsch ("Alles oder
    // nichts", kein Kreis/Blob mehr) komplett entfallen ist.
    void drawAirlinerMarker(TFT_eSPI& gfx, int16_t x, int16_t y, float headingDeg, uint16_t color, bool heavy) {
        double rad = headingDeg * PI / 180.0;
        int16_t noseLen = heavy ? 12 : 9;
        int16_t tailLen = heavy ? 9 : 7;
        int16_t wingLen = heavy ? 10 : 8;
        uint8_t halfWidth = heavy ? 2 : 1;
        int16_t noseX = x + (int16_t)(sin(rad) * noseLen);
        int16_t noseY = y - (int16_t)(cos(rad) * noseLen);
        int16_t tailX = x - (int16_t)(sin(rad) * tailLen);
        int16_t tailY = y + (int16_t)(cos(rad) * tailLen);
        drawThickLine(gfx, tailX, tailY, noseX, noseY, color, halfWidth);

        double wingRad = rad + PI / 2.0;
        int16_t w1x = x + (int16_t)(sin(wingRad) * wingLen);
        int16_t w1y = y - (int16_t)(cos(wingRad) * wingLen);
        int16_t w2x = x - (int16_t)(sin(wingRad) * wingLen);
        int16_t w2y = y + (int16_t)(cos(wingRad) * wingLen);
        drawThickLine(gfx, w1x, w1y, w2x, w2y, color, halfWidth);
    }

    // Privatjet-Silhouette - gleicher Aufbau wie drawAirlinerMarker(), aber
    // kompakter/kuerzer proportioniert (kleinere Rumpf-/Tragflaechenlaenge),
    // wie von Alex gewuenscht. Masse: normal 7/5/6px (vorher 5/3/4px,
    // +40%/+67%/+50%), halfWidth=1 - bleibt damit bewusst durchgehend
    // kleiner als der neue Airliner-Normalwert (9/7/8px), die relative
    // Groessenordnung zwischen den Kategorien bleibt erhalten. heavy=true
    // 10/7/8px, halfWidth=2 (in der Praxis selten, da echte Privatjets kaum
    // je die "A5"-Heavy-Gewichtsklasse erreichen - der Vollstaendigkeit
    // halber trotzdem unterstuetzt, damit auch hier nie auf den alten
    // Kreis-Marker zurueckgefallen wird).
    void drawPrivateJetMarker(TFT_eSPI& gfx, int16_t x, int16_t y, float headingDeg, uint16_t color, bool heavy) {
        double rad = headingDeg * PI / 180.0;
        int16_t noseLen = heavy ? 10 : 7;
        int16_t tailLen = heavy ? 7 : 5;
        int16_t wingLen = heavy ? 8 : 6;
        uint8_t halfWidth = heavy ? 2 : 1;
        int16_t noseX = x + (int16_t)(sin(rad) * noseLen);
        int16_t noseY = y - (int16_t)(cos(rad) * noseLen);
        int16_t tailX = x - (int16_t)(sin(rad) * tailLen);
        int16_t tailY = y + (int16_t)(cos(rad) * tailLen);
        drawThickLine(gfx, tailX, tailY, noseX, noseY, color, halfWidth);

        double wingRad = rad + PI / 2.0;
        int16_t w1x = x + (int16_t)(sin(wingRad) * wingLen);
        int16_t w1y = y - (int16_t)(cos(wingRad) * wingLen);
        int16_t w2x = x - (int16_t)(sin(wingRad) * wingLen);
        int16_t w2y = y + (int16_t)(cos(wingRad) * wingLen);
        drawThickLine(gfx, w1x, w1y, w2x, w2y, color, halfWidth);
    }

    // Turboprop-Silhouette - gleicher Aufbau wie drawAirlinerMarker()
    // (gleiche Rumpf-/Tragflaechenlaenge: normal 9/7/8px, heavy 12/9/10px),
    // zusaetzlich zwei kleine Punkte auf den Tragflaechen (Triebwerke) -
    // typisch fuer zweimotorige Turboprops wie ATR/Dash 8/Saab 340.
    // heavy=true (in der Praxis selten - grosse Turboprops wie die C-130
    // erreichen zwar Heavy-Gewicht, sind aber kaum je per ADS-B im zivilen
    // Luftraum sichtbar; der Vollstaendigkeit halber trotzdem unterstuetzt).
    void drawTurbopropMarker(TFT_eSPI& gfx, int16_t x, int16_t y, float headingDeg, uint16_t color, bool heavy) {
        double rad = headingDeg * PI / 180.0;
        int16_t noseLen = heavy ? 12 : 9;
        int16_t tailLen = heavy ? 9 : 7;
        int16_t wingLen = heavy ? 10 : 8;
        uint8_t halfWidth = heavy ? 2 : 1;
        int16_t noseX = x + (int16_t)(sin(rad) * noseLen);
        int16_t noseY = y - (int16_t)(cos(rad) * noseLen);
        int16_t tailX = x - (int16_t)(sin(rad) * tailLen);
        int16_t tailY = y + (int16_t)(cos(rad) * tailLen);
        drawThickLine(gfx, tailX, tailY, noseX, noseY, color, halfWidth);

        double wingRad = rad + PI / 2.0;
        int16_t w1x = x + (int16_t)(sin(wingRad) * wingLen);
        int16_t w1y = y - (int16_t)(cos(wingRad) * wingLen);
        int16_t w2x = x - (int16_t)(sin(wingRad) * wingLen);
        int16_t w2y = y + (int16_t)(cos(wingRad) * wingLen);
        drawThickLine(gfx, w1x, w1y, w2x, w2y, color, halfWidth);

        // Triebwerks-Punkte: normal Radius 2 (vorher 1), heavy Radius 3 -
        // proportional zur groesseren Fluegelspannweite mitskaliert.
        int16_t engineOffset = heavy ? 7 : 5;
        uint8_t engineRadius = heavy ? 3 : 2;
        int16_t e1x = x + (int16_t)(sin(wingRad) * engineOffset);
        int16_t e1y = y - (int16_t)(cos(wingRad) * engineOffset);
        int16_t e2x = x - (int16_t)(sin(wingRad) * engineOffset);
        int16_t e2y = y + (int16_t)(cos(wingRad) * engineOffset);
        gfx.fillCircle(e1x, e1y, engineRadius, color);
        gfx.fillCircle(e2x, e2y, engineRadius, color);
    }

    // Dispatcher fuer den generischen Marker-Zweig in render()/tick() (dort
    // je dreifach fuer Zeichnen/Loeschen/Neuzeichnen aufgerufen, siehe
    // hp.isGroundVehicle/isRotorcraft-Ketten) - kapselt die Fallunterscheidung
    // an einer einzigen Stelle, damit dort nicht dreimal derselbe
    // Silhouette-switch dupliziert werden muss. Unknown faellt auf die
    // generische Fluzeug-Silhouette zurueck (drawAircraftMarker() oben,
    // KEIN Kreis mehr). "heavy" (ADS-B-Emitter-Kategorie "A5") wird an ALLE
    // vier Formen durchgereicht (siehe dortige heavy-Parameter) - es gibt
    // dadurch keinen eigenen Kreis-Marker fuer Heavy-Flugzeuge mehr, jede
    // Kategorie bekommt konsequent eine groessere/fettere Version ihrer
    // eigenen Silhouette (Alex' Wunsch: "Alles oder nichts", kein Kreis/Blob
    // mehr bei irgendeinem Flugzeug-Marker - nur Rotorcraft/Bodenfahrzeug
    // bleiben eigenstaendige, bereits silhouettenartige Formen).
    void drawTypedMarker(TFT_eSPI& gfx, int16_t x, int16_t y, float headingDeg, uint16_t color,
                          TypeSilhouette sil, bool heavy) {
        switch (sil) {
            case TypeSilhouette::Airliner:   drawAirlinerMarker(gfx, x, y, headingDeg, color, heavy); break;
            case TypeSilhouette::PrivateJet: drawPrivateJetMarker(gfx, x, y, headingDeg, color, heavy); break;
            case TypeSilhouette::Turboprop:  drawTurbopropMarker(gfx, x, y, headingDeg, color, heavy); break;
            default:                         drawAircraftMarker(gfx, x, y, headingDeg, color, heavy); break;
        }
    }

    // Marker fuer die ISS (Bonus-Feature, siehe iss_tracker.h) - ein
    // achtstrahliger "Stern"/Sparkle statt Kreis+Pfeilkopf, damit er auf
    // den ersten Blick eindeutig NICHT mit einem Flugzeug/Hubschrauber/
    // Bodenfahrzeug verwechselt werden kann. Eigene, sonst auf dem Radar
    // ungenutzte Farbe (Magenta) statt der ueblichen Hoehenfarbe - die ISS
    // hat keine sinnvolle "Hoehenkategorie" im Sinne des Radars.
    constexpr uint16_t ISS_MARKER_COLOR = TFT_MAGENTA;

    void drawIssMarker(TFT_eSPI& gfx, int16_t x, int16_t y) {
        constexpr int16_t R_OUTER = 8;
        constexpr int16_t R_INNER = 3;
        gfx.fillCircle(x, y, R_INNER, ISS_MARKER_COLOR);
        for (int i = 0; i < 8; i++) {
            double rad = i * 45.0 * PI / 180.0;
            int16_t ex = x + (int16_t)(sin(rad) * R_OUTER);
            int16_t ey = y - (int16_t)(cos(rad) * R_OUTER);
            gfx.drawLine(x, y, ex, ey, ISS_MARKER_COLOR);
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
        gfx.drawRoundRect(r.x, r.y, r.w, r.h, 4, themeBaseColor(gfx));
        gfx.setTextDatum(MC_DATUM);
        gfx.setTextColor(themeBaseColor(gfx), TFT_BLACK);
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
        // die Legende bei aktiver Nachtdimmung (Sonnenunter- bis -aufgang) die HELLEN Farben,
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
        uint16_t dim = UiTheme::accentColorDimmed(gfx, 0.4f);
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

    // "Klassik-Radar", Teil 1: Kometenschweif hinter der Sweep-Linie
    // (SettingsStore::classicRadarEnabled()). Erweitert die bestehende
    // drawSweepLine() um mehrere zusaetzliche, blasser werdende Segmente
    // HINTER der aktuellen Sweep-Position - ruft dafuer drawSweepLine()
    // in einer Schleife bei mehreren Winkeln auf, anstatt eine eigene,
    // unabhaengige Zeichenroutine zu erfinden. Nutzt exakt denselben
    // Rotationsmechanismus/dieselbe Winkelgeschwindigkeit wie die normale
    // Sweep-Linie (der Aufrufer uebergibt einfach sweepAngleDeg bzw.
    // prevSweepAngleDeg wie gehabt) - es gibt keine zweite, unabhaengige
    // Animation. Jedes Segment nutzt intern denselben L.radius-Bezug wie
    // drawSweepLine() selbst, bleibt also strukturell garantiert innerhalb
    // der Kreisflaeche, exakt wie die normale Sweep-Linie.
    constexpr uint8_t SWEEP_TAIL_SEGMENTS = 8;
    constexpr float SWEEP_TAIL_SPAN_DEG = 24.0f;

    void drawSweepTail(TFT_eSPI& gfx, const Layout& L, float tipAngleDeg, uint16_t baseColor) {
        for (uint8_t i = 0; i < SWEEP_TAIL_SEGMENTS; i++) {
            float t = (float)i / (float)(SWEEP_TAIL_SEGMENTS - 1);
            float segAngle = tipAngleDeg - t * SWEEP_TAIL_SPAN_DEG;
            uint16_t segColor = (i == 0) ? baseColor : scaleColorBrightness(baseColor, 1.0f - t);
            drawSweepLine(gfx, L, segAngle, segColor);
        }
    }

    // Loescht denselben Winkelbereich, den drawSweepTail() fuer dieselbe
    // Spitzenposition gezeichnet haette - aufgerufen mit der ALTEN
    // Sweep-Position, symmetrisch zum bisherigen Erase-Aufruf von
    // drawSweepLine(..., TFT_BLACK) bei der einfachen Linie.
    void eraseSweepTail(TFT_eSPI& gfx, const Layout& L, float tipAngleDeg) {
        for (uint8_t i = 0; i < SWEEP_TAIL_SEGMENTS; i++) {
            float t = (float)i / (float)(SWEEP_TAIL_SEGMENTS - 1);
            float segAngle = tipAngleDeg - t * SWEEP_TAIL_SPAN_DEG;
            drawSweepLine(gfx, L, segAngle, TFT_BLACK);
        }
    }

    void drawStaticBackground(TFT_eSPI& gfx, const Layout& L, float rangeKm) {
        gfx.drawCircle(L.cx, L.cy, L.radius, TFT_DARKGREY);
        gfx.drawCircle(L.cx, L.cy, L.radius * 2 / 3, TFT_DARKGREY);
        gfx.drawCircle(L.cx, L.cy, L.radius / 3, TFT_DARKGREY);
        gfx.drawFastHLine(L.cx - L.radius, L.cy, L.radius * 2, TFT_DARKGREY);
        gfx.drawFastVLine(L.cx, L.cy - L.radius, L.radius * 2, TFT_DARKGREY);

        // "Klassik-Radar", Teil 2: zusaetzliche Rasterspeichen alle 30 Grad
        // (SettingsStore::classicRadarEnabled()) - die 4 Hauptrichtungen
        // (0/90/180/270 Grad, N/S/E/W) sind bereits durch die Kreuzlinien
        // oben abgedeckt, hier nur die 8 dazwischenliegenden Winkel
        // (30/60/120/150/210/240/300/330). Deutlich dezenter als die
        // Hauptlinie (gedaempftes Grau statt TFT_DARKGREY, TFT_eSPI kennt
        // keine variable Linienstaerke). Statisch (keine Animation) -
        // deshalb hier in drawStaticBackground() statt in tick(), laeuft
        // aber trotzdem jeden 80ms-Tick mit, da diese Funktion ohnehin bei
        // jedem Sweep-Restore neu aufgerufen wird (siehe tick() unten).
        // Gleicher Radius-Bezug (L.radius) wie die Sweep-Linie/Kreuzlinien
        // - dadurch strukturell auf die Kreisflaeche begrenzt, kein
        // separater Clip-Check noetig.
        if (SettingsStore::classicRadarEnabled()) {
            uint16_t spokeColor = scaleColorBrightness(TFT_DARKGREY, 0.45f);
            constexpr int16_t SPOKE_ANGLES_DEG[8] = {30, 60, 120, 150, 210, 240, 300, 330};
            for (uint8_t i = 0; i < 8; i++) {
                double rad = (double)SPOKE_ANGLES_DEG[i] * DEG_TO_RAD;
                int16_t x2 = L.cx + (int16_t)(L.radius * sin(rad));
                int16_t y2 = L.cy - (int16_t)(L.radius * cos(rad));
                gfx.drawLine(L.cx, L.cy, x2, y2, spokeColor);
            }
        }

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

    // Scanlinien-Overlay: immer aktiv, kein Schalter. Rein optisch,
    // angelehnt an alte CRT-/Radarmonitore. WICHTIGER FIX: die erste Fassung zeichnete
    // ueber den GESAMTEN Bildschirm (y=0 bis SCREEN_HEIGHT, volle Breite) -
    // dadurch wurden auch der "Menu"-Button in der Kopfzeile und die
    // Reichweiten-Anzeige/Info-Zeile unten von den Scanlinien-Punkten
    // durchloechert, sichtbar als optisch "kaputter"/unterbrochener Text
    // (von Alex per Screenshot gemeldet). Das Overlay soll aber laut
    // Auftrag NUR ueber der eigentlichen Radarkreis-Flaeche liegen - jetzt
    // per Distanz-Check (dx^2+dy^2 <= radius^2) strikt auf den Kreis
    // begrenzt, Buttons/Kopfzeile/Info-Zeile liegen alle AUSSERHALB des
    // Kreises und werden dadurch nie mehr beruehrt. TFT_eSPI kann auf
    // diesem Board keine Pixel zuverlaessig zuruecklesen (readPixel/
    // readRect setzt funktionierendes MISO voraus, was auf den meisten
    // CYD-Boards nicht verlaesslich verdrahtet ist) - deshalb KEIN echtes
    // Alpha-Blending, sondern ein gestreutes Punktmuster (nur jeder 2. Pixel
    // in jeder 3. Zeile wird schwarz uebermalt, mit zeilenweise
    // wechselndem Versatz gegen ein starres Streifenmuster/Moire). Wird
    // bewusst nur am ENDE von render() aufgerufen (voller Redraw bei
    // echter Datenaenderung/Tap), NICHT in tick() (80ms-Takt) - ein neuer
    // Durchlauf nach jedem vollen render() reicht fuer den gewuenschten
    // "immer sichtbaren" Eindruck, ohne den schnellen Tick zusaetzlich zu
    // belasten.
    void drawScanlines(TFT_eSPI& gfx, const Layout& L) {
        int16_t y0 = L.cy - L.radius;
        int16_t y1 = L.cy + L.radius;
        int32_t radiusSq = (int32_t)L.radius * L.radius;
        for (int16_t y = y0; y <= y1; y += 3) {
            int16_t xOffset = ((y - y0) / 3) % 2;
            for (int16_t x = L.cx - L.radius + xOffset; x <= L.cx + L.radius; x += 2) {
                int32_t dx = x - L.cx, dy = y - L.cy;
                if (dx * dx + dy * dy > radiusSq) continue;
                gfx.drawPixel(x, y, TFT_BLACK);
            }
        }
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
        // Rohes Rufzeichen (nicht der ggf. auf den Hex-Code zurueckfallende
        // Anzeigetext callsignText oben) - vom QR-Button gebraucht (siehe
        // qrButtonRect()/runFlightQrScreen() weiter unten), da handleTap()
        // keinen direkten Zugriff auf das aktuell ausgewaehlte Aircraft-
        // Objekt hat, sondern nur auf diesen zwischengespeicherten Zustand.
        char callsign[9] = {0};
        char reg[9] = {0};
        LineMarquee airline, model, type, route, alt, speed, climb, distHeading, squawk, seats;
    };
    PanelState lastPanel;

    // Siehe consumeHeaderRedrawFlag()/qrButtonRect()-Zweig in handleTap()
    // unten - true, solange der Flug-QR-Code-Screen die Kopfzeile
    // ueberschrieben hat und noch niemand drawHeader()/updateStatusLine()
    // deswegen erneut aufgerufen hat.
    bool headerRedrawNeeded = false;

    // Kleiner "QR"-Button oben rechts im Detail-Panel (siehe
    // drawDetailPanel()/handleTap() weiter unten) - oeffnet einen
    // Vollbild-QR-Code mit einem Live-Tracking-Link fuer den aktuell
    // angezeigten Flug. Eigene Funktion, da sowohl beim Zeichnen als auch
    // beim Antippen exakt dieselbe Flaeche gebraucht wird.
    Rect qrButtonRect(int16_t panelTop) {
        return {(int16_t)(Config::SCREEN_WIDTH - 42), (int16_t)(panelTop + 4), 38, 24};
    }

    // Kleiner "+WL"/"-WL"-Button direkt links neben dem QR-Button (siehe
    // drawDetailPanel()/handleTap() weiter unten) - fuegt das aktuell
    // angezeigte Flugzeug per einem Tipp zur Beobachtungsliste
    // (AircraftWatchlist) hinzu bzw. entfernt es wieder, statt den Umweg
    // ueber Menue > Flugoptionen > Beobachtungsliste mit manueller
    // Rufzeichen-Eingabe zu gehen. Gleiche Hoehe/Breite wie der QR-Button,
    // 4px Abstand dazwischen.
    Rect watchlistButtonRect(int16_t panelTop) {
        Rect qr = qrButtonRect(panelTop);
        return {(int16_t)(qr.x - 4 - 38), qr.y, 38, 24};
    }

    // Trefferflaeche der Route-Zeile im Detail-Panel (Herkunft/Ziel) - macht
    // sie antippbar, um ICAO/IATA-Flughafencodes direkt umzuschalten
    // (SettingsStore::useIataAirportCodes()), als schnellerer Weg neben dem
    // Menuepfad Land/Region > Einheiten, der Alex zu verschachtelt war.
    // Beide Wege schalten dieselbe gespeicherte Einstellung um. Geometrie
    // folgt exakt dem festen Zeilen-Layout in drawDetailPanel(): Callsign
    // bei panelTop+26 (+30 Abstand danach), dann Airline/Modell/Typ je
    // LINE_H=22 vor der Route-Zeile.
    Rect routeRowRect(int16_t panelTop) {
        constexpr int16_t LINE_H = 22;
        int16_t routeY = (int16_t)(panelTop + 26 + 30 + LINE_H * 3);
        return {0, (int16_t)(routeY - 14), Config::SCREEN_WIDTH, LINE_H};
    }

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
        advanceAndDrawMarqueeLine(gfx, lastPanel.route);
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
            gfx.drawRect(0, panelTop, Config::SCREEN_WIDTH, DETAIL_PANEL_H, themeBaseColor(gfx));
            lastPanel = PanelState{};
            strncpy(lastPanel.hex, a.hex, sizeof(lastPanel.hex) - 1);
        }

        int16_t y = panelTop + 26;

        {
            // Reserviert rechts Platz fuer QR- UND Beobachtungslisten-Button
            // (siehe qrButtonRect()/watchlistButtonRect()) frei, damit ein
            // langes Rufzeichen nicht darunter/dahinter verschwindet -
            // direkt aus watchlistButtonRect(panelTop).x berechnet (statt
            // eines fest verdrahteten Breitenwerts), da dieser Button jetzt
            // die linke Kante der reservierten Flaeche markiert.
            String txt = a.callsign[0] ? a.callsign : a.hex;
            bool callsignChanged = forceFull || lastPanel.callsignText != txt;
            if (callsignChanged) {
                gfx.fillRect(0, y - 22, Config::SCREEN_WIDTH, 28, TFT_BLACK);
                gfx.setTextColor(themeBaseColor(gfx), TFT_BLACK);
                gfx.setTextSize(2);
                int16_t callsignMaxW = a.callsign[0] ? (int16_t)(watchlistButtonRect(panelTop).x - 14) : textMaxWidth;
                printLineTruncated(gfx, 8, y, callsignMaxW, txt);
                gfx.setTextSize(1);
                lastPanel.callsignText = txt;
                strncpy(lastPanel.callsign, a.callsign, sizeof(lastPanel.callsign) - 1);
                lastPanel.callsign[sizeof(lastPanel.callsign) - 1] = 0;
                strncpy(lastPanel.reg, a.reg, sizeof(lastPanel.reg) - 1);
                lastPanel.reg[sizeof(lastPanel.reg) - 1] = 0;

                // QR-Button neu zeichnen, wann immer diese Zeile neu
                // gezeichnet wird (nicht nur bei forceFull) - sonst koennte
                // ein spaeteres Aendern NUR des Rufzeichens (z.B. ADS-B
                // liefert es live nach) den Button versehentlich
                // ueberschreiben, ohne ihn wieder herzustellen. Nur
                // sichtbar, wenn ueberhaupt ein Rufzeichen bekannt ist -
                // ohne Rufzeichen liesse sich kein sinnvoller
                // Live-Tracking-Link bilden (z.B. bei manchen
                // Sichtflug-Maschinen).
                if (a.callsign[0]) {
                    // Voll gefuellter Button (statt nur eines duennen
                    // Rahmens wie vorher) - der reine Umriss ging im
                    // sonstigen grafischen "Rauschen" des Panels (Sterne,
                    // Rahmenlinien) fast unter und wurde kaum bemerkt.
                    // Gefuellter Hintergrund in der Themenfarbe + schwarze
                    // Schrift sorgt fuer deutlich mehr Kontrast/Auffaelligkeit.
                    Rect qrBtn = qrButtonRect(panelTop);
                    gfx.fillRoundRect(qrBtn.x, qrBtn.y, qrBtn.w, qrBtn.h, 4, themeBaseColor(gfx));
                    gfx.setTextDatum(MC_DATUM);
                    gfx.setTextColor(TFT_BLACK, themeBaseColor(gfx));
                    gfx.drawString("QR", qrBtn.x + qrBtn.w / 2, qrBtn.y + qrBtn.h / 2);
                    gfx.setTextDatum(TL_DATUM);

                    // Beobachtungslisten-Button direkt links daneben - zeigt
                    // "+" oder "-" je nach Beobachtungsstatus, ABER das ist
                    // rein informativ (nur das Symbol aendert sich).
                    // Antippen bleibt in handleTap() unveraendert: fuegt ueber
                    // AircraftWatchlist::addWatched() hinzu, falls noch nicht
                    // drauf, und springt danach immer in den
                    // Beobachtungslisten-Screen - kein direktes Entfernen ueber
                    // diesen Button (frueherer Ansatz hatte einen Bug: das
                    // rohe Rufzeichen aus dem Detail-Panel stimmte oft nicht
                    // exakt mit der intern normalisierten Version in
                    // AircraftWatchlist ueberein, siehe normalize() in
                    // aircraft_watchlist.cpp - dadurch schlug das direkte
                    // Entfernen oft fehl). Entfernen geschieht weiterhin nur
                    // im Beobachtungslisten-Screen selbst ueber das
                    // vorhandene "X". Gleicher visueller Stil wie der
                    // QR-Button.
                    Rect wlBtn = watchlistButtonRect(panelTop);
                    gfx.fillRoundRect(wlBtn.x, wlBtn.y, wlBtn.w, wlBtn.h, 4, themeBaseColor(gfx));
                    gfx.setTextDatum(MC_DATUM);
                    gfx.setTextColor(TFT_BLACK, themeBaseColor(gfx));
                    const char* wlSymbol = AircraftWatchlist::isWatched(a.callsign) ? "-" : "+";
                    gfx.drawString(wlSymbol, wlBtn.x + wlBtn.w / 2, wlBtn.y + wlBtn.h / 2);
                    gfx.setTextDatum(TL_DATUM);
                }
            }
        }
        y += 30;

        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, themeBaseColor(gfx), lastPanel.airline,
                   String(a.airlineName), forceFull);
        y += LINE_H;

        String modelLine = details.loading
            ? String(I18n::t(StringId::DETAIL_MODEL)) + I18n::t(StringId::DETAIL_LOADING_DOTS)
            : String(I18n::t(StringId::DETAIL_MODEL)) + (details.model[0] ? details.model : I18n::t(StringId::DETAIL_UNKNOWN));
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, themeBaseColor(gfx), lastPanel.model, modelLine, forceFull);
        y += LINE_H;

        String typeLine = String(I18n::t(StringId::DETAIL_TYPE)) + (a.typeCode[0] ? a.typeCode : I18n::t(StringId::DETAIL_UNKNOWN));
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, themeBaseColor(gfx), lastPanel.type, typeLine, forceFull);
        y += LINE_H;

        // Start-/Zielflughafen (ICAO-Code) ueber AircraftDetails::get() -
        // wird zusammen mit dem Modell abgefragt (siehe aircraft_details.cpp),
        // aber ueber das Rufzeichen statt den Hex-Code aufgeloest. Ohne
        // Rufzeichen (z.B. manche Sichtflug-Maschinen) bleibt die Route
        // grundsaetzlich unbekannt, kein Abruf noetig.
        String routeLine;
        if (!a.callsign[0]) {
            routeLine = String(I18n::t(StringId::DETAIL_ROUTE)) + I18n::t(StringId::DETAIL_UNKNOWN);
        } else if (details.loading) {
            routeLine = String(I18n::t(StringId::DETAIL_ROUTE)) + I18n::t(StringId::DETAIL_LOADING_DOTS);
        } else if (details.routeOrigin[0] && details.routeDest[0]) {
            // IATA-Anzeige (Menue > Land/Region > Einheiten) nur, wenn die
            // Lookup-Kette fuer BEIDE Flughaefen einen IATA-Code geliefert
            // hat (siehe routeOriginIata/routeDestIata in
            // aircraft_details.h) - sonst sauberer Rueckfall auf ICAO,
            // statt einen halb-ICAO/halb-IATA-Mix oder Leerstring zu zeigen.
            bool useIata = SettingsStore::useIataAirportCodes() &&
                           details.routeOriginIata[0] && details.routeDestIata[0];
            const char* origin = useIata ? details.routeOriginIata : details.routeOrigin;
            const char* dest = useIata ? details.routeDestIata : details.routeDest;
            routeLine = String(I18n::t(StringId::DETAIL_ROUTE)) + origin + " -> " + dest;
        } else {
            routeLine = String(I18n::t(StringId::DETAIL_ROUTE)) + I18n::t(StringId::DETAIL_UNKNOWN);
        }
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, themeBaseColor(gfx), lastPanel.route, routeLine, forceFull);
        // Dezente Umrandung zeigt, dass diese Zeile antippbar ist (siehe
        // routeRowRect()/handleTap() - ICAO/IATA-Umschaltung). Jeden
        // Frame neu gezeichnet statt nur bei Textaenderung, da
        // updateMarqueeLine() bei unveraendertem Text (z.B. "unbekannt")
        // gar nichts zeichnet und die Umrandung sonst nach einem Toggle
        // verschwinden wuerde.
        {
            Rect routeRow = routeRowRect(panelTop);
            gfx.drawRoundRect(routeRow.x + 3, routeRow.y + 1, routeRow.w - 6, routeRow.h - 2, 4, themeBaseColor(gfx));
        }
        y += LINE_H;

        char buf[48];
        snprintf(buf, sizeof(buf), "%s%.0fm / %.0fft", I18n::t(StringId::DETAIL_ALT),
                 Units::feetToMeters((float)a.altBaroFt), (float)a.altBaroFt);
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, themeBaseColor(gfx), lastPanel.alt, String(buf), forceFull);
        y += LINE_H;

        snprintf(buf, sizeof(buf), "%s%.0fkm/h / %.0fkt", I18n::t(StringId::DETAIL_SPEED),
                 Units::ktToKmh(a.groundSpeedKt), a.groundSpeedKt);
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, themeBaseColor(gfx), lastPanel.speed, String(buf), forceFull);
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
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, themeBaseColor(gfx), lastPanel.climb, climbLine, forceFull);
        y += LINE_H;

        // Zusaetzlich zu km/nm auch Meilen (mi) - nm allein war fuer
        // Laien-Nutzer aus Ländern mit imperialen Einheiten (v.a. USA)
        // nicht selbsterklaerend, "Meilen" sind dort die gebraeuchlichere
        // Alltags-Distanzeinheit (nm bleibt trotzdem stehen, da es zur
        // Geschwindigkeit in kt passt: 1 kt = 1 nm/h). Peilung (Richtung vom
        // eigenen Standort zum Flugzeug, siehe compassLabel() oben) haengt
        // bewusst hier an dieselbe Zeile an statt eine eigene zu bekommen -
        // im Detail-Panel ist praktisch kein Platz mehr uebrig (286px Hoehe,
        // schon jetzt bis auf ein paar Pixel mit den zehn bestehenden Zeilen
        // ausgefuellt). Wird die Zeile dadurch zu breit fuer die Panel-
        // Breite, scrollt sie automatisch wie jede andere lange Zeile hier
        // (siehe updateMarqueeLine()) - kein Abschneiden noetig. Eigener,
        // groesserer lokaler Puffer statt des sonst hier wiederverwendeten
        // "buf" (nur 48 Byte, reicht fuer die laengeren Uebersetzungen mit
        // allen drei Werten nicht mehr aus).
        char distBuf[96];
        snprintf(distBuf, sizeof(distBuf), "%s%.0fkm / %.0fnm / %.0fmi  %s%.0f  %s%.0f° %s",
                 I18n::t(StringId::DETAIL_DIST),
                 a.distanceKm, Units::kmToNm(a.distanceKm), Units::kmToMi(a.distanceKm),
                 I18n::t(StringId::DETAIL_HDG), a.headingDeg,
                 I18n::t(StringId::DETAIL_BEARING_PREFIX), a.bearingDeg, compassLabel(a.bearingDeg));
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, themeBaseColor(gfx), lastPanel.distHeading, String(distBuf), forceFull);
        y += LINE_H;

        String squawkLine = String(I18n::t(StringId::DETAIL_SQUAWK)) + (a.squawk[0] ? a.squawk : I18n::t(StringId::DETAIL_UNKNOWN));
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, themeBaseColor(gfx), lastPanel.squawk, squawkLine, forceFull);
        y += LINE_H;

        String seatsLine = a.estSeats > 0
            ? String(I18n::t(StringId::DETAIL_SEATS_EST)) + a.estSeats
            : String(I18n::t(StringId::DETAIL_SEATS_UNKNOWN));
        updateMarqueeLine(gfx, y, LINE_H, textMaxWidth, themeBaseColor(gfx), lastPanel.seats, seatsLine, forceFull);
        y += 22;

        if (forceFull) {
            gfx.setTextColor(themeDimColor(gfx), TFT_BLACK);
            gfx.setCursor(8, y);
            gfx.print(I18n::t(StringId::DETAIL_TAP_CLOSE));
        }

        lastPanel.valid = true;
    }

    // Kurzer Zeilenumbruch (hoechstens 2 Zeilen) fuer die Hinweiszeile unter
    // dem QR-Code - noetig, da manche Uebersetzungen (z.B. Italienisch/
    // Spanisch) bei Textgroesse 1 breiter als der Bildschirm waeren (gleiche
    // Grundidee wie menu_screen.cpp::wrapTitleLines(), hier aber bewusst
    // einfacher gehalten, da nur EINE kurze Hinweiszeile betroffen ist).
    void drawWrappedCenteredHint(TFT_eSPI& gfx, const String& text, int16_t centerX, int16_t startY,
                                  int16_t maxWidth, int16_t lineH) {
        String line1 = text;
        String line2;
        if (gfx.textWidth(line1) > maxWidth) {
            int32_t breakAt = -1;
            for (int32_t i = 0; i < (int32_t)line1.length(); i++) {
                if (line1[i] == ' ' && gfx.textWidth(line1.substring(0, i)) <= maxWidth) {
                    breakAt = i;
                }
            }
            if (breakAt > 0) {
                line2 = line1.substring(breakAt + 1);
                line1 = line1.substring(0, breakAt);
            }
        }
        gfx.setTextDatum(MC_DATUM);
        gfx.setTextColor(TFT_WHITE, TFT_BLACK);
        gfx.drawString(line1, centerX, startY);
        if (line2.length() > 0) {
            gfx.drawString(line2, centerX, (int16_t)(startY + lineH));
        }
        gfx.setTextDatum(TL_DATUM);
    }

    // Vollbild-QR-Code mit einem FlightAware-Live-Tracking-Link fuer das
    // uebergebene Rufzeichen - erreichbar ueber den "QR"-Button oben rechts
    // im Detail-Panel (siehe drawDetailPanel()/qrButtonRect()). Gleiches
    // QRCode-Bibliotheks-Muster (Version 4, ECC_LOW, 4px-Module) wie der
    // bestehende WebUI-QR-Code in webui_screen.cpp.
    void runFlightQrScreen(TFT_eSPI& gfx, const char* callsign) {
        MenuStars::reset();
        // Explizit setzen statt vom Aufrufer geerbt anzunehmen - die
        // Breitenberechnung in drawWrappedCenteredHint() unten haengt vom
        // aktuellen textSize-Zustand ab, siehe dortiger Kommentar.
        gfx.setTextSize(1);

        String cs = String(callsign);
        cs.trim();
        cs.toUpperCase();

        // FlightAware statt z.B. Flightradar24 gewaehlt, da deren
        // "/live/flight/<Rufzeichen>"-URL-Schema direkt mit dem rohen
        // ADS-B-Rufzeichen funktioniert, ohne zusaetzliche Aufloesung ueber
        // einen anderen Dienst.
        String url = "https://flightaware.com/live/flight/" + cs;

        constexpr uint8_t QR_VERSION = 4;
        constexpr int16_t QR_SIZE_MODULES = 33; // Version 4: 4*4+17 = 33
        constexpr int16_t QR_BLOCK = 4;
        constexpr int16_t QR_QUIET = 2;
        constexpr int16_t QR_PIXEL_SIZE = (QR_SIZE_MODULES + 2 * QR_QUIET) * QR_BLOCK;
        constexpr int16_t QR_X = (Config::SCREEN_WIDTH - QR_PIXEL_SIZE) / 2;
        constexpr int16_t QR_Y = 40;

        uint8_t qrData[qrcode_getBufferSize(QR_VERSION)];
        QRCode qrcode;
        qrcode_initText(&qrcode, qrData, QR_VERSION, ECC_LOW, url.c_str());

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

        gfx.fillScreen(TFT_BLACK);
        gfx.setTextColor(themeBaseColor(gfx), TFT_BLACK);
        gfx.setCursor(10, 14);
        gfx.println(cs);

        gfx.fillRect(QR_X, QR_Y, QR_PIXEL_SIZE, QR_PIXEL_SIZE, TFT_WHITE);
        for (uint8_t my = 0; my < qrcode.size; my++) {
            for (uint8_t mx = 0; mx < qrcode.size; mx++) {
                if (qrcode_getModule(&qrcode, mx, my)) {
                    int16_t px = (int16_t)(QR_X + (QR_QUIET + mx) * QR_BLOCK);
                    int16_t py = (int16_t)(QR_Y + (QR_QUIET + my) * QR_BLOCK);
                    gfx.fillRect(px, py, QR_BLOCK, QR_BLOCK, TFT_BLACK);
                }
            }
        }

        drawWrappedCenteredHint(gfx, I18n::t(StringId::DETAIL_QR_HINT), Config::SCREEN_WIDTH / 2,
                                (int16_t)(QR_Y + QR_PIXEL_SIZE + 14), (int16_t)(Config::SCREEN_WIDTH - 20), 16);

        drawButton(gfx, backBtn, I18n::t(StringId::BACK));

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (backBtn.contains(tap.x, tap.y)) return;
            }
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) return;
            MenuStars::update(gfx);
            delay(20);
        }
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
            uint16_t color = UiTheme::accentColorDimmed(gfx, bright / 255.0f);
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
            uint16_t color = UiTheme::accentColorDimmed(gfx, bright / 255.0f);
            gfx.drawPixel(bgStars[i].x, bgStars[i].y, color);
        }
    }
}

uint16_t themeColor(TFT_eSPI& gfx) {
    return themeBaseColor(gfx);
}

void invalidatePanel() {
    lastPanel.valid = false;
}

bool consumeHeaderRedrawFlag() {
    bool v = headerRedrawNeeded;
    headerRedrawNeeded = false;
    return v;
}

void render(TFT_eSPI& tft, int16_t top) {
    Layout L = computeLayout(top);
    float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];

    // Radar-Puls-Trigger: NUR bei echter Datenaenderung (Versionsvergleich),
    // nicht bei jedem render()-Aufruf - siehe Kommentar bei lastPulseVersion
    // oben. Laeuft unabhaengig davon, ob gerade ein Detail-Panel offen ist,
    // damit kein Datenwechsel waehrend eines geoeffneten Panels verloren geht.
    uint32_t currentDataVersion = AircraftTable::version();
    if (currentDataVersion != lastPulseVersion) {
        lastPulseVersion = currentDataVersion;
        if (SettingsStore::radarPulseEnabled()) {
            pulseActive = true;
            pulseStartMs = millis();
        }
    }

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
            // Kein drawScanlines() hier: das Detail-Panel ersetzt den
            // Radarkreis komplett durch eine eigene Text-Ansicht - das
            // Scanlinien-Overlay ist explizit auf die Radarkreis-Flaeche
            // begrenzt (siehe drawScanlines()) und haette hier nichts
            // Sinnvolles zum Ueberlagern.
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

    if (SettingsStore::classicRadarEnabled()) {
        drawSweepTail(tft, L, sweepAngleDeg, sweepLineColor(tft));
    } else {
        drawSweepLine(tft, L, sweepAngleDeg, sweepLineColor(tft));
    }
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
    for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) phosphorEntries[i].seenThisRender = false;

    for (uint8_t i = 0; i < count && i < MAX_HIT_POINTS; i++) {
        Aircraft& a = snapshot[i];
        if (a.distanceKm > rangeKm * 1.05f) continue;

        if (SettingsStore::hideGroundVehicles() && a.category[0] == 'C') continue;

        if (SettingsStore::onlyHelicopters() && !(a.category[0] == 'A' && a.category[1] == '7')) continue;

        // "Nur niedrig fliegende Flugzeuge" - dieselbe Schwelle wie die
        // gruene Hoehen-Einfaerbung (Config::COLOR_LOW_ALT_THRESHOLD_FT,
        // siehe colorForAltitude()). Bodenfahrzeuge (Kategorie "C")
        // bewusst IMMER ausgeschlossen, wenn dieser Filter aktiv ist -
        // unabhaengig vom separaten hideGroundVehicles()-Schalter, da es
        // sich nicht um "niedrig fliegende Flugzeuge" handelt.
        if (SettingsStore::onlyLowAltitude() &&
            (a.category[0] == 'C' || a.altBaroFt >= Config::COLOR_LOW_ALT_THRESHOLD_FT)) continue;

        if (AirlineFilter::isHidden(a.callsign)) continue;

        visibleCount++;

        RadarMath::PolarCoord polar{a.distanceKm, a.bearingDeg};
        RadarMath::ScreenPoint pt = RadarMath::toScreen(polar, L.cx, L.cy, L.radius, rangeKm);

        // ADS-B-Emitter-Kategorie "C*" = Bodenfahrzeug (nur ueberhaupt
        // sichtbar, wenn "Bodenfahrzeuge ausblenden" aus ist, siehe Filter
        // oben) - eigene Farbe/Marker statt der hoehenbasierten Flugzeug-
        // Darstellung, siehe drawGroundVehicleMarker()/colorForGroundVehicle().
        bool isGroundVehicle = a.category[0] == 'C';
        // ADS-B-Emitter-Kategorie "A7" = Rotorcraft (Hubschrauber) - eigenes
        // Symbol statt des Pfeilkopf-Markers, siehe drawHelicopterMarker().
        bool isRotorcraft = a.category[0] == 'A' && a.category[1] == '7';
        // ADS-B-Emitter-Kategorie "A5" = "Heavy" - bekommt dieselbe Typ-
        // Silhouette wie sonst auch, nur groesser/fetter (siehe
        // drawTypedMarker()/heavy-Parameter), unabhaengig vom oranger-Ring-
        // Status (isNotable unten), der jetzt ausschliesslich fuer Militaer-/
        // Regierungs-Rufzeichen reserviert ist.
        bool isHeavy = isHeavyCategory(a.category);
        // Nur relevant fuer den generischen Marker-Fall unten (siehe
        // drawTypedMarker()) - Bodenfahrzeug/Rotorcraft behalten wie
        // gehabt Vorrang vor der Typcode-Silhouette.
        TypeSilhouette typeSilhouette = classifyTypeSilhouette(a.typeCode);
        // Der CRT-Phosphor-Effekt betrifft jetzt ALLE Marker-Kategorien
        // gleichermassen (Alex' ausdruecklicher Wunsch - vorher waren nur
        // niedrig fliegende, gruene Flugzeuge betroffen, siehe Kommentar bei
        // crtPhosphorColor() oben). "ownColor" ist die normale,
        // kategorie-eigene Farbe (Gruen/Gelb/Rot nach Hoehe, Blau fuer
        // Bodenfahrzeuge) - genau DIESE Farbe faedet crtPhosphorColor() aus,
        // statt wie vorher immer die Theme-Grundfarbe. Der Phosphor-Zustand
        // wird unabhaengig vom aktiven Farbschema kontinuierlich mitgetrackt
        // (siehe PhosphorEntry oben), nur die tatsaechliche Farbe wird
        // ausschliesslich im CRT-Modus ueberschrieben.
        bool crtFadeEligible = true;
        uint16_t ownColor = isGroundVehicle ? colorForGroundVehicle(tft) : colorForAltitude(tft, a.altBaroFt);
        uint16_t color;
        PhosphorEntry* ph = findOrCreatePhosphor(a.hex);
        if (ph) ph->seenThisRender = true;
        color = crtModeActive() ? crtPhosphorColor(ownColor, ph, millis()) : ownColor;

        // "Klassik-Radar", Teil 3+4: Sonar-Ping UND Signal-Rauschen bei
        // neuem Kontakt - Tracking (isNewContactAndTouch()) laeuft bewusst
        // IMMER, die sichtbaren Effekte nur bei eingeschaltetem
        // Klassik-Radar (siehe Kommentar bei isNewContactAndTouch() oben).
        // Beide Effekte werden vom selben Ereignis ausgeloest und laufen
        // unabhaengig voneinander gleichzeitig.
        bool isNewContact = isNewContactAndTouch(a.hex, millis());
        if (isNewContact && SettingsStore::classicRadarEnabled()) {
            triggerSonarPing(pt.x, pt.y, ownColor);
            triggerStaticNoise(pt.x, pt.y);
        }

        bool isSelected = selectedHex[0] && strcmp(a.hex, selectedHex) == 0;
        bool isEmergency = SettingsStore::emergencyAlertEnabled() && isEmergencySquawk(a.squawk);
        // Squawk-Wachliste (siehe squawk_watchlist.h) zaehlt als derselbe
        // "isWatched"-Zustand wie die Rufzeichen-Beobachtungsliste - beide
        // bedeuten inhaltlich dasselbe ("ein Flugzeug, das mich
        // interessiert, ist da"), daher ein gemeinsames Flag statt eines
        // eigenen Alarm-Modus.
        bool isWatched = SettingsStore::watchlistAlertEnabled() &&
                          (AircraftWatchlist::isWatched(a.callsign) || SquawkWatchlist::isWatched(a.squawk));
        // Niedrigste Prioritaet der drei Ring-Markierungen (siehe unten) -
        // rein informativ, kein Alarm wie Notfall/Beobachtungsliste. Deckt
        // jetzt zwei unabhaengige Kriterien ab: Militaer-/Regierungs-
        // Rufzeichen-Praefixe (isNotableCallsign(), aktuell immer false,
        // siehe dortiger Kommentar) UND die neue, per Schalter aktivierbare
        // Squawk-basierte Militaer-/Behoerdenflug-Erkennung
        // (isMilitaryGovSquawk(), SettingsStore::
        // militarySquawkDetectionEnabled()).
        bool isNotable = isNotableCallsign(a.callsign) ||
                          (SettingsStore::militarySquawkDetectionEnabled() && isMilitaryGovSquawk(a.squawk));

        if (isSelected) {
            tft.drawCircle(pt.x, pt.y, 9, TFT_WHITE);
            selectionStillPresent = true;
            selected = a;
            drawBearingIndicator(tft, L, a.bearingDeg);
        }
        if (isGroundVehicle) {
            drawGroundVehicleMarker(tft, pt.x, pt.y, color);
        } else if (isRotorcraft) {
            drawHelicopterMarker(tft, pt.x, pt.y, color);
        } else {
            drawTypedMarker(tft, pt.x, pt.y, a.headingDeg, color, typeSilhouette, isHeavy);
        }

        if (isEmergency) {
            tft.drawCircle(pt.x, pt.y, 12, TFT_RED);
        } else if (isWatched) {
            tft.drawCircle(pt.x, pt.y, 12, TFT_CYAN);
        } else if (isNotable) {
            tft.drawCircle(pt.x, pt.y, 12, TFT_ORANGE);
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
        hitPoints[i].isNotable = isNotable;
        hitPoints[i].isGroundVehicle = isGroundVehicle;
        hitPoints[i].isRotorcraft = isRotorcraft;
        hitPoints[i].isHeavy = isHeavy;
        hitPoints[i].typeSilhouette = typeSilhouette;
        hitPoints[i].bearingDeg = a.bearingDeg;
        hitPoints[i].crtFadeEligible = crtFadeEligible;
        hitPoints[i].baseColor = ownColor;
        strncpy(hitPoints[i].hex, a.hex, sizeof(hitPoints[i].hex) - 1);
        strncpy(hitPoints[i].callsign, a.callsign, sizeof(hitPoints[i].callsign) - 1);
    }

    // PhosphorEntry-Eintraege freigeben, die in diesem Durchlauf nicht
    // gebraucht wurden (Flugzeug ausser Reichweite geraten oder ganz
    // verschwunden) - sonst wuerde die feste Kapazitaet (gleiche Groesse wie
    // hitPoints[]) irgendwann von "toten" Eintraegen belegt bleiben.
    for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
        if (phosphorEntries[i].used && !phosphorEntries[i].seenThisRender) {
            phosphorEntries[i].used = false;
        }
    }

    if (visibleCount > 0) lastAircraftSeenMs = millis();

    if (selectionStillPresent) {
        tft.endWrite();
        drawDetailPanel(tft, selected);
        // FIX (Alex' Meldung: Scanlinien durchloechern "Modell:
        // unbekannt"/"Typ: unbekannt" im Detail-Panel): hier fehlte ein
        // return - die Ausfuehrung lief bisher ungebremst weiter bis zu
        // drawScanlines(tft, L) weiter unten, die dann direkt ueber das
        // gerade gezeichnete Panel hinwegzeichnete (Panel-Flaeche und
        // Scanlinien-Bounding-Box ueberlappen vollstaendig). Kein erneutes
        // tft.startWrite() mehr noetig, da direkt zurueckgekehrt wird -
        // gleiches Prinzip wie im bereits korrekten panelAlreadyOpen-
        // Fast-Path weiter oben (tft.endWrite() + return direkt nach
        // drawDetailPanel()).
        return;
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

    drawScanlines(tft, L);

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
        if (SettingsStore::classicRadarEnabled()) {
            eraseSweepTail(tft, L, prevSweepAngleDeg);
        } else {
            drawSweepLine(tft, L, prevSweepAngleDeg, TFT_BLACK);
        }
        // Alten Radar-Puls-Ring (falls im letzten Tick gezeichnet) ebenfalls
        // erst schwarz uebermalen, BEVOR der statische Hintergrund
        // wiederhergestellt wird - gleiches Prinzip wie bei der Sweep-Linie.
        // Gleiches Clipping wie beim Zeichnen des Rings unten - sonst
        // erzeugt gerade dieser Loeschschritt die vom Nutzer gemeldeten
        // schwarzen Unterbrechungen in den Button-Rahmen (ein
        // ungeclipptes tft.drawCircle() haette hier munter in die
        // Kopf-/Fusszeile hinein schwarze Pixel gemalt).
        if (prevPulseRadius >= 0) {
            for (int16_t angleDeg = 0; angleDeg < 360; angleDeg++) {
                double rad = angleDeg * DEG_TO_RAD;
                int16_t px = L.cx + (int16_t)(prevPulseRadius * sin(rad));
                int16_t py = L.cy - (int16_t)(prevPulseRadius * cos(rad));
                if (py < top || py >= L.infoTop) continue;
                tft.drawPixel(px, py, TFT_BLACK);
            }
        }
        // Alte Sonar-Ping-Ringe (Klassik-Radar Teil 3) ebenfalls loeschen,
        // gleiches Prinzip/gleiche Reihenfolge wie beim Radar-Puls oben.
        // Doppeltes Clipping (Radar-Kreis UND top/L.infoTop) - ein Ping in
        // Randnaehe des Kreises koennte sonst trotz des kleinen Radius in
        // Kopf-/Fusszeile hineinmalen, genau der Fehler, der beim Radar-Puls
        // (siehe ACHTER FIX oben) und bei drawScanlines() bereits einmal
        // real aufgetreten ist.
        for (uint8_t i = 0; i < MAX_SONAR_PINGS; i++) {
            if (sonarPings[i].prevRadius < 0) continue;
            for (int16_t angleDeg = 0; angleDeg < 360; angleDeg++) {
                double rad = angleDeg * DEG_TO_RAD;
                int16_t px = sonarPings[i].cx + (int16_t)(sonarPings[i].prevRadius * sin(rad));
                int16_t py = sonarPings[i].cy - (int16_t)(sonarPings[i].prevRadius * cos(rad));
                int16_t dx = px - L.cx;
                int16_t dy = py - L.cy;
                if ((int32_t)dx * dx + (int32_t)dy * dy > (int32_t)L.radius * L.radius) continue;
                if (py < top || py >= L.infoTop) continue;
                tft.drawPixel(px, py, TFT_BLACK);
            }
        }
        // Altes Signal-Rauschen (Klassik-Radar Teil 4) ebenfalls loeschen -
        // hier reicht ein einfaches Nachzeichnen der zuletzt TATSAECHLICH
        // gezeichneten Punkte (prevPointCount/prevPointsX/Y, siehe unten),
        // die beim Zeichnen bereits geclippt wurden - kein erneuter
        // Clip-Check noetig.
        for (uint8_t i = 0; i < MAX_STATIC_NOISE; i++) {
            for (uint8_t p = 0; p < staticNoises[i].prevPointCount; p++) {
                tft.drawPixel(staticNoises[i].prevPointsX[p], staticNoises[i].prevPointsY[p], TFT_BLACK);
            }
        }
        // Alte Regentropfen-Linien ebenfalls loeschen - laeuft UNABHAENGIG
        // vom aktuellen Wetterstatus (haengt nur von hasPrev ab), damit ein
        // Tropfen, der noch sichtbar war, als der Regen gerade aufgehoert
        // hat, sauber verschwindet statt stehen zu bleiben.
        for (uint8_t i = 0; i < MAX_RAINDROPS; i++) {
            if (!rainDrops[i].hasPrev) continue;
            tft.drawLine(rainDrops[i].prevX1, rainDrops[i].prevY1,
                         rainDrops[i].prevX2, rainDrops[i].prevY2, TFT_BLACK);
        }
        drawStaticBackground(tft, L, rangeKm);
        tft.fillCircle(L.cx, L.cy, 3, TFT_WHITE);
    }

    float oldSweepAngle = sweepAngleDeg;
    sweepAngleDeg += SWEEP_DEGREES_PER_SEC * (deltaMs / 1000.0f);
    if (sweepAngleDeg >= 360.0f) sweepAngleDeg -= 360.0f;

    if (SettingsStore::classicRadarEnabled()) {
        drawSweepTail(tft, L, sweepAngleDeg, sweepLineColor(tft));
    } else {
        drawSweepLine(tft, L, sweepAngleDeg, sweepLineColor(tft));
    }
    prevSweepAngleDeg = sweepAngleDeg;

    // Radar-Puls (SettingsStore::radarPulseEnabled(), ausgeloest in
    // render() bei echter Datenaenderung) - Ring waechst linear ueber
    // PULSE_DURATION_MS von 0 auf PULSE_MAX_RADIUS_FACTOR * L.radius, dabei
    // faedet die Helligkeit gleichzeitig von voll auf 0. Die weiter unten
    // folgende Flugzeug-Marker-Schleife zeichnet eventuell vom Ring
    // ueberdeckte Marker automatisch wieder her (laeuft bei jedem Tick neu).
    // ACHTER FIX (Alex' Meldung: der Ring schnitt die untere Kante der
    // Mode-/Menu-Button-Rahmen ab): PULSE_MAX_RADIUS_FACTOR=1.4 laesst den
    // Ring bewusst ueber L.radius hinauswachsen (Teil des Effekts), bisher
    // per tft.drawCircle() - eine Bibliotheksfunktion, die NUR an den
    // physischen Displaygrenzen clippt, nicht an unserer eigenen
    // Kopf-/Fusszeilen-Grenze (top/L.infoTop). Bei radius=96 erreicht der
    // Ring am Maximalpunkt bis zu top-22px, deutlich in die Kopfzeile
    // hinein. Jetzt durch eine winkelbasierte, selbst geclippte Zeichnung
    // ersetzt (gleiches Prinzip wie drawScanlines() oben) - jeder Punkt
    // wird nur gezeichnet, wenn er innerhalb
    // [top, L.infoTop) liegt.
    uint32_t nowMs = millis();
    if (pulseActive) {
        uint32_t elapsed = nowMs - pulseStartMs;
        if (elapsed >= PULSE_DURATION_MS) {
            pulseActive = false;
            prevPulseRadius = -1;
        } else {
            float fraction = (float)elapsed / (float)PULSE_DURATION_MS;
            int16_t pulseRadius = (int16_t)(fraction * L.radius * PULSE_MAX_RADIUS_FACTOR);
            float brightnessFraction = 1.0f - fraction;
            uint16_t pulseColor = scaleColorBrightness(sweepLineColor(tft), brightnessFraction);
            for (int16_t angleDeg = 0; angleDeg < 360; angleDeg++) {
                double rad = angleDeg * DEG_TO_RAD;
                int16_t px = L.cx + (int16_t)(pulseRadius * sin(rad));
                int16_t py = L.cy - (int16_t)(pulseRadius * cos(rad));
                if (py < top || py >= L.infoTop) continue;
                tft.drawPixel(px, py, pulseColor);
            }
            prevPulseRadius = pulseRadius;
        }
    }

    // Sonar-Ping (Klassik-Radar Teil 3, ausgeloest in render() bei neuem
    // Kontakt ueber isNewContactAndTouch()/triggerSonarPing()) - LOKALER
    // Ring an der Flugzeugposition statt vom Zentrum aus (Unterschied zum
    // Radar-Puls oben), waechst linear ueber SONAR_PING_DURATION_MS von 0
    // auf SONAR_PING_MAX_RADIUS, faedet dabei gleichzeitig aus. Jeder Punkt
    // wird zusaetzlich auf den Radar-Kreis geclippt (siehe Kommentar beim
    // Loesch-Block oben) - ein Flugzeug knapp am Kreisrand darf den Ring
    // nicht darueber hinausmalen lassen. Die weiter unten folgende
    // Flugzeug-Marker-Schleife zeichnet eventuell vom Ring ueberdeckte
    // Marker automatisch wieder her, exakt wie beim Radar-Puls.
    for (uint8_t i = 0; i < MAX_SONAR_PINGS; i++) {
        if (!sonarPings[i].active) continue;
        uint32_t elapsed = nowMs - sonarPings[i].startMs;
        if (elapsed >= SONAR_PING_DURATION_MS) {
            sonarPings[i].active = false;
            sonarPings[i].prevRadius = -1;
            continue;
        }
        float fraction = (float)elapsed / (float)SONAR_PING_DURATION_MS;
        int16_t ringRadius = (int16_t)(fraction * SONAR_PING_MAX_RADIUS);
        float brightnessFraction = 1.0f - fraction;
        uint16_t ringColor = scaleColorBrightness(sonarPings[i].color, brightnessFraction);
        for (int16_t angleDeg = 0; angleDeg < 360; angleDeg++) {
            double rad = angleDeg * DEG_TO_RAD;
            int16_t px = sonarPings[i].cx + (int16_t)(ringRadius * sin(rad));
            int16_t py = sonarPings[i].cy - (int16_t)(ringRadius * cos(rad));
            int16_t dx = px - L.cx;
            int16_t dy = py - L.cy;
            if ((int32_t)dx * dx + (int32_t)dy * dy > (int32_t)L.radius * L.radius) continue;
            if (py < top || py >= L.infoTop) continue;
            tft.drawPixel(px, py, ringColor);
        }
        sonarPings[i].prevRadius = ringRadius;
    }

    // Signal-Rauschen (Klassik-Radar Teil 4, ausgeloest zusammen mit dem
    // Sonar-Ping oben) - ein paar zufaellige Einzelpixel dicht um die
    // Flugzeugposition (STATIC_NOISE_SPREAD, deutlich kleiner als der
    // Sonar-Ping-Radius), JEDEN Tick neu ausgewuerfelt fuer einen echten
    // Flacker-Effekt, verschwindet nach STATIC_NOISE_DURATION_MS komplett.
    // Gleiches doppeltes Clipping (Radar-Kreis + top/L.infoTop) wie beim
    // Sonar-Ping - ein Flugzeug direkt am Kreisrand darf keine Pixel
    // ausserhalb malen.
    for (uint8_t i = 0; i < MAX_STATIC_NOISE; i++) {
        if (!staticNoises[i].active) continue;
        uint32_t elapsed = nowMs - staticNoises[i].startMs;
        if (elapsed >= STATIC_NOISE_DURATION_MS) {
            staticNoises[i].active = false;
            staticNoises[i].prevPointCount = 0;
            continue;
        }
        uint8_t count = 0;
        for (uint8_t p = 0; p < STATIC_NOISE_POINT_COUNT; p++) {
            int16_t ox = (int16_t)random(-STATIC_NOISE_SPREAD, STATIC_NOISE_SPREAD + 1);
            int16_t oy = (int16_t)random(-STATIC_NOISE_SPREAD, STATIC_NOISE_SPREAD + 1);
            int16_t px = (int16_t)(staticNoises[i].cx + ox);
            int16_t py = (int16_t)(staticNoises[i].cy + oy);
            int16_t dx = px - L.cx;
            int16_t dy = py - L.cy;
            if ((int32_t)dx * dx + (int32_t)dy * dy > (int32_t)L.radius * L.radius) continue;
            if (py < top || py >= L.infoTop) continue;
            tft.drawPixel(px, py, staticNoises[i].color);
            staticNoises[i].prevPointsX[count] = px;
            staticNoises[i].prevPointsY[count] = py;
            count++;
        }
        staticNoises[i].prevPointCount = count;
    }

    // Animierter Regen-Effekt (SettingsStore::rainEffectEnabled(), Menue >
    // System > Radar-Darstellung, AN per Default) - nur aktiv, wenn der
    // Schalter an ist UND Weather::current() Regen/Gewitter zeigt UND eine
    // Windrichtung bekannt ist (sonst bleibt rainDrops[] einfach untaetig/
    // inaktiv - der Erase-Block oben raeumt dann noch sichtbare Tropfen von
    // einem vorherigen "es regnet"-Zustand sauber weg, auch wenn der
    // Schalter waehrenddessen ausgeschaltet wird). Jeder Tropfen wird
    // einzeln auf den Radar-Kreis geclippt (Distanz-Check beider Linien-
    // Endpunkte zum Zentrum, gleiches Prinzip wie Sonar-Ping/Signal-
    // Rauschen oben) - ein Tropfen nahe am Kreisrand darf keine Pixel
    // ausserhalb malen.
    {
        Weather::Condition cond = Weather::current();
        float windDir = Weather::currentWindDirectionDeg();
        bool rainy = SettingsStore::rainEffectEnabled() &&
                     (cond == Weather::Condition::Rain || cond == Weather::Condition::Thunderstorm) && windDir >= 0.0f;
        float step = RAIN_DROP_SPEED_PX_PER_SEC * (deltaMs / 1000.0f);
        uint16_t dropColor = scaleColorBrightness(TFT_SKYBLUE, 0.45f);

        for (uint8_t i = 0; i < MAX_RAINDROPS; i++) {
            RainDrop& d = rainDrops[i];
            if (!rainy) {
                d.active = false;
                d.hasPrev = false;
                continue;
            }
            if (!d.active) {
                spawnRainDrop(d, L, windDir);
            }

            d.x += d.vx * step;
            d.y += d.vy * step;

            float dx = d.x - L.cx;
            float dy = d.y - L.cy;
            if (dx * dx + dy * dy > (float)L.radius * (float)L.radius) {
                // Kreis verlassen - sofort an neuer Randposition neu starten,
                // damit staendig eine aehnliche Tropfenzahl sichtbar bleibt.
                spawnRainDrop(d, L, windDir);
            }

            int16_t x1 = (int16_t)d.x;
            int16_t y1 = (int16_t)d.y;
            int16_t x2 = (int16_t)(d.x - d.vx * RAIN_DROP_LENGTH);
            int16_t y2 = (int16_t)(d.y - d.vy * RAIN_DROP_LENGTH);

            auto inBounds = [&](int16_t px, int16_t py) {
                int32_t ddx = px - L.cx;
                int32_t ddy = py - L.cy;
                return ddx * ddx + ddy * ddy <= (int32_t)L.radius * L.radius &&
                       py >= top && py < L.infoTop && px >= 0 && px < Config::SCREEN_WIDTH;
            };
            if (inBounds(x1, y1) && inBounds(x2, y2)) {
                tft.drawLine(x1, y1, x2, y2, dropColor);
                d.prevX1 = x1; d.prevY1 = y1; d.prevX2 = x2; d.prevY2 = y2;
                d.hasPrev = true;
            } else {
                d.hasPrev = false;
            }
        }
    }

    for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
        if (!hitPoints[i].valid) continue;
        HitPoint& hp = hitPoints[i];

        // CRT-Phosphor-Effekt: Sweep-Treffer-Erkennung laeuft unabhaengig
        // vom aktiven Farbschema kontinuierlich mit (siehe Kommentar bei
        // crtFadeEligible in render()), die Marker-Farbe wird aber nur im
        // CRT-Modus tatsaechlich ueberschrieben. Jetzt fuer ALLE Kategorien
        // (hp.baseColor haelt die jeweils eigene, ungefaedete Farbe fest -
        // Gruen/Gelb/Rot/Blau), nicht mehr nur fuer niedrig fliegende,
        // gruene Flugzeuge.
        if (hp.crtFadeEligible) {
            if (sweepCrossedBearing(oldSweepAngle, sweepAngleDeg, hp.bearingDeg)) {
                PhosphorEntry* ph = findOrCreatePhosphor(hp.hex);
                if (ph) {
                    ph->everSwept = true;
                    ph->lastSweptMs = nowMs;
                }
            }
            if (crtModeActive()) {
                PhosphorEntry* ph = findOrCreatePhosphor(hp.hex);
                hp.color = crtPhosphorColor(hp.baseColor, ph, nowMs);
            }
        }

        bool inAlertRange = hp.distanceKm <= Config::LED_ALERT_RADIUS_KM;
        if (inAlertRange && !ledBlinkOn) {
            if (hp.isGroundVehicle) {
                drawGroundVehicleMarker(tft, hp.x, hp.y, TFT_BLACK);
            } else if (hp.isRotorcraft) {
                drawHelicopterMarker(tft, hp.x, hp.y, TFT_BLACK);
            } else {
                drawTypedMarker(tft, hp.x, hp.y, hp.headingDeg, TFT_BLACK, hp.typeSilhouette, hp.isHeavy);
            }
            if (hp.isEmergency || hp.isWatched || hp.isNotable) {
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
        } else if (hp.isRotorcraft) {
            drawHelicopterMarker(tft, hp.x, hp.y, hp.color);
        } else {
            drawTypedMarker(tft, hp.x, hp.y, hp.headingDeg, hp.color, hp.typeSilhouette, hp.isHeavy);
        }

        if (hp.isEmergency) {
            tft.drawCircle(hp.x, hp.y, 12, TFT_RED);
        } else if (hp.isWatched) {
            tft.drawCircle(hp.x, hp.y, 12, TFT_CYAN);
        } else if (hp.isNotable) {
            tft.drawCircle(hp.x, hp.y, 12, TFT_ORANGE);
        }

        tft.setTextColor(hp.color);
        tft.setTextDatum(BC_DATUM);
        const char* label = hp.callsign[0] ? hp.callsign : hp.hex;
        tft.drawString(label, hp.x, hp.y - 8);
        tft.setTextDatum(TL_DATUM);
    }

    // ISS-Marker (Bonus-Feature, siehe iss_tracker.h) - im selben
    // Rendering-Durchlauf wie die Flugzeug-Marker oben, mit derselben
    // Distanz-/Peilungsberechnung (RadarMath::toPolar()/toScreen()), aber
    // bewusst NICHT Teil von hitPoints[]/visibleCountNow - zaehlt nicht
    // als "Flugzeug", loest keinen Alarm/Ton aus und ist nicht antippbar.
    // Nur gezeichnet, wenn der Schalter an ist (SettingsStore::
    // issMarkerEnabled(), Menue > Flugoptionen > Anzeigefilter, AN per
    // Default) UND eine Position bekannt UND innerhalb des aktuell
    // eingestellten Radius ist - da die ISS mit ~7,66 km/s extrem schnell
    // ist, ist das vermutlich nur fuer wenige Sekunden der Fall, wenn
    // ueberhaupt (so beabsichtigt, nicht kuenstlich verlaengert).
    if (SettingsStore::issMarkerEnabled()) {
        IssTracker::Position iss = IssTracker::current();
        if (iss.available) {
            double homeLat = 0, homeLon = 0;
            LocationManager::getHomeLocation(homeLat, homeLon);
            RadarMath::PolarCoord issPolar = RadarMath::toPolar(homeLat, homeLon, iss.lat, iss.lon);
            if (issPolar.distanceKm <= rangeKm * 1.05f) {
                RadarMath::ScreenPoint issPt = RadarMath::toScreen(issPolar, L.cx, L.cy, L.radius, rangeKm);
                drawIssMarker(tft, issPt.x, issPt.y);
                tft.setTextColor(ISS_MARKER_COLOR);
                tft.setTextDatum(BC_DATUM);
                tft.drawString("ISS", issPt.x, issPt.y - 10);
                tft.setTextDatum(TL_DATUM);
            }
        }
    }

    uint8_t visibleCountNow = 0;
    for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
        if (hitPoints[i].valid) visibleCountNow++;
    }
    if (visibleCountNow > 0) lastAircraftSeenMs = millis();

    // Filter-Hinweis: erscheint jetzt IMMER (unabhaengig von aircraftCount),
    // sobald mindestens einer der Sichtbarkeitsfilter aktiv ist - macht
    // sichtbar, dass weitere Flugzeuge unsichtbar bleiben KOENNTEN, auch
    // wenn gerade zufaellig welche sichtbar sind (Alex' Meldung: vorher
    // verschwand der Hinweis, sobald trotz aktivem Niedrigflieger-Filter
    // ein passendes Flugzeug da war). Deshalb hier VOR der
    // EmptySky/TapForDetails-Verzweigung berechnet, statt nur im
    // EmptySky-Zweig. hideGroundVehicles() bewusst NICHT in dieser Liste -
    // ist werksseitig standardmaessig AN, wuerde also bei den meisten
    // Nutzern praktisch immer auftauchen und den Hinweis unnoetig
    // "geschwaetzig" machen, obwohl es nur der Normalzustand ist (Alex'
    // Rueckmeldung). Nur die Filter, die man eher vergisst.
    String activeFilters;
    if (SettingsStore::onlyHelicopters()) {
        activeFilters += I18n::t(StringId::RADAR_FILTER_NAME_HELICOPTERS);
    }
    if (SettingsStore::onlyLowAltitude()) {
        if (activeFilters.length()) activeFilters += ", ";
        activeFilters += I18n::t(StringId::RADAR_FILTER_NAME_LOW_ALTITUDE);
    }
    if (AirlineFilter::count() > 0) {
        if (activeFilters.length()) activeFilters += ", ";
        activeFilters += I18n::t(StringId::RADAR_FILTER_NAME_AIRLINE);
    }
    infoTextHasFilterHint = activeFilters.length() > 0;

    String infoText;
    InfoMsgKind kind;
    if (visibleCountNow == 0) {
        kind = InfoMsgKind::EmptySky;
        uint32_t emptySec = (millis() - lastAircraftSeenMs) / 1000;
        char buf[16];
        if (emptySec < 60) {
            snprintf(buf, sizeof(buf), "%lus", (unsigned long)emptySec);
        } else {
            unsigned long minutes = emptySec / 60;
            snprintf(buf, sizeof(buf), "%lumin", minutes);
        }
        infoText = String(I18n::t(StringId::RADAR_EMPTY_SKY_PREFIX)) + buf;
    } else {
        kind = InfoMsgKind::TapForDetails;
        // Anzahl sichtbarer Flugzeuge der Hinweiszeile voranstellen (z.B.
        // "5 Flugzeuge - Für mehr Details ein Flugzeug antippen") - auf
        // einen Blick sichtbar, ohne dass man erst tippen oder zaehlen
        // muss. visibleCountNow ist oben ohnehin schon berechnet.
        String countText = (visibleCountNow == 1)
            ? I18n::t(StringId::RADAR_AIRCRAFT_COUNT_SINGULAR)
            : String(visibleCountNow) + I18n::t(StringId::RADAR_AIRCRAFT_COUNT_PLURAL_SUFFIX);
        infoText = countText + " - " + I18n::t(StringId::RADAR_TAP_FOR_DETAILS);
    }

    // Filter-Hinweis an die jeweils aktive Zeile anhaengen (scrollt bei
    // Bedarf mit, wie der Rest der Info-Zeile) - das Wort
    // RADAR_FILTER_HINT_WORD ("Hinweis") wird rot hervorgehoben, der Rest
    // in normaler Farbe (siehe printColoredSegment()). redStart markiert,
    // wo genau das Wort innerhalb von infoText beginnt - NICHT zwingend am
    // Anfang, da es hinter dem jeweiligen Haupttext angehaengt wird.
    int32_t redStart = 0;
    int32_t redLen = 0;
    if (infoTextHasFilterHint) {
        String hintWord = I18n::t(StringId::RADAR_FILTER_HINT_WORD);
        redStart = (int32_t)infoText.length() + 3; // " - " davor, siehe unten
        redLen = (int32_t)hintWord.length();
        infoText += " - " + hintWord + String(I18n::t(StringId::RADAR_FILTER_ACTIVE_PREFIX)) + activeFilters;
    }

    constexpr int16_t INFO_TEXT_X = 8;
    constexpr int16_t INFO_TEXT_GAP = 6;
    int16_t infoTextY = L.infoTop + 20;
    int16_t infoTextW = L.rangeBtn.x - INFO_TEXT_X - INFO_TEXT_GAP;

    if (kind != infoMarqueeKind) {
        infoMarqueeKind = kind;
        setupInfoMarquee(tft, infoText, infoTextW, redStart, redLen);
    } else {
        updateInfoMarqueeText(tft, infoText, infoTextW, redStart, redLen);
    }
    drawInfoMarquee(tft, INFO_TEXT_X, infoTextY, infoTextW);

    tft.endWrite();
}

bool handleTap(TFT_eSPI& tft, int16_t x, int16_t y, int16_t top) {
    Layout L = computeLayout(top);

    if (selectedHex[0]) {
        int16_t panelTop = Config::SCREEN_HEIGHT - DETAIL_PANEL_H;
        Rect qrBtn = qrButtonRect(panelTop);
        if (lastPanel.callsign[0] && qrBtn.contains(x, y)) {
            runFlightQrScreen(tft, lastPanel.callsign);
            lastPanel.valid = false;
            headerRedrawNeeded = true;
            return true;
        }

        Rect wlBtn = watchlistButtonRect(panelTop);
        if (lastPanel.callsign[0] && wlBtn.contains(x, y)) {
            // Ignoriert stillschweigend, falls das Flugzeug schon drauf
            // steht oder die Liste voll ist - kein Problem, der
            // Beobachtungslisten-Screen zeigt danach ohnehin den
            // tatsaechlichen Stand.
            AircraftWatchlist::addWatched(lastPanel.callsign);
            AircraftWatchlistScreen::run(tft);
            // Vollbild-Overlay wie der QR-Code-Screen - gleiches Muster wie
            // beim QR-Button oben (Panel-Neuaufbau + Kopfzeile erneuern).
            lastPanel.valid = false;
            headerRedrawNeeded = true;
            return true;
        }

        Rect routeRow = routeRowRect(panelTop);
        if (routeRow.contains(x, y)) {
            SettingsStore::setUseIataAirportCodes(!SettingsStore::useIataAirportCodes());
            return true;
        }

        for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
            if (!hitPoints[i].valid) continue;
            int16_t dx = x - hitPoints[i].x;
            int16_t dy = y - hitPoints[i].y;
            if (dx * dx + dy * dy <= 12 * 12) {
                strncpy(selectedHex, hitPoints[i].hex, sizeof(selectedHex) - 1);
                AircraftDetails::request(hitPoints[i].hex, hitPoints[i].callsign);
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

    // Antippbarer Filter-Hinweis in der unteren Info-Zeile (siehe render())
    // - erscheint dort jetzt unabhaengig von aircraftCount, sobald
    // mindestens ein Filter aktiv ist (also auch neben der normalen
    // "X Flugzeuge - ..."-Zeile, nicht nur bei "Leerer Himmel"). Nur aktiv,
    // wenn dort gerade tatsaechlich ein Filter-Hinweis angezeigt wird
    // (infoTextHasFilterHint), sonst wuerde ein Tap auf die normale
    // Info-Zeile ungewollt das Menue oeffnen. Springt direkt ins
    // "Anzeigefilter"-Menue, damit sich der Filter ohne eigene Navigation
    // sofort abstellen laesst (Alex' Wunsch nach der langen Fehlersuche).
    // Gleiches Muster wie runFlightQrScreen()/AircraftWatchlistScreen::run()
    // oben: blockierender Vollbild-Aufruf direkt aus handleTap(), danach
    // Panel/Kopfzeile ueber headerRedrawNeeded neu aufbauen lassen.
    if (infoTextHasFilterHint) {
        Rect infoTextRow = {0, L.infoTop, (int16_t)(L.rangeBtn.x - 2), (int16_t)(L.rangeBtn.h + 8)};
        if (infoTextRow.contains(x, y)) {
            MenuScreen::run(tft, true);
            lastPanel.valid = false;
            headerRedrawNeeded = true;
            return true;
        }
    }

    for (uint8_t i = 0; i < MAX_HIT_POINTS; i++) {
        if (!hitPoints[i].valid) continue;
        int16_t dx = x - hitPoints[i].x;
        int16_t dy = y - hitPoints[i].y;
        if (dx * dx + dy * dy <= 12 * 12) {
            strncpy(selectedHex, hitPoints[i].hex, sizeof(selectedHex) - 1);
            AircraftDetails::request(hitPoints[i].hex, hitPoints[i].callsign);
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

void selectAircraft(const char* hex, const char* callsign) {
    strncpy(selectedHex, hex, sizeof(selectedHex) - 1);
    AircraftDetails::request(hex, callsign);
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
            // Squawk-Wachliste loest denselben WatchlistBlue-Alarm aus wie
            // die Rufzeichen-Beobachtungsliste (siehe squawk_watchlist.h) -
            // gleicher watchlistAlertEnabled()-Schalter, kein eigenes
            // Ein/Aus dafuer.
            if (watchlistOn && (AircraftWatchlist::isWatched(table[i].callsign) ||
                                 SquawkWatchlist::isWatched(table[i].squawk))) anyWatched = true;
        }
        AircraftTable::unlock();
    }

    LedAlert::Mode mode = anyEmergency ? LedAlert::Mode::EmergencyRed
                         : anyWatched  ? LedAlert::Mode::WatchlistBlue
                         : anyClose    ? LedAlert::Mode::ProximityGreen
                                       : LedAlert::Mode::Off;

    // Update-Verfuegbar-Signal (dreimal kurz Magenta, alle 10s) laeuft
    // unabhaengig von proximityOn/emergencyOn/watchlistOn immer mit, sofern
    // OtaUpdate::isUpdateAvailable() true liefert - LedAlert::update()
    // unterdrueckt es selbst automatisch waehrend mode == EmergencyRed.
    ledBlinkOn = LedAlert::update(mode, nowMs, OtaUpdate::isUpdateAvailable());
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
