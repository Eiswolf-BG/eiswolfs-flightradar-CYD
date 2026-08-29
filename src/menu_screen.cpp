#include "menu_screen.h"
#include "touch_input.h"
#include "calibration_screen.h"
#include "wifi_manage_screen.h"
#include "stats_screen.h"
#include "stats_history_screen.h"
#include "logbook_files_screen.h"
#include "webui_screen.h"
#include "location_presets_screen.h"
#include "airline_filter_screen.h"
#include "aircraft_watchlist_screen.h"
#include "squawk_watchlist_screen.h"
#include "aircraft_list_screen.h"
#include "brightness_screen.h"
#include "timeout_screen.h"
#include "language_screen.h"
#include "units_screen.h"
#include "radar_theme_screen.h"
#include "settings_backup.h"
#include "settings_store.h"
#include "ota_update.h"
#include "net_task.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"
#include "changelog.h"
#include <time.h>

namespace MenuScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // ROW_GAP/ROW_START_Y bleiben unveraendert (werden von subMenuRowRect()
    // weiter unten mitbenutzt).
    constexpr int16_t ROW_GAP = 1;
    constexpr int16_t ROW_START_Y = 18;

    // Region-Unterseite (Sprache/Einheiten/Zurueck, nur 3 Eintraege):
    // Zeilenhoehe/-abstand werden aus der tatsaechlich verfuegbaren
    // Bildschirmflaeche errechnet (gleiches Muster wie bei subMenuRowRect()
    // weiter unten), statt eine kleine, fuer volle Seiten gedachte feste
    // Hoehe (vorher 22px) zu benutzen - die liess bei nur 3 Eintraegen fast
    // den ganzen Bildschirm leer und machte die Buttons winzig und schwer
    // zu treffen.
    constexpr uint8_t REGION_ROW_COUNT = 3;
    constexpr int16_t REGION_ROW_GAP = 10;
    constexpr int16_t REGION_END_Y = Config::SCREEN_HEIGHT - 10;
    constexpr int16_t REGION_ROW_H =
        (REGION_END_Y - ROW_START_Y - (REGION_ROW_COUNT - 1) * REGION_ROW_GAP) / REGION_ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(ROW_START_Y + index * (REGION_ROW_H + REGION_ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), REGION_ROW_H};
    }

    constexpr int16_t CAT_ROW_H = 50;
    constexpr int16_t CAT_ROW_GAP = 10;
    constexpr int16_t CAT_START_Y = 30;

    Rect catRowRect(uint8_t index) {
        return {10, (int16_t)(CAT_START_Y + index * (CAT_ROW_H + CAT_ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), CAT_ROW_H};
    }

    // Generische, aus der tatsaechlich verfuegbaren Bildschirmflaeche
    // berechnete Zeilenhoehe fuer eine Unterseite mit "count" Eintraegen -
    // ersetzt die vorher fuer jede Seite einzeln kopierten FLIGHT_ROW_H/
    // SYSTEM_ROW_H/BACKUP_RESET_ROW_H-Konstantenbloecke (gleiches Grundmuster,
    // nur COUNT/GAP unterschiedlich). Noetig geworden, weil die Flugoptionen-
    // und System-Seiten jetzt in mehrere kleinere Unterseiten aufgeteilt sind
    // (grosse Kategorie-Buttons statt langer Einzelzeilen-Listen, Alex'
    // Wunsch, analog zum Hauptmenue) - jede dieser Unterseiten hat eine
    // andere Anzahl Eintraege, ein fester Konstanten-Satz pro Seite haette
    // hier nur unnoetig viel fast identischen Code bedeutet.
    // startY optional ueberschreibbar (Default weiterhin ROW_START_Y) - fuer
    // Seiten mit einem zusaetzlichen "?"-Info-Button oben rechts im Header,
    // der bei ROW_START_Y=18 mit der ersten Zeile ueberlappen wuerde.
    // Aktuell von keiner Seite mehr genutzt (der bisher einzige Anwendungs-
    // fall, Page::FlightFilters, hat seinen "?"-Button inzwischen direkt in
    // die ISS-Marker-Zeile verlegt, siehe drawRowInfoButton()) - Parameter
    // bleibt fuer kuenftige Seiten mit demselben Bedarf erhalten.
    Rect subMenuRowRect(uint8_t index, uint8_t count, int16_t gap = 10, int16_t startY = ROW_START_Y) {
        int16_t endY = Config::SCREEN_HEIGHT - 10;
        int16_t rowH = (int16_t)((endY - startY - (int16_t)(count - 1) * gap) / count);
        return {10, (int16_t)(startY + index * (rowH + gap)),
                (int16_t)(Config::SCREEN_WIDTH - 20), rowH};
    }

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label,
                     bool active = false, bool danger = false) {
        uint16_t accent = danger ? TFT_RED : TFT_GREEN;
        uint16_t bg = active ? accent : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : accent;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Kleiner "?"-Info-Button rechts INNERHALB einer normalen Zeile (statt
    // wie sonst oben rechts im Seiten-Header) - gleiches Prinzip/gleiche
    // Groesse wie die neuen "?"-Buttons in radar_theme_screen.cpp (dort
    // bewusst dupliziert statt geteilt, siehe CLAUDE.md "jeder Screen
    // unabhaengig lauffaehig"). Bislang nur fuer die ISS-Marker-Zeile
    // gebraucht (siehe Page::FlightFilters), aber generisch genug fuer
    // jede subMenuRowRect()-Zeile.
    constexpr int16_t ROW_INFO_BTN_SIZE = 20;
    constexpr int16_t ROW_INFO_BTN_PAD = 6;

    Rect rowInfoBtnRect(const Rect& row) {
        return {(int16_t)(row.x + row.w - ROW_INFO_BTN_SIZE - ROW_INFO_BTN_PAD),
                (int16_t)(row.y + (row.h - ROW_INFO_BTN_SIZE) / 2),
                ROW_INFO_BTN_SIZE, ROW_INFO_BTN_SIZE};
    }

    void drawRowInfoButton(TFT_eSPI& tft, const Rect& row) {
        Rect btn = rowInfoBtnRect(row);
        drawButton(tft, btn, "?");
    }

    // Zweizeilige Variante von drawButton() - fuer den zusammengelegten
    // "Nach Update suchen"-Button im System-Menue (siehe Page::System),
    // der jetzt in Zeile 1 die aktuelle Version und in Zeile 2 den
    // bisherigen Button-Text traegt, seit der eigene "Info"-Button/-Screen
    // entfernt wurde (die Versionsnummer war dort der einzige wirklich
    // relevante Inhalt).
    void drawButtonTwoLines(TFT_eSPI& tft, const Rect& r, const String& line1, const String& line2) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        constexpr int16_t LINE_GAP = 14;
        tft.drawString(line1, r.x + r.w / 2, r.y + r.h / 2 - LINE_GAP / 2);
        tft.drawString(line2, r.x + r.w / 2, r.y + r.h / 2 + LINE_GAP / 2);
        tft.setTextDatum(TL_DATUM);
    }

    String onOff(bool on) { return I18n::t(on ? StringId::ON : StringId::OFF); }

    // Fortschrittspunkte-Anzeige waehrend SettingsBackup::backup()/restore()
    // laufen (siehe Aufrufe unten in Page::BackupReset) - diese sind
    // synchrone, SD-lastige Vorgaenge, die spuerbar dauern koennen und den
    // Button vorher wie eingefroren wirken liessen. SettingsBackup ruft den
    // hier uebergebenen Funktionszeiger vor jedem der beiden Kopiervorgaenge
    // (erst Einstellungen, dann WLAN) auf. Namespace-globale Zeiger/Variablen
    // statt Lambda-Capture, da ein einfacher C-Funktionszeiger uebergeben
    // werden muss (kein std::function im Projekt).
    TFT_eSPI* progressTft = nullptr;
    Rect progressBtnRect;
    String progressLabel;
    uint8_t progressDots = 0;

    void drawProgressStep() {
        if (!progressTft) return;
        progressDots++;
        String label = progressLabel;
        for (uint8_t i = 0; i < progressDots; i++) label += ".";
        drawButton(*progressTft, progressBtnRect, label);
    }

    String screenTimeoutLabel(uint8_t minutes) {
        String prefix = I18n::t(StringId::MENU_SCREEN_TIMEOUT_PREFIX);
        if (minutes == 0) return prefix + I18n::t(StringId::NEVER);
        return prefix + String(minutes) + " min";
    }

    String brightnessLabel(uint8_t percent) {
        return String(I18n::t(StringId::MENU_BRIGHTNESS_PREFIX)) + String(percent) + "%";
    }

    void showBriefMessage(TFT_eSPI& tft, const String& msg, uint16_t color) {
        tft.fillRect(0, Config::SCREEN_HEIGHT - 18, Config::SCREEN_WIDTH, 18, TFT_BLACK);
        tft.setTextColor(color, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(msg, Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT - 9);
        tft.setTextDatum(TL_DATUM);
        delay(1200);
    }

    // Wie die einfache layoutWrapped()-Variante, aber mit optionalem
    // Scroll-Offset und Sichtfenster (scrollY/viewTop/viewBottom) - Zeilen
    // ausserhalb des Sichtfensters werden uebersprungen. Mit draw=false wird
    // nur die Gesamthoehe berechnet, ohne etwas zu zeichnen (fuer die
    // Scroll-Bedarfspruefung vorab).
    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                          int16_t lineHeight, const String& text, int16_t scrollY,
                          int16_t viewTop, int16_t viewBottom, bool draw) {
        int16_t y = startY;
        int32_t start = 0;
        int32_t len = text.length();
        while (start < len) {
            while (start < len && text[start] == ' ') start++;
            if (start >= len) break;

            // Erzwungener Zeilenumbruch bei "\n" (z.B. fuer die
            // Aufzaehlungspunkte im OTA-Changelog, siehe changelog.h) -
            // Wortumbruch an Leerzeichen wird auf den Abschnitt VOR dem
            // naechsten "\n" begrenzt, damit ein "\n" nie einfach
            // ueberlesen wird. Vorher hatte ein eingebettetes "\n" gar
            // keine Wirkung auf das Layout.
            int32_t segEnd = text.indexOf('\n', start);
            if (segEnd < 0) segEnd = len;

            String line = text.substring(start, segEnd);
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
            // Falls die gezeichnete Zeile exakt bis zum "\n" reichte, jetzt
            // ueberspringen (es ist kein Leerzeichen, wuerde von der
            // Leerzeichen-Schleife oben sonst nicht entfernt).
            if (start < len && text[start] == '\n') start++;
        }
        return y;
    }

    // Zerlegt einen Titel-Text in bis zu maxLines Zeilen, die jeweils bei
    // der aktuell auf tft gesetzten Textgroesse in maxWidth passen (gleiches
    // Wortumbruch-Prinzip wie layoutWrapped(), nur ohne Scroll-Fenster, da
    // ein Titel immer komplett sichtbar sein muss statt gescrollt zu
    // werden). Frueher wurde bei zu langem Titel nur zwischen Textgroesse
    // 2/1 umgeschaltet, aber auch bei Groesse 1 konnte ein langer Titel
    // (z.B. "Pruefung fehlgeschlagen. WLAN pruefen.") immer noch breiter
    // als die Box sein und lief dann links/rechts ueber den Bildschirmrand
    // hinaus. Gibt die tatsaechliche Zeilenzahl zurueck (mindestens 1); im
    // seltenen Fall, dass der Text nach maxLines Zeilen immer noch nicht
    // vollstaendig umgebrochen ist, landet der Rest unveraendert in der
    // letzten erlaubten Zeile.
    int wrapTitleLines(TFT_eSPI& tft, const String& text, int16_t maxWidth, String* outLines, int maxLines) {
        int count = 0;
        int32_t start = 0;
        int32_t len = text.length();
        while (start < len && count < maxLines) {
            while (start < len && text[start] == ' ') start++;
            if (start >= len) break;

            String line = text.substring(start, len);
            bool isLastAllowedLine = (count == maxLines - 1);
            if (!isLastAllowedLine) {
                while (tft.textWidth(line) > maxWidth) {
                    int32_t lastSpace = line.lastIndexOf(' ');
                    if (lastSpace <= 0) break;
                    line = line.substring(0, lastSpace);
                }
            }
            outLines[count++] = line;
            start += line.length();
        }
        if (count == 0) {
            outLines[0] = text;
            count = 1;
        }
        return count;
    }

    // Warn-/Bestaetigungs-Ueberlage, die praktisch den kompletten Bildschirm
    // einnimmt (nur ein paar Pixel Rand) - urspruenglich nur fuers
    // Einschalten des Flugbuchs gebaut (erklaert, warum es sich nach 24h
    // automatisch wieder abschaltet), jetzt generisch mit uebergebenem
    // Titel-/Text-String, damit sie auch fuer die "Einstellungen
    // zuruecksetzen"-Bestaetigung (Werksreset) und die OTA-Update-
    // Bestaetigung wiederverwendet werden kann - alles seltene, potenziell
    // folgenreiche Aktionen, die dieselbe deutliche Bestaetigung verdienen.
    // accentColor (Default TFT_RED fuer die beiden bestehenden, wirklich
    // destruktiven Aufrufer) faerbt Rahmen und Titel - der OTA-Aufrufer
    // uebergibt TFT_GREEN, da ein Update zwar bestaetigt werden sollte,
    // aber keine "gefaehrliche" Loesch-Aktion wie Werksreset/Flugbuch ist.
    // title/body stehen VOR dem Aufruf per I18n::t() fest (statt StringIds
    // entgegenzunehmen), damit auch dynamisch zusammengesetzte Texte (z.B.
    // mit eingefuegter Versionsnummer bei OTA) moeglich sind. "Achtung!!!"
    // (title) steht ganz oben, mit einer Leerzeile Abstand zum Fliesstext
    // (body) darunter; der Text scrollt bei Bedarf (laengere Uebersetzungen)
    // ueber eigene Pfeil-Buttons, OK/Zurueck bleiben dabei immer unten fix
    // und kollisionsfrei sichtbar. Sternchen laufen im Hintergrund mit, wie
    // auf allen anderen Menue-Screens (nur der Radar-Screen selbst spart
    // sich das wegen der CPU-Last durch Abfragen/Zeichnen). Gibt true
    // zurueck, wenn "OK" angetippt wurde, false bei "Zurueck".
    bool confirmWarningScreen(TFT_eSPI& tft, const String& title, const String& body, uint16_t accentColor = TFT_RED) {
        constexpr int16_t BOX_X = 4;
        constexpr int16_t BOX_Y = 4;
        constexpr int16_t BOX_W = Config::SCREEN_WIDTH - 2 * BOX_X;
        constexpr int16_t BOX_H = Config::SCREEN_HEIGHT - 2 * BOX_Y;
        constexpr int16_t TEXT_MAX_WIDTH = BOX_W - 20;
        constexpr int16_t LINE_H = 16;
        constexpr int16_t TITLE_Y = BOX_Y + 16;

        // Titel-Text vorab in so viele Zeilen umbrechen, wie bei Groesse 2
        // (oder bei zu langem Text Groesse 1) noetig sind - siehe
        // wrapTitleLines() oben. VIEW_TOP (Start des Fliesstexts) haengt
        // dadurch von der tatsaechlichen Zeilenzahl des Titels ab, ist also
        // kein constexpr mehr wie vorher (wo immer nur eine Titelzeile
        // angenommen wurde).
        tft.setTextSize(2);
        uint8_t titleTextSize = 2;
        if (tft.textWidth(title) > TEXT_MAX_WIDTH) {
            tft.setTextSize(1);
            titleTextSize = 1;
        }
        constexpr int MAX_TITLE_LINES = 3;
        String titleLines[MAX_TITLE_LINES];
        int titleLineCount = wrapTitleLines(tft, title, TEXT_MAX_WIDTH, titleLines, MAX_TITLE_LINES);
        // Fliesstext wird immer bei Groesse 1 vermessen/gezeichnet (siehe
        // layoutWrapped()-Aufrufe unten) - Groesse hier zurücksetzen, falls
        // obiger Titel-Breitentest sie auf 2 stehen gelassen hat, sonst
        // wuerde die gleich folgende totalH-Berechnung (vor dem ersten
        // redraw()) mit falscher (zu breiter) Schriftgroesse rechnen.
        tft.setTextSize(1);
        // Eine Leerzeile Abstand zwischen Titel und Fliesstext.
        int16_t VIEW_TOP = (int16_t)(TITLE_Y + titleLineCount * LINE_H + 12);

        constexpr int16_t BTN_H = 36;
        constexpr int16_t BTN_GAP = 8;
        constexpr int16_t BOTTOM_MARGIN = 8;
        constexpr int16_t CANCEL_Y = BOX_Y + BOX_H - BOTTOM_MARGIN - BTN_H;
        constexpr int16_t OK_Y = CANCEL_Y - BTN_GAP - BTN_H;
        constexpr int16_t SCROLL_ROW_H = 28;
        constexpr int16_t SCROLL_ROW_GAP = 8;

        // Ohne Scroll-Pfeile verfuegbare Texthoehe zuerst pruefen - nur wenn
        // der Text da nicht reinpasst, wird zusaetzlich Platz fuer die
        // Pfeile reserviert (mehr Text-Platz bei kurzen Uebersetzungen).
        constexpr int16_t VIEW_BOTTOM_NO_SCROLL = OK_Y - 8;
        constexpr int16_t VIEW_BOTTOM_SCROLL = VIEW_BOTTOM_NO_SCROLL - SCROLL_ROW_H - SCROLL_ROW_GAP;

        int16_t totalH = layoutWrapped(tft, BOX_X + 10, VIEW_TOP, TEXT_MAX_WIDTH, LINE_H, body, 0, 0, 0, false);

        bool scrollable = (totalH - VIEW_BOTTOM_NO_SCROLL) > 0;
        int16_t viewBottom = scrollable ? VIEW_BOTTOM_SCROLL : VIEW_BOTTOM_NO_SCROLL;
        int16_t maxScroll = totalH - viewBottom;
        if (maxScroll < 0) maxScroll = 0;
        int16_t scrollY = 0;
        constexpr int16_t SCROLL_STEP = 48;

        Rect okBtn     = {(int16_t)(BOX_X + 10), OK_Y, (int16_t)(BOX_W - 20), BTN_H};
        Rect cancelBtn = {(int16_t)(BOX_X + 10), CANCEL_Y, (int16_t)(BOX_W - 20), BTN_H};
        int16_t scrollRowY = VIEW_BOTTOM_SCROLL + SCROLL_ROW_GAP;
        Rect upBtn   = {(int16_t)(BOX_X + BOX_W / 2 - 64), scrollRowY, 60, SCROLL_ROW_H};
        Rect downBtn = {(int16_t)(BOX_X + BOX_W / 2 + 4), scrollRowY, 60, SCROLL_ROW_H};

        MenuStars::reset();

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.drawRoundRect(BOX_X, BOX_Y, BOX_W, BOX_H, 6, accentColor);

            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(accentColor, TFT_BLACK);
            // Titel-Groesse/-Zeilen wurden oben vor der Layout-Berechnung
            // bereits einmalig ermittelt (titleTextSize/titleLines) - hier
            // nur noch zeichnen, ueber ggf. mehrere Zeilen gestapelt.
            tft.setTextSize(titleTextSize);
            for (int i = 0; i < titleLineCount; i++) {
                tft.drawString(titleLines[i], BOX_X + BOX_W / 2, (int16_t)(TITLE_Y + i * LINE_H));
            }
            tft.setTextSize(1);
            tft.setTextDatum(TL_DATUM);

            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            layoutWrapped(tft, BOX_X + 10, VIEW_TOP, TEXT_MAX_WIDTH, LINE_H, body, scrollY, VIEW_TOP, viewBottom, true);

            drawButton(tft, okBtn, I18n::t(StringId::OK));
            drawButton(tft, cancelBtn, I18n::t(StringId::BACK));
            if (scrollable) {
                drawButton(tft, upBtn, "^");
                drawButton(tft, downBtn, "v");
            }
        };

        redraw();

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (okBtn.contains(tap.x, tap.y)) return true;
                if (cancelBtn.contains(tap.x, tap.y)) return false;
                if (scrollable && upBtn.contains(tap.x, tap.y) && scrollY > 0) {
                    scrollY -= SCROLL_STEP;
                    if (scrollY < 0) scrollY = 0;
                    redraw();
                } else if (scrollable && downBtn.contains(tap.x, tap.y) && scrollY < maxScroll) {
                    scrollY += SCROLL_STEP;
                    if (scrollY > maxScroll) scrollY = maxScroll;
                    redraw();
                }
            }
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            // "false" (= Abbrechen) als sicherer Standard, da diese Funktion
            // fuer Warnungen wie die Werksreset-Bestaetigung genutzt wird.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) return false;
            MenuStars::update(tft);
            delay(20);
        }
    }

    // Einfacher Info-Screen mit nur EINEM Button (kein Abbrechen) - fuer
    // Endzustaende, bei denen es nichts mehr zu entscheiden gibt, nur zu
    // bestaetigen (z.B. Ergebnis eines OTA-Updates). Anders als
    // showBriefMessage() (kurze Meldung unten am Bildschirmrand,
    // verschwindet nach 1,2s automatisch von selbst) bleibt dieser Screen
    // stehen, bis aktiv bestaetigt wird - wichtig bei sicherheitsrelevanten
    // Meldungen wie einem fehlgeschlagenen oder erfolgreichen Firmware-
    // Update, die der Nutzer auf keinen Fall verpassen darf. Gleicher
    // Kasten-/Scroll-Aufbau wie confirmWarningScreen(), nur mit einem
    // einzigen, ueber die volle Breite gehenden Button statt OK/Zurueck.
    //
    // tappedLabel (optional, Standard leer): Manche Aufrufer (siehe
    // runOtaUpdateScreen() unten, Erfolgsfall) fuehren nach dem Antippen
    // noch eine kurze, aber spuerbar dauernde Aktion aus (hier:
    // ESP.restart(), das WLAN/Netzwerk-Cleanup vor dem eigentlichen Neustart
    // macht) - ohne sichtbare Rueckmeldung wirkte der Screen in dieser Zeit
    // eingefroren, und Alex' Testbericht zeigte mehrfaches frustriertes
    // Nachtippen auf den (eigentlich schon "erledigten") Button. Ist
    // tappedLabel gesetzt, wird der Button-Text beim Antippen SOFORT darauf
    // umgeschaltet (z.B. "Bitte warten, Geraet startet neu..."), bevor
    // infoScreen() zurueckkehrt und der Aufrufer seine (dauernde) Aktion
    // startet - der Tipp wurde also sichtbar registriert.
    void infoScreen(TFT_eSPI& tft, const String& title, const String& body, uint16_t accentColor,
                     const String& buttonLabel, const String& tappedLabel = "") {
        constexpr int16_t BOX_X = 4;
        constexpr int16_t BOX_Y = 4;
        constexpr int16_t BOX_W = Config::SCREEN_WIDTH - 2 * BOX_X;
        constexpr int16_t BOX_H = Config::SCREEN_HEIGHT - 2 * BOX_Y;
        constexpr int16_t TEXT_MAX_WIDTH = BOX_W - 20;
        constexpr int16_t LINE_H = 16;
        constexpr int16_t TITLE_Y = BOX_Y + 16;

        // Siehe wrapTitleLines()/confirmWarningScreen() oben - Titel kann
        // je nach Textlaenge/Sprache mehrere Zeilen brauchen, VIEW_TOP ist
        // deshalb kein constexpr mehr, sondern haengt von der tatsaechlich
        // benoetigten Zeilenzahl ab.
        tft.setTextSize(2);
        uint8_t titleTextSize = 2;
        if (tft.textWidth(title) > TEXT_MAX_WIDTH) {
            tft.setTextSize(1);
            titleTextSize = 1;
        }
        constexpr int MAX_TITLE_LINES = 3;
        String titleLines[MAX_TITLE_LINES];
        int titleLineCount = wrapTitleLines(tft, title, TEXT_MAX_WIDTH, titleLines, MAX_TITLE_LINES);
        // Fliesstext wird immer bei Groesse 1 vermessen/gezeichnet - siehe
        // Kommentar in confirmWarningScreen().
        tft.setTextSize(1);
        int16_t VIEW_TOP = (int16_t)(TITLE_Y + titleLineCount * LINE_H + 12);

        constexpr int16_t BTN_H = 40;
        constexpr int16_t BOTTOM_MARGIN = 10;
        constexpr int16_t BTN_Y = BOX_Y + BOX_H - BOTTOM_MARGIN - BTN_H;
        constexpr int16_t SCROLL_ROW_H = 28;
        constexpr int16_t SCROLL_ROW_GAP = 8;

        constexpr int16_t VIEW_BOTTOM_NO_SCROLL = BTN_Y - 8;
        constexpr int16_t VIEW_BOTTOM_SCROLL = VIEW_BOTTOM_NO_SCROLL - SCROLL_ROW_H - SCROLL_ROW_GAP;

        int16_t totalH = layoutWrapped(tft, BOX_X + 10, VIEW_TOP, TEXT_MAX_WIDTH, LINE_H, body, 0, 0, 0, false);

        bool scrollable = (totalH - VIEW_BOTTOM_NO_SCROLL) > 0;
        int16_t viewBottom = scrollable ? VIEW_BOTTOM_SCROLL : VIEW_BOTTOM_NO_SCROLL;
        int16_t maxScroll = totalH - viewBottom;
        if (maxScroll < 0) maxScroll = 0;
        int16_t scrollY = 0;
        constexpr int16_t SCROLL_STEP = 48;

        Rect okBtn = {(int16_t)(BOX_X + 10), BTN_Y, (int16_t)(BOX_W - 20), BTN_H};
        int16_t scrollRowY = VIEW_BOTTOM_SCROLL + SCROLL_ROW_GAP;
        Rect upBtn   = {(int16_t)(BOX_X + BOX_W / 2 - 64), scrollRowY, 60, SCROLL_ROW_H};
        Rect downBtn = {(int16_t)(BOX_X + BOX_W / 2 + 4), scrollRowY, 60, SCROLL_ROW_H};

        MenuStars::reset();

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.drawRoundRect(BOX_X, BOX_Y, BOX_W, BOX_H, 6, accentColor);

            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(accentColor, TFT_BLACK);
            // Titel-Groesse/-Zeilen wurden oben vor der Layout-Berechnung
            // bereits einmalig ermittelt (titleTextSize/titleLines).
            tft.setTextSize(titleTextSize);
            for (int i = 0; i < titleLineCount; i++) {
                tft.drawString(titleLines[i], BOX_X + BOX_W / 2, (int16_t)(TITLE_Y + i * LINE_H));
            }
            tft.setTextSize(1);
            tft.setTextDatum(TL_DATUM);

            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            layoutWrapped(tft, BOX_X + 10, VIEW_TOP, TEXT_MAX_WIDTH, LINE_H, body, scrollY, VIEW_TOP, viewBottom, true);

            drawButton(tft, okBtn, buttonLabel);
            if (scrollable) {
                drawButton(tft, upBtn, "^");
                drawButton(tft, downBtn, "v");
            }
        };

        redraw();

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (okBtn.contains(tap.x, tap.y)) {
                    // Siehe Kommentar bei tappedLabel oben - sofortige
                    // Rueckmeldung, dass der Tipp angekommen ist, bevor die
                    // (evtl. spuerbar dauernde) Aktion des Aufrufers startet.
                    if (tappedLabel.length() > 0) {
                        drawButton(tft, okBtn, tappedLabel);
                    }
                    return;
                }
                if (scrollable && upBtn.contains(tap.x, tap.y) && scrollY > 0) {
                    scrollY -= SCROLL_STEP;
                    if (scrollY < 0) scrollY = 0;
                    redraw();
                } else if (scrollable && downBtn.contains(tap.x, tap.y) && scrollY < maxScroll) {
                    scrollY += SCROLL_STEP;
                    if (scrollY > maxScroll) scrollY = maxScroll;
                    redraw();
                }
            }
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) return;
            MenuStars::update(tft);
            delay(20);
        }
    }

    // Fortschrittsanzeige waehrend OtaUpdate::performUpdate() laeuft -
    // gleiches Namespace-globale-Zeiger-Prinzip wie progressTft oben (siehe
    // Settings-Backup-Fortschrittspunkte), da OtaUpdate::performUpdate()
    // ebenfalls einen einfachen C-Funktionszeiger erwartet, keine
    // Lambda-Capture erlaubt.
    TFT_eSPI* otaProgressTft = nullptr;

    void drawOtaProgress(uint8_t percent) {
        if (!otaProgressTft) return;
        TFT_eSPI& t = *otaProgressTft;
        // Zwei Zeilen statt einer langen: der Praefix-Text
        // (OTA_INSTALLING_PREFIX) ist in manchen Sprachen zu lang, um
        // zusammen mit der Prozentzahl auf einer Zeile bei lesbarer
        // Schriftgroesse zu passen (lief vorher links/rechts ueber den
        // Bildschirmrand hinaus). Jetzt: Beschriftung klein oben, Prozent
        // gross darunter.
        constexpr int16_t BAND_H = 60;
        int16_t cy = Config::SCREEN_HEIGHT / 2;
        t.fillRect(0, (int16_t)(cy - BAND_H / 2), Config::SCREEN_WIDTH, BAND_H, TFT_BLACK);

        t.setTextDatum(MC_DATUM);
        t.setTextColor(TFT_GREEN, TFT_BLACK);
        t.setTextSize(1);
        t.drawString(I18n::t(StringId::OTA_INSTALLING_PREFIX), Config::SCREEN_WIDTH / 2, (int16_t)(cy - 14));

        String percentLabel = String(percent) + "%";
        t.setTextSize(3);
        t.drawString(percentLabel, Config::SCREEN_WIDTH / 2, (int16_t)(cy + 12));

        t.setTextSize(1);
        t.setTextDatum(TL_DATUM);
    }

    // Kompletter Ablauf fuer "Nach Update suchen" (System-Menue) - Pruefung
    // gegen GitHub-Releases, bei verfuegbarem Update explizite Bestaetigung
    // (confirmWarningScreen() mit gruenem statt rotem Akzent - ein Update
    // ist keine destruktive Aktion wie Werksreset, verdient aber trotzdem
    // eine bewusste Bestaetigung, da WLAN/Strom waehrend des Vorgangs nicht
    // unterbrochen werden sollten), danach Fortschrittsanzeige waehrend
    // Download+Flash. WICHTIG: startet NICHT mehr automatisch neu und
    // springt bei einem Fehler auch nicht einfach stillschweigend zurueck
    // ins Menue - jedes Ergebnis (Erfolg wie Fehler) wird ueber infoScreen()
    // als eigener, stehenbleibender Screen angezeigt, den der Nutzer aktiv
    // bestaetigen muss. Bei Erfolg startet erst ein expliziter Tap auf
    // "Jetzt neu starten" tatsaechlich neu.
    void runOtaUpdateScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(I18n::t(StringId::OTA_CHECKING), Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
        tft.setTextDatum(TL_DATUM);

        // NetTask (Core 0) waehrend der eigentlichen Netzwerk-Anfrage
        // pausieren - siehe net_task.h::pause() fuer die Begruendung
        // (geteilte WLAN-Funk-/TLS-Ressourcen zwischen Core 0 und dieser
        // OTA-Anfrage auf Core 1). Bewusst NUR um den eigentlichen
        // HTTPS-Aufruf herum, nicht um die anschliessende Bestaetigungs-
        // Anzeige, damit ADS-B/WebUI/Wetter nicht laenger als noetig
        // stillstehen, waehrend der Nutzer in Ruhe die Bestaetigung liest.
        // pause() wartet jetzt aktiv, bis NetTask wirklich sicher
        // pausierbar ist (nicht mitten in einer ADS-B-Anfrage) - kommt es
        // dabei zum Timeout, GAR NICHT erst mit der Update-Pruefung
        // fortfahren (sonst genau das urspruengliche Haenger-Risiko).
        if (!NetTask::pause()) {
            infoScreen(tft, I18n::t(StringId::OTA_NETWORK_BUSY), "", TFT_RED, I18n::t(StringId::OK));
            return;
        }
        OtaUpdate::CheckInfo info = OtaUpdate::checkForUpdate();
        NetTask::resume();

        if (info.result == OtaUpdate::CheckResult::Error) {
            infoScreen(tft, I18n::t(StringId::OTA_CHECK_FAILED), "", TFT_RED, I18n::t(StringId::OK));
            return;
        }
        if (info.result == OtaUpdate::CheckResult::UpToDate) {
            String upToDateTitle = String(I18n::t(StringId::OTA_UP_TO_DATE_PREFIX)) + info.latestVersion;
            infoScreen(tft, upToDateTitle, "", TFT_GREEN, I18n::t(StringId::OK));
            return;
        }

        String title = String(I18n::t(StringId::OTA_UPDATE_AVAILABLE_PREFIX)) + info.latestVersion;
        bool confirmed = confirmWarningScreen(tft, title, I18n::t(StringId::OTA_CONFIRM_BODY), TFT_GREEN);
        if (!confirmed) return;

        otaProgressTft = &tft;
        tft.fillScreen(TFT_BLACK);
        drawOtaProgress(0);
        // Gleicher Grund wie oben bei checkForUpdate() - waehrend des
        // eigentlichen Downloads/Flashens darf NetTask nicht gleichzeitig
        // um die WLAN-Funk-/TLS-Ressourcen konkurrieren.
        if (!NetTask::pause()) {
            otaProgressTft = nullptr;
            infoScreen(tft, I18n::t(StringId::OTA_NETWORK_BUSY), "", TFT_RED, I18n::t(StringId::OK));
            return;
        }
        bool ok = OtaUpdate::performUpdate(info.downloadUrl, drawOtaProgress);
        NetTask::resume();
        otaProgressTft = nullptr;

        if (ok) {
            // Bewusst KEIN automatischer Neustart mehr - der Nutzer
            // bestaetigt aktiv per Button, damit er den Erfolg auch wirklich
            // mitbekommt (vorher lief die Meldung nur 1,5s an, dann
            // Neustart - leicht zu verpassen).
            //
            // BEWUSST OHNE Changelog an dieser Stelle (war testweise kurz
            // drin, siehe Git-Historie): hier laeuft noch die ALTE, gerade
            // zu ersetzende Firmware - die kennt den Changelog-Text der NEU
            // heruntergeladenen Version gar nicht, der neue Code wird ja
            // erst nach ESP.restart() tatsaechlich ausgefuehrt. Stattdessen
            // zeigt main.cpp::showWhatsNewIfNeeded() den Changelog beim
            // naechsten Boot an, wenn wirklich schon die neue Firmware
            // laeuft (siehe dort).
            // BEWUSST OHNE tappedLabel: ESP.restart() direkt danach ist so
            // schnell, dass ein umgeschalteter Button-Text ohnehin nicht
            // mehr lesbar ist - er hat aber, weil er laenger als "Jetzt neu
            // starten" war, den Button-Text ueberlaufen lassen (Alex'
            // Meldung). Der Button zeigt jetzt einfach durchgehend nur noch
            // "Jetzt neu starten".
            infoScreen(tft, I18n::t(StringId::OTA_UPDATE_SUCCESS), I18n::t(StringId::OTA_SUCCESS_BODY),
                       TFT_GREEN, I18n::t(StringId::OTA_RESTART_BUTTON));
            // Setzt das Flag, das main.cpp::showWhatsNewIfNeeded() beim
            // naechsten Boot ausliest - siehe settings_store.h fuer die
            // Begruendung (Changelog-Screen soll NUR nach einem echten
            // OTA-Update erscheinen, nicht nach jedem simplen Neuflashen).
            SettingsStore::setOtaJustInstalled(true);
            ESP.restart();
        } else {
            // Bewusst ein stehenbleibender Info-Screen statt der alten
            // showBriefMessage() (1,2s, dann automatisch zurueck ins Menue)
            // - ein fehlgeschlagenes Firmware-Update ist keine
            // Nebensaechlichkeit, die man verpassen darf.
            infoScreen(tft, I18n::t(StringId::OTA_UPDATE_FAILED), I18n::t(StringId::OTA_FAILED_BODY),
                       TFT_RED, I18n::t(StringId::OK));
        }
    }

    // Neue Untermenues (SystemDisplay/SystemTools/FlightStatsLogbook/
    // FlightLed/FlightTools) - entstanden beim Aufraeumen der Flugoptionen-/
    // System-Seiten in grosse Kategorie-Buttons statt langer Einzelzeilen-
    // Listen (Alex' Wunsch, analog zum Hauptmenue). System und Flight sind
    // dadurch selbst jetzt auch Kategorie-Seiten (wie Main), keine flachen
    // Listen mehr.
    enum class Page {
        Main, Region, System, Flight, BackupReset,
        SystemDisplay, SystemTools,
        FlightLists, FlightStatsLogbook, FlightLed, FlightFilters, FlightTools
    };
}

