#include "connection_status_screen.h"
#include "aircraft_table.h"
#include "config.h"
#include "settings_store.h"
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

    constexpr int16_t LINE_X = 10;
    constexpr int16_t LINE_H = 24;
    constexpr int16_t LINE_MAX_W = Config::SCREEN_WIDTH - 20;
    // Datenquellen-Liste (Auftrag "Liste statt Zyklus-Knopf") - jede Quelle
    // eine eigene, einzeln antippbare Zeile mit rundem Status-Punkt statt
    // des vorherigen einzelnen Zyklus-Knopfs.
    constexpr int16_t SOURCE_ROW_H = 34;
    constexpr int16_t SOURCE_ROW_GAP = 4;
    constexpr int16_t SOURCE_DOT_R = 6;
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();

    // Auftrag "Flackern beheben" (direkt nach v6.9.3 aufgefallen): dieser
    // Screen zeichnete bisher BEDINGUNGSLOS alle ~500ms den kompletten
    // Bildschirm neu (fillScreen+alles), auch wenn sich der Inhalt gar
    // nicht geaendert hatte (z.B. blieb "5s" oft ueber zwei 500ms-Haelften
    // gleich, da die Sekunde noch nicht umgesprungen war) - mit dem neuen,
    // zusaetzlichen Datenquellen-Knopf dauerte jeder dieser unnoetigen
    // Redraws spuerbar laenger und das Schwarz-Aufblitzen wurde sichtbar.
    // lastSig haelt eine Signatur des zuletzt tatsaechlich gezeichneten
    // Inhalts fest - der teure Redraw (fillScreen + alles neu) laeuft nur
    // noch bei echter Aenderung, gleiches Prinzip wie die Inhalts-
    // Signaturen anderswo im Projekt. sourceBtn bleibt zwischen den
    // Iterationen bestehen (letzter TATSAECHLICH gezeichneter Stand),
    // damit die Tipp-Erkennung unten auch in einer uebersprungenen
    // Iteration mit der richtigen Position arbeitet - backBtn ist fix
    // (haengt nicht vom Inhalt ab) und wird nur einmal berechnet.
    String lastContentSig = "\x01";
    String lastAgeStr = "";
    Rect sourceRows[Config::ADSB_SOURCE_COUNT] = {};
    Rect backBtn = {LINE_X, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

    // "Letzter Abruf"-Zeile aendert sich (Sekundenzaehler) viel haeufiger
    // als alles andere auf diesem Screen - deshalb eigens gemerkte
    // Position/Hoehe dieser einen Zeile, damit sie bei einer reinen
    // Alters-Aenderung PUNKTUELL (kleines fillRect + Zeile neu) aktualisiert
    // werden kann, statt dafuer jedesmal den kompletten Bildschirm per
    // fillScreen() wischen und neu zeichnen zu muessen (das war trotz der
    // Signatur-Pruefung weiterhin 1x/Sekunde sichtbares Flackern, siehe
    // Diagnose-Log vom 26.10.).
    int16_t ageLineBaselineY = 0;
    int16_t ageLineLines = 1;
    const String agePrefix = I18n::t(StringId::CONNECTION_STATUS_LAST_FETCH_PREFIX);

    while (!done) {
        AircraftTable::lock();
        uint8_t tracked = AircraftTable::validCount();
        AircraftTable::unlock();

        uint32_t ageMs = AircraftTable::msSinceLastSuccessfulFetch(millis());
        AircraftTable::FetchOutcome outcome = AircraftTable::lastFetchOutcome();
        Quality quality = qualityForAge(ageMs);
        String ageStr = formatAge(ageMs);

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

        // Manuell waehlbare ADS-B-Datenquelle (Auftrag "Fallback-Server").
        uint8_t sourceIdx = SettingsStore::adsbSourceIndex();
        if (sourceIdx >= Config::ADSB_SOURCE_COUNT) sourceIdx = Config::DEFAULT_ADSB_SOURCE_INDEX;

        // Enthaelt bewusst NICHT ageStr - dessen Aenderung wird weiter unten
        // separat per Teil-Redraw behandelt, statt hier einen kompletten
        // Full-Redraw auszuloesen.
        String contentSig = String(tracked) + "|" + resultText + "|" + (int)quality + "|" + sourceIdx;

        // Ob sich bei einem reinen Alters-Update die Zeilenanzahl (1 oder 2
        // Zeilen) aendern wuerde - falls ja, wuerde ein punktueller
        // Teil-Redraw die Zeilen darunter verschieben/ueberlappen, also dann
        // lieber einen vollen Redraw auslösen (sehr seltener Fall, da
        // Praefix+Wert hier immer kurz sind).
        bool ageWouldWrap = tft.textWidth(agePrefix + ageStr) > LINE_MAX_W;
        bool ageWrapChanged = (ageWouldWrap != (ageLineLines > 1)) && contentSig == lastContentSig;

        if (contentSig != lastContentSig || ageWrapChanged) {
            lastContentSig = contentSig;

            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CONNECTION_STATUS));

            int16_t y = 40;

            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            y += drawPrefixedLine(tft, LINE_X, y, LINE_MAX_W, LINE_H,
                                   I18n::t(StringId::CONNECTION_STATUS_TRACKED_PREFIX), String(tracked));

            ageLineBaselineY = y;
            int16_t ageLineH = drawPrefixedLine(tft, LINE_X, y, LINE_MAX_W, LINE_H, agePrefix, ageStr);
            ageLineLines = (int16_t)(ageLineH / LINE_H);
            y += ageLineH;
            lastAgeStr = ageStr;

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

            // Ueberschrift der Datenquellen-Liste (Auftrag "Liste statt
            // Zyklus-Knopf") - danach eine eigene, einzeln antippbare Zeile
            // pro Quelle mit rundem Status-Punkt (gefuellt+gruen = aktiv,
            // nur Umriss = inaktiv), statt des vorherigen einzelnen
            // Zyklus-Knopfs, der zufaellig zur naechsten Quelle sprang.
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(LINE_X, y);
            tft.println(I18n::t(StringId::CONNECTION_STATUS_SOURCE_PREFIX));
            y += LINE_H;

            int16_t rowsTotalH = (int16_t)(Config::ADSB_SOURCE_COUNT * SOURCE_ROW_H +
                                            (Config::ADSB_SOURCE_COUNT - 1) * SOURCE_ROW_GAP);
            int16_t rowsStartY = (int16_t)(y + 6);
            // Nach unten gegen den Zurueck-Knopf geklemmt (gleiches Prinzip
            // wie vorher beim einzelnen Knopf) - garantiert mind. 8px
            // Abstand, unabhaengig davon, wie hoch der Inhalt darueber in
            // der jeweiligen Sprache ausfaellt.
            int16_t maxRowsBottom = (int16_t)(Config::SCREEN_HEIGHT - 50 - 8);
            if (rowsStartY + rowsTotalH > maxRowsBottom) {
                rowsStartY = (int16_t)(maxRowsBottom - rowsTotalH);
            }
            for (uint8_t i = 0; i < Config::ADSB_SOURCE_COUNT; i++) {
                int16_t rowY = (int16_t)(rowsStartY + i * (SOURCE_ROW_H + SOURCE_ROW_GAP));
                Rect row = {LINE_X, rowY, (int16_t)(Config::SCREEN_WIDTH - 20), SOURCE_ROW_H};
                sourceRows[i] = row;
                tft.fillRoundRect(row.x, row.y, row.w, row.h, 4, TFT_BLACK);
                tft.drawRoundRect(row.x, row.y, row.w, row.h, 4, UiTheme::accentColor(tft));

                bool active = (i == sourceIdx);
                int16_t dotCx = (int16_t)(row.x + 16);
                int16_t dotCy = (int16_t)(row.y + row.h / 2);
                if (active) {
                    tft.fillCircle(dotCx, dotCy, SOURCE_DOT_R, TFT_GREEN);
                } else {
                    tft.drawCircle(dotCx, dotCy, SOURCE_DOT_R, UiTheme::accentColor(tft));
                }

                // drawString()+setTextDatum() statt setCursor()+println() -
                // bei unserem Font ist nur die datum-basierte drawString()-
                // Variante von der Grundlinien-Verankerung unberuehrt (siehe
                // CLAUDE.md "Schriftzustand").
                tft.setTextDatum(ML_DATUM);
                tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
                tft.drawString(Config::ADSB_SOURCE_NAMES[i], (int16_t)(row.x + 32), dotCy);
                tft.setTextDatum(TL_DATUM);
            }

            drawButton(tft, backBtn, I18n::t(StringId::BACK));
        } else if (ageStr != lastAgeStr) {
            // Punktueller Teil-Redraw NUR der Alters-Zeile: kleines
            // fillRect (schwarz) genau ueber dieser einen Zeile, dann Text
            // neu zeichnen - der Rest des Bildschirms bleibt unberuehrt,
            // dadurch kein sichtbares Flackern mehr bei jedem Sekunden-Tick.
            // Hoehe/Breite des Clear-Rects bewusst grosszuegig gewaehlt
            // (deckt Ascent/Descent unseres Grundlinien-verankerten Fonts
            // ab, siehe CLAUDE.md "Schriftzustand"), damit auch laengere
            // Vorwerte (z.B. "45min" -> "2h") vollstaendig verschwinden.
            int16_t clearH = (int16_t)(ageLineLines * LINE_H + 6);
            tft.fillRect(LINE_X, (int16_t)(ageLineBaselineY - LINE_H + 4), LINE_MAX_W, clearH, TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            drawPrefixedLine(tft, LINE_X, ageLineBaselineY, LINE_MAX_W, LINE_H, agePrefix, ageStr);
            lastAgeStr = ageStr;
        }

        // Anders als die meisten anderen Menue-Screens (die nur bei einem
        // Tap neu zeichnen) wird hier alle ~500ms GEPRUEFT, ob sich etwas
        // geaendert hat (Datenalter/Farbstatus sollen sichtbar live
        // weiterlaufen, waehrend der Screen offen ist, Alex' Testwunsch) -
        // tatsaechlich NEU GEZEICHNET wird aber nur oben, wenn sich die
        // Signatur wirklich geaendert hat (Auftrag "Flackern beheben").
        TouchInput::Point tap;
        bool tapped = false;
        uint32_t waitStartMs = millis();
        while (true) {
            if (TouchInput::wasTapped(tap)) { tapped = true; break; }
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
            if (millis() - waitStartMs >= 500) break;
            MenuStars::update(tft);
            delay(20);
        }
        if (done) break;
        if (tapped && backBtn.contains(tap.x, tap.y)) {
            done = true;
        } else if (tapped) {
            for (uint8_t i = 0; i < Config::ADSB_SOURCE_COUNT; i++) {
                if (sourceRows[i].contains(tap.x, tap.y)) {
                    // Direkte Auswahl GENAU dieser Zeile (Auftrag "Liste
                    // statt Zyklus-Knopf") statt wie vorher zyklisch zur
                    // naechsten Quelle zu springen - Aenderung sofort
                    // wirksam (naechster NetTask-Abruf nutzt sie direkt,
                    // siehe AdsbClient::fetch()). Der naechste
                    // Schleifendurchlauf liest den neuen Wert frisch und
                    // loest damit automatisch einen Redraw aus (die
                    // Signatur enthaelt sourceIdx).
                    SettingsStore::setAdsbSourceIndex(i);
                    break;
                }
            }
        }
    }
}

}