void run(TFT_eSPI& tft, bool startAtFilters) {
    Page page = startAtFilters ? Page::FlightFilters : Page::Main;
    bool done = false;
    MenuStars::reset();

    while (!done) {
        tft.fillScreen(TFT_BLACK);

        if (page == Page::Main) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_SETTINGS));

            Rect regionBtn = catRowRect(0);
            Rect wifiBtn   = catRowRect(1);
            Rect systemBtn = catRowRect(2);
            Rect flightBtn = catRowRect(3);
            Rect backBtn   = catRowRect(4);

            drawButton(tft, regionBtn, I18n::t(StringId::MENU_CATEGORY_REGION));
            drawButton(tft, wifiBtn, I18n::t(StringId::MENU_CATEGORY_WIFI));
            drawButton(tft, systemBtn, I18n::t(StringId::MENU_CATEGORY_SYSTEM));
            if (OtaUpdate::isUpdateAvailable()) {
                // Gleicher kleiner roter Punkt wie am Menu-Button im Header
                // (main.cpp::drawMenuButton()) - zeigt schon auf der
                // Hauptseite des Menues, in welcher Kategorie sich das
                // Update versteckt, ohne dass man erst durchklicken muss.
                tft.fillCircle((int16_t)(systemBtn.x + systemBtn.w - 8), (int16_t)(systemBtn.y + 8), 4, TFT_RED);
                tft.drawCircle((int16_t)(systemBtn.x + systemBtn.w - 8), (int16_t)(systemBtn.y + 8), 4, TFT_BLACK);
            }
            drawButton(tft, flightBtn, I18n::t(StringId::MENU_CATEGORY_FLIGHT));
            drawButton(tft, backBtn, I18n::t(StringId::BACK));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
                // Bricht die AEUSSERE Seiten-Schleife (while (!done)) mit ab,
                // egal auf welcher Menue-Unterseite man gerade steht - kommt
                // dadurch beim Verlassen von run() automatisch beim
                // Radarscreen raus.
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (regionBtn.contains(tap.x, tap.y)) {
                page = Page::Region;
            } else if (wifiBtn.contains(tap.x, tap.y)) {
                WifiManageScreen::run(tft);
            } else if (systemBtn.contains(tap.x, tap.y)) {
                page = Page::System;
            } else if (flightBtn.contains(tap.x, tap.y)) {
                page = Page::Flight;
            } else if (backBtn.contains(tap.x, tap.y)) {
                done = true;
            }

        } else if (page == Page::Region) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_REGION));

            Rect languageBtn = rowRect(0);
            Rect unitsBtn    = rowRect(1);
            Rect backBtn     = rowRect(2);

            drawButton(tft, languageBtn, String(I18n::t(StringId::MENU_LANGUAGE)) + ": " + I18n::languageName(SettingsStore::language()));
            drawButton(tft, unitsBtn, I18n::t(StringId::MENU_UNITS));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
                // Bricht die AEUSSERE Seiten-Schleife (while (!done)) mit ab,
                // egal auf welcher Menue-Unterseite man gerade steht - kommt
                // dadurch beim Verlassen von run() automatisch beim
                // Radarscreen raus.
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (languageBtn.contains(tap.x, tap.y)) {
                LanguageScreen::run(tft);
            } else if (unitsBtn.contains(tap.x, tap.y)) {
                UnitsScreen::run(tft);
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Main;
            }

        } else if (page == Page::System) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_SYSTEM));

            // Kompakte Kategorie-Seite (3 grosse Buttons + Zurueck statt
            // vorher 11 einzelne Zeilen) - "Anzeige" und "Werkzeuge" fassen
            // die frueheren Einzelpunkte in zwei Untermenues zusammen (siehe
            // Page::SystemDisplay/Page::SystemTools unten). "Nach Update
            // suchen" bleibt bewusst ein eigener, sofort sichtbarer Button
            // (kein Untermenue, nur eine einzelne Aktion) - zeigt weiterhin
            // direkt Version + roten Punkt. Gleiche catRowRect()-Groesse wie
            // das Hauptmenue (Page::Main), passt auch mit 4 Zeilen (statt
            // dort 5) noch komfortabel auf den Bildschirm.
            Rect displayBtn      = catRowRect(0);
            Rect toolsBtn        = catRowRect(1);
            Rect checkUpdateBtn  = catRowRect(2);
            Rect backBtn         = catRowRect(3);

            drawButton(tft, displayBtn, I18n::t(StringId::MENU_CATEGORY_DISPLAY));
            drawButton(tft, toolsBtn, I18n::t(StringId::MENU_CATEGORY_SYSTEM_TOOLS));
            // Zeigt "Update verfuegbar: vX.X.X" statt der laufenden Version,
            // sobald der Hintergrund-Check (oder ein vorheriger manueller
            // Check) eins gefunden hat - der eigentliche Tastendruck fuehrt
            // trotzdem immer noch zu einem frischen checkForUpdate()-Aufruf
            // in runOtaUpdateScreen(), diese Zeile ist nur eine Vorschau.
            String checkUpdateLine1 = OtaUpdate::isUpdateAvailable()
                ? String(I18n::t(StringId::OTA_UPDATE_AVAILABLE_PREFIX)) + OtaUpdate::availableVersion()
                : String(I18n::t(StringId::CHECK_UPDATE_VERSION_PREFIX)) + Config::APP_VERSION;
            drawButtonTwoLines(tft, checkUpdateBtn, checkUpdateLine1, I18n::t(StringId::MENU_CHECK_UPDATE));
            if (OtaUpdate::isUpdateAvailable()) {
                // Gleicher kleiner roter Punkt wie an den anderen Stellen
                // (Menu-Button, "System"-Kachel) - hier zusaetzlich zur
                // bereits geaenderten Textzeile oben, damit der Button auch
                // beim schnellen Ueberfliegen der Seite auffaellt.
                tft.fillCircle((int16_t)(checkUpdateBtn.x + checkUpdateBtn.w - 8), (int16_t)(checkUpdateBtn.y + 8), 4, TFT_RED);
                tft.drawCircle((int16_t)(checkUpdateBtn.x + checkUpdateBtn.w - 8), (int16_t)(checkUpdateBtn.y + 8), 4, TFT_BLACK);
            }
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
                // Bricht die AEUSSERE Seiten-Schleife (while (!done)) mit ab,
                // egal auf welcher Menue-Unterseite man gerade steht - kommt
                // dadurch beim Verlassen von run() automatisch beim
                // Radarscreen raus.
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (displayBtn.contains(tap.x, tap.y)) {
                page = Page::SystemDisplay;
            } else if (toolsBtn.contains(tap.x, tap.y)) {
                page = Page::SystemTools;
            } else if (checkUpdateBtn.contains(tap.x, tap.y)) {
                runOtaUpdateScreen(tft);
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Main;
            }

        } else if (page == Page::SystemDisplay) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_DISPLAY));

            // Alles, was das Aussehen des Displays selbst betrifft
            // (Helligkeit, Timeout, Nachtmodus, Invertieren, Radar-
            // Farbschema) - vorher einzelne Zeilen auf der flachen System-
            // Liste, jetzt hier zusammengefasst (siehe Page::System oben).
            // "Radar-Darstellung" (RadarThemeScreen, enthaelt u.a. den
            // Radar-Puls/CRT-Phosphor-Effekt-Schalter) ganz nach oben, vor
            // "Helligkeit" (Alex' Wunsch nach besserer Auffindbarkeit).
            Rect radarThemeBtn = subMenuRowRect(0, 7);
            Rect brightnessBtn = subMenuRowRect(1, 7);
            Rect timeoutBtn    = subMenuRowRect(2, 7);
            Rect nightDimBtn   = subMenuRowRect(3, 7);
            Rect invertBtn     = subMenuRowRect(4, 7);
            // Fuer Tischmontage (GitHub-Meldung: Radarkreise "waschen" von
            // oben betrachtet aus, wegen der eingeschraenkten vertikalen
            // Blickwinkel des TFT-Panels) - dreht Bild UND Touch-Mapping um
            // 180 Grad, siehe SettingsStore::displayRotated180() und
            // TouchInput::setRotated180().
            Rect rotateBtn     = subMenuRowRect(5, 7);
            Rect backBtn       = subMenuRowRect(6, 7);

            drawButton(tft, radarThemeBtn, I18n::t(StringId::MENU_RADAR_THEME));
            drawButton(tft, brightnessBtn, brightnessLabel(SettingsStore::brightnessPercent()));
            drawButton(tft, timeoutBtn, screenTimeoutLabel(SettingsStore::screenTimeoutMinutes()));
            drawButton(tft, nightDimBtn, I18n::t(StringId::MENU_NIGHT_DIMMING) + onOff(SettingsStore::nightDimmingEnabled()));
            String invertLabel = SettingsStore::displayInverted()
                                      ? I18n::t(StringId::MENU_DISPLAY_INVERTED)
                                      : I18n::t(StringId::MENU_DISPLAY_NORMAL);
            drawButton(tft, invertBtn, invertLabel);
            drawButton(tft, rotateBtn, I18n::t(StringId::MENU_DISPLAY_ROTATE) + onOff(SettingsStore::displayRotated180()));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (brightnessBtn.contains(tap.x, tap.y)) {
                BrightnessScreen::run(tft);
            } else if (timeoutBtn.contains(tap.x, tap.y)) {
                // Vorher: Durchklicken per wiederholtem Antippen (0-10, ein
                // Tipp pro Minute - bei z.B. 10 Minuten also zehn Tipps).
                // Jetzt: eigener Screen mit Schieberegler, siehe
                // timeout_screen.cpp - dort lebt jetzt auch der
                // Ruhebildschirm-Umschalter (inhaltlich eng verwandt, und
                // dort ist Platz fuer eine kurze Erklaerung).
                TimeoutScreen::run(tft);
            } else if (nightDimBtn.contains(tap.x, tap.y)) {
                SettingsStore::setNightDimmingEnabled(!SettingsStore::nightDimmingEnabled());
            } else if (invertBtn.contains(tap.x, tap.y)) {
                bool newState = !SettingsStore::displayInverted();
                SettingsStore::setDisplayInverted(newState);
                tft.invertDisplay(newState);
            } else if (radarThemeBtn.contains(tap.x, tap.y)) {
                RadarThemeScreen::run(tft);
            } else if (rotateBtn.contains(tap.x, tap.y)) {
                bool newRotated = !SettingsStore::displayRotated180();
                SettingsStore::setDisplayRotated180(newRotated);
                tft.setRotation(newRotated ? 2 : 0);
                TouchInput::setRotated180(newRotated);
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::System;
            }

        } else if (page == Page::SystemTools) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_SYSTEM_TOOLS));

            // Wartungs-/Einrichtungs-Funktionen, die nicht taeglich
            // gebraucht werden - Kalibrierung, Web-Livekarte-Info und
            // Sicherung & Reset (das bestehende Page::BackupReset-Untermenue
            // bleibt unveraendert, wird jetzt nur eine Ebene tiefer erreicht:
            // System > Werkzeuge > Sicherung & Reset).
            Rect calibBtn       = subMenuRowRect(0, 4);
            Rect webuiBtn       = subMenuRowRect(1, 4);
            Rect backupResetBtn = subMenuRowRect(2, 4);
            Rect backBtn        = subMenuRowRect(3, 4);

            drawButton(tft, calibBtn, I18n::t(StringId::MENU_CALIBRATE));
            drawButton(tft, webuiBtn, I18n::t(StringId::MENU_LOGBOOK_WEBUI));
            drawButton(tft, backupResetBtn, I18n::t(StringId::MENU_BACKUP_RESET));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (calibBtn.contains(tap.x, tap.y)) {
                CalibrationScreen::run(tft);
            } else if (webuiBtn.contains(tap.x, tap.y)) {
                WebUiScreen::run(tft);
            } else if (backupResetBtn.contains(tap.x, tap.y)) {
                page = Page::BackupReset;
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::System;
            }

        } else if (page == Page::BackupReset) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_BACKUP_RESET));

            Rect backupBtn  = subMenuRowRect(0, 4);
            Rect restoreBtn = subMenuRowRect(1, 4);
            Rect resetBtn   = subMenuRowRect(2, 4);
            Rect backBtn    = subMenuRowRect(3, 4);

            drawButton(tft, backupBtn, I18n::t(StringId::MENU_BACKUP));
            drawButton(tft, restoreBtn, I18n::t(StringId::MENU_RESTORE));
            // Danger-Akzent (rot) - deutlich von Sichern/Wiederherstellen
            // abgesetzt, da diese Aktion (nach Bestaetigung) ALLE Daten
            // unwiderruflich loescht, siehe confirmWarningScreen() unten.
            drawButton(tft, resetBtn, I18n::t(StringId::MENU_FACTORY_RESET), false, true);
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
                // Bricht die AEUSSERE Seiten-Schleife (while (!done)) mit ab,
                // egal auf welcher Menue-Unterseite man gerade steht - kommt
                // dadurch beim Verlassen von run() automatisch beim
                // Radarscreen raus.
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (backupBtn.contains(tap.x, tap.y)) {
                progressTft = &tft;
                progressBtnRect = backupBtn;
                progressLabel = I18n::t(StringId::MENU_BACKUP);
                progressDots = 0;
                bool ok = SettingsBackup::backup(drawProgressStep);
                progressTft = nullptr;
                showBriefMessage(tft, I18n::t(ok ? StringId::MENU_BACKUP_SAVED : StringId::MENU_BACKUP_FAILED),
                                 ok ? TFT_GREEN : TFT_RED);
            } else if (restoreBtn.contains(tap.x, tap.y)) {
                if (SettingsBackup::hasBackup()) {
                    progressTft = &tft;
                    progressBtnRect = restoreBtn;
                    progressLabel = I18n::t(StringId::MENU_RESTORE);
                    progressDots = 0;
                    bool ok = SettingsBackup::restore(drawProgressStep);
                    progressTft = nullptr;
                    showBriefMessage(tft, I18n::t(ok ? StringId::MENU_RESTORED : StringId::MENU_RESTORE_FAILED),
                                     ok ? TFT_GREEN : TFT_RED);
                }
            } else if (resetBtn.contains(tap.x, tap.y)) {
                if (confirmWarningScreen(tft, I18n::t(StringId::MENU_LOGBOOK_WARNING_TITLE),
                                          I18n::t(StringId::MENU_FACTORY_RESET_WARNING_BODY))) {
                    tft.fillScreen(TFT_BLACK);
                    tft.setTextDatum(MC_DATUM);
                    tft.setTextColor(TFT_RED, TFT_BLACK);
                    tft.drawString(I18n::t(StringId::MENU_FACTORY_RESET_DELETING),
                                    Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
                    tft.setTextDatum(TL_DATUM);
                    // Erfolgsfall: factoryReset() startet das Geraet neu und
                    // kehrt nie zurueck - dieser Code danach laeuft nur im
                    // (seltenen) Fehlerfall (SD nicht eingehaengt) ueberhaupt
                    // weiter.
                    bool ok = SettingsBackup::factoryReset();
                    if (!ok) {
                        showBriefMessage(tft, I18n::t(StringId::MENU_FACTORY_RESET_FAILED), TFT_RED);
                    }
                }
            } else if (backBtn.contains(tap.x, tap.y)) {
                // Jetzt ueber Page::SystemTools erreicht (System > Werkzeuge
                // > Sicherung & Reset), nicht mehr direkt ueber Page::System -
                // "Zurueck" fuehrt deshalb dorthin zurueck statt zur System-
                // Kategorie-Seite, damit die Navigation stimmig bleibt.
                page = Page::SystemTools;
            }

        } else if (page == Page::Flight) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_FLIGHT));

            // Kompakte Kategorie-Seite (6 grosse Buttons) - Flugzeugliste
            // und Beobachtungsliste stecken jetzt in einem eigenen
            // "Listen"-Untermenue (vorher direkt hier als Buttons), die
            // reinen Sichtbarkeitsfilter in einem eigenen "Anzeigefilter"-
            // Untermenue (vorher Teil von "Werkzeuge") - Alex' Wunsch nach
            // klarerer Trennung. "Werkzeuge" bleibt bestehen, enthaelt aber
            // nur noch Standort-Presets und Beobachtungsalarm (siehe
            // Page::FlightLists/Page::FlightFilters/Page::FlightTools unten).
            Rect listsBtn        = subMenuRowRect(0, 6);
            Rect statsLogbookBtn = subMenuRowRect(1, 6);
            Rect ledBtn          = subMenuRowRect(2, 6);
            Rect filtersBtn      = subMenuRowRect(3, 6);
            Rect toolsBtn        = subMenuRowRect(4, 6);
            Rect backBtn         = subMenuRowRect(5, 6);

            drawButton(tft, listsBtn, I18n::t(StringId::MENU_CATEGORY_LISTS));
            drawButton(tft, statsLogbookBtn, I18n::t(StringId::MENU_CATEGORY_STATS_LOGBOOK));
            drawButton(tft, ledBtn, I18n::t(StringId::MENU_CATEGORY_LED));
            drawButton(tft, filtersBtn, I18n::t(StringId::MENU_CATEGORY_FILTERS));
            drawButton(tft, toolsBtn, I18n::t(StringId::MENU_CATEGORY_TOOLS));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (listsBtn.contains(tap.x, tap.y)) {
                page = Page::FlightLists;
            } else if (statsLogbookBtn.contains(tap.x, tap.y)) {
                page = Page::FlightStatsLogbook;
            } else if (ledBtn.contains(tap.x, tap.y)) {
                page = Page::FlightLed;
            } else if (filtersBtn.contains(tap.x, tap.y)) {
                page = Page::FlightFilters;
            } else if (toolsBtn.contains(tap.x, tap.y)) {
                page = Page::FlightTools;
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Main;
            }

        } else if (page == Page::FlightLists) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_LISTS));

            Rect aircraftListBtn = subMenuRowRect(0, 4);
            Rect watchlistBtn    = subMenuRowRect(1, 4);
            Rect squawkWatchBtn  = subMenuRowRect(2, 4);
            Rect backBtn         = subMenuRowRect(3, 4);

            drawButton(tft, aircraftListBtn, I18n::t(StringId::MENU_AIRCRAFT_LIST));
            drawButton(tft, watchlistBtn, I18n::t(StringId::MENU_WATCHLIST));
            drawButton(tft, squawkWatchBtn, I18n::t(StringId::MENU_SQUAWK_WATCHLIST));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (aircraftListBtn.contains(tap.x, tap.y)) {
                if (AircraftListScreen::run(tft)) {
                    // Ein Flugzeug wurde in der Liste ausgewaehlt - direkt bis
                    // zum Radar zurueckspringen (mit offenem Detail-Panel),
                    // statt in der Flugoptionen-Seite stehen zu bleiben.
                    done = true;
                }
            } else if (watchlistBtn.contains(tap.x, tap.y)) {
                AircraftWatchlistScreen::run(tft);
            } else if (squawkWatchBtn.contains(tap.x, tap.y)) {
                SquawkWatchlistScreen::run(tft);
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Flight;
            }

        } else if (page == Page::FlightStatsLogbook) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_STATS_LOGBOOK));

            Rect statsBtn        = subMenuRowRect(0, 5);
            Rect statsHistoryBtn = subMenuRowRect(1, 5);
            Rect logFilesBtn     = subMenuRowRect(2, 5);
            Rect logbookBtn      = subMenuRowRect(3, 5);
            Rect backBtn         = subMenuRowRect(4, 5);

            drawButton(tft, statsBtn, I18n::t(StringId::MENU_STATISTICS));
            drawButton(tft, statsHistoryBtn, I18n::t(StringId::MENU_STATS_HISTORY));
            drawButton(tft, logFilesBtn, I18n::t(StringId::MENU_LOGBOOK_FILES));
            drawButton(tft, logbookBtn, I18n::t(StringId::MENU_FLIGHT_LOGBOOK) + onOff(SettingsStore::flightLogbookEnabled()));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (statsBtn.contains(tap.x, tap.y)) {
                StatsScreen::run(tft);
            } else if (statsHistoryBtn.contains(tap.x, tap.y)) {
                StatsHistoryScreen::run(tft);
            } else if (logFilesBtn.contains(tap.x, tap.y)) {
                LogbookFilesScreen::run(tft);
            } else if (logbookBtn.contains(tap.x, tap.y)) {
                if (SettingsStore::flightLogbookEnabled()) {
                    // Ausschalten ist immer unbedenklich - keine Bestaetigung noetig.
                    SettingsStore::setFlightLogbookEnabled(false);
                    SettingsStore::setFlightLogbookEnabledAtEpoch(0);
                    SettingsStore::setFlightLogbookSessionFile("");
                } else if (confirmWarningScreen(tft, I18n::t(StringId::MENU_LOGBOOK_WARNING_TITLE),
                                                 I18n::t(StringId::MENU_LOGBOOK_WARNING_BODY))) {
                    SettingsStore::setFlightLogbookEnabled(true);
                    SettingsStore::setFlightLogbookEnabledAtEpoch((uint32_t)time(nullptr));
                    // Leerer Eintrag erzwingt eine frische Sitzungsdatei beim
                    // naechsten FlightLogbook::update() statt eine evtl. noch
                    // vorhandene alte Datei weiterzuschreiben.
                    SettingsStore::setFlightLogbookSessionFile("");
                }
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Flight;
            }

        } else if (page == Page::FlightLed) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_LED));

            Rect heartbeatBtn = subMenuRowRect(0, 4);
            Rect emergencyBtn = subMenuRowRect(1, 4);
            Rect proximityBtn = subMenuRowRect(2, 4);
            Rect backBtn      = subMenuRowRect(3, 4);

            drawButton(tft, heartbeatBtn, I18n::t(StringId::MENU_LED_HEARTBEAT) + onOff(SettingsStore::ledHeartbeatEnabled()));
            drawButton(tft, emergencyBtn, I18n::t(StringId::MENU_EMERGENCY_ALERT) + onOff(SettingsStore::emergencyAlertEnabled()));
            drawButton(tft, proximityBtn, I18n::t(StringId::MENU_PROXIMITY_LED) + onOff(SettingsStore::proximityAlertEnabled()));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (heartbeatBtn.contains(tap.x, tap.y)) {
                SettingsStore::setLedHeartbeatEnabled(!SettingsStore::ledHeartbeatEnabled());
            } else if (emergencyBtn.contains(tap.x, tap.y)) {
                SettingsStore::setEmergencyAlertEnabled(!SettingsStore::emergencyAlertEnabled());
            } else if (proximityBtn.contains(tap.x, tap.y)) {
                SettingsStore::setProximityAlertEnabled(!SettingsStore::proximityAlertEnabled());
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Flight;
            }

        } else if (page == Page::FlightFilters) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_FILTERS));

            // Kein Seiten-Header-"?"-Button mehr (frueher oben rechts, siehe
            // Git-Historie) - der ISS-Marker-Hilfetext haengt jetzt direkt an
            // der ISS-Marker-Zeile selbst (drawRowInfoButton() unten), analog
            // zu den neuen "?"-Buttons in radar_theme_screen.cpp. Zeilen
            // starten deshalb wieder beim normalen ROW_START_Y=18 (Default-
            // Parameter von subMenuRowRect()) statt der bisherigen, wegen des
            // Header-Buttons nach unten verschobenen 34.
            Rect airlineBtn        = subMenuRowRect(0, 6);
            Rect groundBtn         = subMenuRowRect(1, 6);
            Rect helicoptersBtn    = subMenuRowRect(2, 6);
            Rect lowAltitudeBtn    = subMenuRowRect(3, 6);
            Rect issMarkerBtn      = subMenuRowRect(4, 6);
            Rect backBtn           = subMenuRowRect(5, 6);

            drawButton(tft, airlineBtn, I18n::t(StringId::MENU_AIRLINE_FILTER));
            // Label jetzt "Bodenfahrzeuge anzeigen" statt "...ausblenden" -
            // Alex' Meldung: "ausblenden: AN" liest sich unlogisch (klingt,
            // als waere Ausblenden aktiv gewaehlt, obwohl "AN" hier eigentlich
            // "Fahrzeuge sind sichtbar" bedeuten sollte). Deshalb Anzeige-
            // Text UMGEKEHRT zum gespeicherten hideGroundVehicles()-Wert -
            // die Einstellung selbst (SettingsStore::hideGroundVehicles(),
            // Speicherformat, radar_screen.cpp/aircraft_list_screen.cpp/
            // web_export_server.cpp-Filterlogik) bleibt unveraendert, nur
            // wie es hier angezeigt wird, ist gedreht.
            drawButton(tft, groundBtn, I18n::t(StringId::MENU_HIDE_GROUND) + onOff(!SettingsStore::hideGroundVehicles()));
            drawButton(tft, helicoptersBtn, I18n::t(StringId::MENU_ONLY_HELICOPTERS) + onOff(SettingsStore::onlyHelicopters()));
            drawButton(tft, lowAltitudeBtn, I18n::t(StringId::MENU_ONLY_LOW_ALTITUDE) + onOff(SettingsStore::onlyLowAltitude()));
            // Kein Sichtbarkeitsfilter im engeren Sinne (blendet keine
            // Flugzeuge aus), aber thematisch am ehesten hier passend - "was
            // wird zusaetzlich auf dem Radar angezeigt". Siehe iss_tracker.h.
            drawButton(tft, issMarkerBtn, I18n::t(StringId::MENU_ISS_MARKER) + onOff(SettingsStore::issMarkerEnabled()));
            drawRowInfoButton(tft, issMarkerBtn);
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            // "?"-Info-Button zuerst pruefen (kleine Flaeche innerhalb der
            // ISS-Marker-Zeile) - sonst wuerde ein Tap darauf faelschlich als
            // Tap auf die ganze Zeile (Schalter umlegen) gewertet, gleiches
            // Prinzip wie in radar_theme_screen.cpp.
            if (rowInfoBtnRect(issMarkerBtn).contains(tap.x, tap.y)) {
                infoScreen(tft, I18n::t(StringId::ISS_MARKER_INFO_TITLE), I18n::t(StringId::ISS_MARKER_INFO_BODY),
                           TFT_GREEN, I18n::t(StringId::OK));
            } else if (airlineBtn.contains(tap.x, tap.y)) {
                AirlineFilterScreen::run(tft);
            } else if (groundBtn.contains(tap.x, tap.y)) {
                SettingsStore::setHideGroundVehicles(!SettingsStore::hideGroundVehicles());
            } else if (helicoptersBtn.contains(tap.x, tap.y)) {
                SettingsStore::setOnlyHelicopters(!SettingsStore::onlyHelicopters());
            } else if (lowAltitudeBtn.contains(tap.x, tap.y)) {
                SettingsStore::setOnlyLowAltitude(!SettingsStore::onlyLowAltitude());
            } else if (issMarkerBtn.contains(tap.x, tap.y)) {
                SettingsStore::setIssMarkerEnabled(!SettingsStore::issMarkerEnabled());
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Flight;
            }

        } else { // Page::FlightTools
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_TOOLS));

            Rect locationBtn       = subMenuRowRect(0, 3);
            Rect watchlistAlertBtn = subMenuRowRect(1, 3);
            Rect backBtn           = subMenuRowRect(2, 3);

            drawButton(tft, locationBtn, I18n::t(StringId::MENU_LOCATION_PRESETS));
            drawButton(tft, watchlistAlertBtn, I18n::t(StringId::MENU_WATCHLIST_ALERT) + onOff(SettingsStore::watchlistAlertEnabled()));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (locationBtn.contains(tap.x, tap.y)) {
                LocationPresetsScreen::run(tft);
            } else if (watchlistAlertBtn.contains(tap.x, tap.y)) {
                SettingsStore::setWatchlistAlertEnabled(!SettingsStore::watchlistAlertEnabled());
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Flight;
            }
        }
    }
}

void showInfoScreen(TFT_eSPI& tft, const String& title, const String& body,
                     uint16_t accentColor, const String& buttonLabel) {
    infoScreen(tft, title, body, accentColor, buttonLabel);
}

}
