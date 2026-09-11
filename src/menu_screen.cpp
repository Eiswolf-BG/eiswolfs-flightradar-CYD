#include "menu_screen.h"
#include "touch_input.h"
#include "calibration_screen.h"
#include "wifi_manage_screen.h"
#include "stats_screen.h"
#include "stats_history_screen.h"
#include "logbook_files_screen.h"
#include "flight_logbook.h"
#include "webui_screen.h"
#include "mqtt_screen.h"
#include "location_presets_screen.h"
#include "airline_filter_screen.h"
#include "aircraft_watchlist_screen.h"
#include "squawk_watchlist_screen.h"
#include "aircraft_list_screen.h"
#include "live_traffic_screen.h"
#include "connection_status_screen.h"
#include "brightness_screen.h"
#include "timeout_screen.h"
#include "menu_timeout_screen.h"
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
#include "github_screen_logo_image.h"
#include "changelog.h"
#include <time.h>
#include "ui_theme.h"

// In main.cpp definiert, ohne eigenen Header (globale Funktion, kein
// Namespace) - zeigt den GitHub-QR-Code-Screen mit Alex' Avatar-Bild
// (siehe github_screen_logo_image.h). Frueher ueber den inzwischen
// entfernten Header-Titel "Eiswolfs FR" erreichbar, seitdem verwaist -
// wird jetzt ueber den neuen "Über"-Menuepunkt (System > Werkzeuge) wieder
// erreichbar gemacht, ohne die bestehende Funktion/Logik anzufassen.
void runGithubQrScreen(TFT_eSPI& tftRef);

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
        uint16_t accent = danger ? TFT_RED : UiTheme::accentColor(tft);
        uint16_t bg = active ? accent : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : accent;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Wie drawButton(), aber mit einer optionalen zweiten, kleineren/
    // dezenteren Zeile darunter (aktuell nur fuer den Flugbuch-Abschalt-
    // Countdown gebraucht, siehe Page::FlightStatsLogbook) - eigene
    // Variante statt drawButton() um einen optionalen Parameter zu
    // erweitern, da drawButton() sonst ueberall nur eine Zeile zentriert
    // zeichnet. subLabel leer = identisch zu drawButton().
    void drawButtonWithSubline(TFT_eSPI& tft, const Rect& r, const String& label,
                                const String& subLabel) {
        if (subLabel.length() == 0) {
            drawButton(tft, r, label);
            return;
        }
        uint16_t accent = UiTheme::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        // Zwei Zeilen, mittig im oberen/unteren Drittel der Zeile platziert
        // (Zeilenhoehe 50px, Zeichenhoehe des Fonts bei Size 1 nur ~8-9px -
        // reichlich Abstand zueinander und zum Zeilenrahmen, siehe CLAUDE.md
        // Textbreiten-/-hoehen-Pflichtpruefung).
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, (int16_t)(r.y + r.h * 0.36f));
        tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.6f), TFT_BLACK);
        tft.drawString(subLabel, r.x + r.w / 2, (int16_t)(r.y + r.h * 0.74f));
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

    String onOff(bool on) { return I18n::t(on ? StringId::ON : StringId::OFF); }

    // Countdown-Text bis zur 24h-Sicherheitsabschaltung des Flugbuchs, fuer
    // die zweite Zeile in der Flugbuch-Ein/Aus-Zeile (siehe Page::
    // FlightStatsLogbook). Leerer String, wenn der Countdown gerade nicht
    // sinnvoll anzeigbar ist (siehe FlightLogbook::secondsUntilAutoOff()).
    String logbookCountdownText() {
        int32_t remaining = FlightLogbook::secondsUntilAutoOff();
        if (remaining < 0) return "";
        uint32_t hh = (uint32_t)remaining / 3600;
        uint32_t mm = ((uint32_t)remaining % 3600) / 60;
        char buf[24];
        if (hh > 0) {
            snprintf(buf, sizeof(buf), "%uh %umin", (unsigned)hh, (unsigned)mm);
        } else {
            snprintf(buf, sizeof(buf), "%umin", (unsigned)mm);
        }
        return String(I18n::t(StringId::FLIGHT_LOGBOOK_COUNTDOWN_PREFIX)) + buf;
    }

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

    // Gleiches Muster wie screenTimeoutLabel() oben, fuer den neuen Menue-
    // Timeout-Button (menu_timeout_screen.cpp) - "min"/"s" bleiben wie
    // ueberall sonst im Projekt unuebersetzt.
    String menuTimeoutLabel(uint16_t seconds) {
        String prefix = I18n::t(StringId::MENU_MENU_TIMEOUT_PREFIX);
        if (seconds == 0) return prefix + I18n::t(StringId::NEVER);
        if (seconds < 60) return prefix + String(seconds) + "s";
        uint16_t mins = seconds / 60;
        uint16_t secs = seconds % 60;
        if (secs == 0) return prefix + String(mins) + " min";
        return prefix + String(mins) + "min " + String(secs) + "s";
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
                if (lastSpace > 0) {
                    line = line.substring(0, lastSpace);
                    continue;
                }
                // Kein Leerzeichen mehr zum Umbrechen uebrig - typischerweise
                // eine lange URL ohne Leerzeichen (z.B. "github.com/Eiswolf-
                // BG/eiswolfs-flightradar-CYD" im GitHub-Stern-Hinweis, Alex'
                // Meldung: lief vorher ueber den Rand, mit einem Zeichen
                // mitten im Wort abgeschnitten). Zusaetzlich an Bindestrichen
                // umbrechbar - der Bindestrich bleibt dabei am Ende der
                // oberen Zeile stehen (uebliche Umbruch-Konvention), die
                // naechste Zeile beginnt direkt mit dem Folgezeichen. Bei
                // mehreren Bindestrichen wiederholt sich diese Schleife
                // automatisch, bis die Zeile passt oder kein Bindestrich mehr
                // uebrig ist (dann bricht sie wie zuvor einfach ab).
                //
                // WICHTIG: die Suche nach dem naechsten Bindestrich muss den
                // bereits am Zeilenende STEHENDEN Bindestrich (aus dem
                // vorherigen Schleifendurchlauf) ausklammern - sonst findet
                // lastIndexOf() bei jedem weiteren Durchlauf immer wieder
                // GENAU diesen einen, die Zeile bleibt dadurch unveraendert
                // und die Schleife haengt sich endlos auf (per Diagnose-Log
                // real reproduziert: das Geraet blieb beim Rendern des
                // GitHub-Stern-Hinweises stehen). Deshalb wird der bereits
                // vorhandene Bindestrich am Ende vor der Suche entfernt.
                String searchIn = line;
                if (searchIn.endsWith("-")) searchIn.remove(searchIn.length() - 1);
                int32_t lastHyphen = searchIn.lastIndexOf('-');
                if (lastHyphen > 0) {
                    line = line.substring(0, lastHyphen + 1);
                    continue;
                }
                break;
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
    // uebergibt UiTheme::accentColor(tft), da ein Update zwar bestaetigt werden sollte,
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

        // BUGFIX (Alex' Meldung: "Update installiert"-Bestaetigung flackerte
        // sichtbar) - MenuStars::update() verteilt seine Sterne zufaellig
        // ueber den GESAMTEN Bildschirm, ohne Ruecksicht auf bereits
        // gezeichneten Vordergrund-Inhalt, und pulsiert jeden Stern danach
        // dauerhaft an genau dieser festen Position (kein Loeschen/
        // Neuplatzieren). Diese Box hier deckt bis auf einen 4px-Rand die
        // GESAMTE Bildschirmflaeche ab - praktisch jeder der 34 Sterne
        // landet also zwangslaeufig auf Titel/Text/Button-Pixeln und
        // blinkt dort einzeln vor sich hin, was in der Summe wie
        // grossflaechiges Flackern aussieht. Anders als bei normalen
        // Menueseiten (kleine Buttons, viel echter schwarzer Hintergrund
        // dazwischen) gibt es hier keinen sinnvollen freien Bereich fuer
        // einen Sternenhintergrund - deshalb bewusst KEIN MenuStars::
        // reset()/update() mehr fuer diesen (und die beiden strukturell
        // identischen infoScreen()/twoPartInfoScreen()-) Dialoge.
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

            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
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
            // Inaktivitaets-Timeout - siehe SettingsStore::menuIdleTimeoutMs().
            // "false" (= Abbrechen) als sicherer Standard, da diese Funktion
            // fuer Warnungen wie die Werksreset-Bestaetigung genutzt wird.
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) return false;
            delay(20);
        }
    }

    // Kleine, herunterskalierte Kopie von Alex' Avatar-Bild (siehe
    // github_screen_logo_image.h, dort normalerweise 240x240px fuer den
    // GitHub-QR-Screen) fuer die OTA-Screens unten - Alex' Wunsch: "unten
    // mittig noch mein Logo platzieren". Dekodiert den RLE-Strom wie
    // main.cpp::drawGithubScreenLogo() zeilenweise (kein 115.200-Byte-
    // Vollbild-Puffer noetig), tastet dabei aber pro Ausgabezeile/-spalte
    // nur den naechstgelegenen Quellpixel ab (Nearest-Neighbor) statt
    // wirklich zu mitteln - fuer ein derart kleines Deko-Icon ausreichend
    // und ohne zusaetzlichen Rechenaufwand. Bricht die Dekodierung ab,
    // sobald alle "size" Ausgabezeilen gezeichnet sind, statt immer den
    // kompletten 240-Zeilen-Strom zu lesen.
    void drawSmallAvatarLogo(TFT_eSPI& t, int16_t centerX, int16_t topY, int16_t size) {
        uint16_t lineBuf[GITHUB_SCREEN_LOGO_W];
        uint16_t outBuf[GITHUB_SCREEN_LOGO_W]; // "size" bleibt <= 240 (Quellbildbreite), siehe Aufrufer
        int16_t lineFill = 0;
        int16_t row = 0;
        int16_t outRow = 0;
        size_t pos = 0;
        int16_t x = (int16_t)(centerX - size / 2);
        while (row < GITHUB_SCREEN_LOGO_H && outRow < size && pos + 2 < GITHUB_SCREEN_LOGO_RLE_LEN) {
            uint8_t count = GITHUB_SCREEN_LOGO_RLE[pos];
            uint16_t value = (uint16_t)GITHUB_SCREEN_LOGO_RLE[pos + 1] |
                              ((uint16_t)GITHUB_SCREEN_LOGO_RLE[pos + 2] << 8);
            pos += 3;

            while (count > 0) {
                int16_t spaceInLine = GITHUB_SCREEN_LOGO_W - lineFill;
                int16_t take = count < spaceInLine ? count : spaceInLine;
                for (int16_t i = 0; i < take; i++) lineBuf[lineFill + i] = value;
                lineFill += take;
                count -= take;
                if (lineFill == GITHUB_SCREEN_LOGO_W) {
                    int32_t srcForOutRow = (int32_t)outRow * GITHUB_SCREEN_LOGO_H / size;
                    if (row == srcForOutRow) {
                        for (int16_t c = 0; c < size; c++) {
                            int16_t srcCol = (int16_t)((int32_t)c * GITHUB_SCREEN_LOGO_W / size);
                            outBuf[c] = lineBuf[srcCol];
                        }
                        t.pushImage(x, (int16_t)(topY + outRow), size, 1, outBuf);
                        outRow++;
                    }
                    lineFill = 0;
                    row++;
                }
            }
        }
    }

    // Rein informativer, NICHT-interaktiver Erfolgs-Screen fuer den
    // automatischen Neustart nach einem OTA-Update (siehe
    // runOtaUpdateScreen() unten) - zeigt Titel + Text kurz an, OHNE
    // Button/Touch-Warteschleife. Ersetzt den frueheren "Jetzt neu
    // starten"-Button-Screen: dessen Warteschleife rief bei erreichtem
    // (mittlerweile einstellbarem) Menue-Timeout immer wieder komplett neu
    // infoScreen() auf, wenn seit dem letzten ECHTEN Tap (z.B. dem
    // Antippen von "Nach Update suchen" ganz am Anfang) schon laenger
    // nichts mehr angetippt wurde - das erzeugte ein sichtbares Dauer-
    // Flackern (Alex' Meldung). Ein automatischer Neustart nach einer
    // kurzen, fest bemessenen Lesepause (siehe Aufrufer) braucht keine
    // Touch-Warteschleife mehr und kann dieses Problem grundsaetzlich
    // nicht mehr haben. Gleicher Kasten-/Titel-/Text-Aufbau wie
    // infoScreen() unten, nur ohne Button/Scroll (der kurze Text passt in
    // allen 8 Sprachen ohne Scrollen).
    void drawOtaSuccessMessage(TFT_eSPI& tft, const String& title, const String& body, uint16_t accentColor) {
        constexpr int16_t BOX_X = 4;
        constexpr int16_t BOX_Y = 4;
        constexpr int16_t BOX_W = Config::SCREEN_WIDTH - 2 * BOX_X;
        constexpr int16_t BOX_H = Config::SCREEN_HEIGHT - 2 * BOX_Y;
        constexpr int16_t TEXT_MAX_WIDTH = BOX_W - 20;
        constexpr int16_t LINE_H = 16;
        constexpr int16_t TITLE_Y = BOX_Y + 16;

        tft.fillScreen(TFT_BLACK);
        tft.drawRoundRect(BOX_X, BOX_Y, BOX_W, BOX_H, 6, accentColor);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accentColor, TFT_BLACK);
        tft.setTextSize(2);
        uint8_t titleTextSize = 2;
        if (tft.textWidth(title) > TEXT_MAX_WIDTH) {
            tft.setTextSize(1);
            titleTextSize = 1;
        }
        constexpr int MAX_TITLE_LINES = 3;
        String titleLines[MAX_TITLE_LINES];
        int titleLineCount = wrapTitleLines(tft, title, TEXT_MAX_WIDTH, titleLines, MAX_TITLE_LINES);
        tft.setTextSize(titleTextSize);
        for (int i = 0; i < titleLineCount; i++) {
            tft.drawString(titleLines[i], BOX_X + BOX_W / 2, (int16_t)(TITLE_Y + i * LINE_H));
        }
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);

        // Logo unten mittig (Alex' Wunsch) - der Text-Bereich bekommt dafuer
        // ein reduziertes viewBottom, statt den Logo-Platz erst NACH dem
        // Zeichnen zu reservieren - so kann eine laengere Uebersetzung das
        // Logo nie ueberlappen (wird stattdessen wie ein normaler
        // Sichtfenster-Rand einfach nicht mehr gezeichnet, siehe
        // layoutWrapped()-Sichtfenster-Parameter oben).
        // 300% groesser (Alex' Wunsch) - 3x 28px -> 84px.
        constexpr int16_t LOGO_SIZE = 84;
        constexpr int16_t LOGO_BOTTOM_MARGIN = 8;
        int16_t logoTopY = (int16_t)(BOX_Y + BOX_H - LOGO_SIZE - LOGO_BOTTOM_MARGIN);
        int16_t textViewBottom = (int16_t)(logoTopY - 6);

        int16_t viewTop = (int16_t)(TITLE_Y + titleLineCount * LINE_H + 12);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        layoutWrapped(tft, BOX_X + 10, viewTop, TEXT_MAX_WIDTH, LINE_H, body, 0, 0, textViewBottom, true);

        drawSmallAvatarLogo(tft, Config::SCREEN_WIDTH / 2, logoTopY, LOGO_SIZE);
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
    // Rueckgabewert: true = per echtem Tap auf den Button beendet, false =
    // per Inaktivitaets-Timeout (SettingsStore::menuIdleTimeoutMs())
    // zurueckgekehrt, ohne dass der Nutzer tatsaechlich getippt hat - fuer
    // Aufrufer mit einer unbedingten, potenziell gefaehrlichen Folgeaktion
    // (z.B. ein Neustart) relevant, die meisten bestehenden Aufrufer
    // brauchen ihn nicht und koennen ihn wie bisher ignorieren (in C++
    // gefahrlos moeglich).
    bool infoScreen(TFT_eSPI& tft, const String& title, const String& body, uint16_t accentColor,
                     const String& buttonLabel, bool ignoreIdleTimeout = false) {
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

        // BUGFIX (Alex' Meldung: "Update installiert"-Bestaetigung
        // flackerte sichtbar) - siehe ausfuehrlicher Kommentar in
        // confirmWarningScreen() oben: die zufaellig ueber den GESAMTEN
        // Bildschirm verteilten, dauerhaft an fixer Position pulsierenden
        // MenuStars-Sterne landen bei dieser fast bildschirmfuellenden Box
        // zwangslaeufig auf Titel/Text/Button und blinken dort einzeln -
        // deshalb bewusst KEIN MenuStars::reset()/update() mehr hier.
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

            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
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
                    return true;
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
            // Inaktivitaets-Timeout - siehe SettingsStore::menuIdleTimeoutMs().
            // Bewusst uebersprungen, wenn ignoreIdleTimeout gesetzt ist
            // (Alex' Wunsch, Bugfix) - einziger bisheriger Aufrufer:
            // showWhatsNewIfNeeded() in main.cpp fuer den automatischen
            // "Was ist neu?"-Changelog-Screen direkt nach einem echten
            // OTA-Update. Dieser eine Screen MUSS stehen bleiben, bis aktiv
            // bestaetigt wird, unabhaengig vom konfigurierten Menue-
            // Timeout - ein potenziell kritischer Update-Hinweis darf nicht
            // im Hintergrund wegtimeouten, waehrend der Nutzer kurz
            // abgelenkt ist. Alle anderen ca. 15+ Aufrufer lassen den neuen
            // Parameter auf seinem Default (false) und unterliegen dem
            // Timeout weiterhin ganz normal.
            if (!ignoreIdleTimeout && TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) return false;
            delay(20);
        }
    }

    // Zwei-Themen-Variante von infoScreen() - fuer den kombinierten
    // "Update"-Button im System-Menue (Page::System), dessen einzelner
    // "?"-Button jetzt sowohl die Update-Suche als auch den LED-Hinweis-
    // Schalter erklaeren muss. Rendert body1, dann einen dezenten, nicht
    // bis zum Rand durchgezogenen Trennstrich (gleicher Stil wie der
    // interne Trennstrich im Button selbst), dann body2 - jeweils
    // eigenstaendig wortweise umgebrochen statt eines einzigen
    // durchlaufenden Fliesstexts, da layoutWrapped() keine Absaetze kennt.
    // Scroll-Logik ansonsten identisch zu infoScreen().
    void twoPartInfoScreen(TFT_eSPI& tft, const String& title, const String& body1, const String& body2,
                            uint16_t accentColor, const String& buttonLabel) {
        constexpr int16_t BOX_X = 4;
        constexpr int16_t BOX_Y = 4;
        constexpr int16_t BOX_W = Config::SCREEN_WIDTH - 2 * BOX_X;
        constexpr int16_t BOX_H = Config::SCREEN_HEIGHT - 2 * BOX_Y;
        constexpr int16_t TEXT_MAX_WIDTH = BOX_W - 20;
        constexpr int16_t LINE_H = 16;
        constexpr int16_t TITLE_Y = BOX_Y + 16;
        // Root Cause (per Live-Diagnose 31.08., siehe Chat-Verlauf): unser
        // Font ist BASELINE-verankert (siehe CLAUDE.md) - die Buchstaben
        // der ERSTEN body2-Zeile ragen ca. 10-11px UEBER ihre Baseline
        // (body2Start) nach oben hinaus. Der Trennstrich sass mit nur
        // SEP_GAP/2=7px Abstand VOR body2Start mitten in dieser
        // Aufragzone - "Trennstrich schneidet durch den ersten Satz von
        // body2" trotz rechnerisch korrekter body1End/body2Start-Werte.
        // Jetzt deutlich grosszuegiger UND asymmetrisch: Trennstrich sitzt
        // naeher an body1 (das hat selbst schon eingebaute Descent-
        // Reserve, siehe sepY-Formel unten), body2Start bekommt genug
        // Abstand, damit auch dessen Aufragzone sicher frei bleibt.
        constexpr int16_t SEP_GAP = 26; // Gesamtluecke zwischen body1End und body2Start

        tft.setTextSize(2);
        uint8_t titleTextSize = 2;
        if (tft.textWidth(title) > TEXT_MAX_WIDTH) {
            tft.setTextSize(1);
            titleTextSize = 1;
        }
        constexpr int MAX_TITLE_LINES = 3;
        String titleLines[MAX_TITLE_LINES];
        int titleLineCount = wrapTitleLines(tft, title, TEXT_MAX_WIDTH, titleLines, MAX_TITLE_LINES);
        tft.setTextSize(1);
        int16_t VIEW_TOP = (int16_t)(TITLE_Y + titleLineCount * LINE_H + 12);

        constexpr int16_t BTN_H = 40;
        constexpr int16_t BOTTOM_MARGIN = 10;
        constexpr int16_t BTN_Y = BOX_Y + BOX_H - BOTTOM_MARGIN - BTN_H;
        constexpr int16_t SCROLL_ROW_H = 28;
        constexpr int16_t SCROLL_ROW_GAP = 8;

        constexpr int16_t VIEW_BOTTOM_NO_SCROLL = BTN_Y - 8;
        constexpr int16_t VIEW_BOTTOM_SCROLL = VIEW_BOTTOM_NO_SCROLL - SCROLL_ROW_H - SCROLL_ROW_GAP;

        int16_t body1End = layoutWrapped(tft, BOX_X + 10, VIEW_TOP, TEXT_MAX_WIDTH, LINE_H, body1, 0, 0, 0, false);
        // sepY NICHT hinter body1End (das war der Bug), sondern 6px DAVOR -
        // body1End selbst liegt ja bereits einen vollen LINE_H unter body1s
        // tatsaechlicher letzter Baseline (baseline+16), waehrend Text nur
        // ein paar px Unterlaenge hat - dort ist also schon von Natur aus
        // reichlich Luft. body2Start bekommt den vollen SEP_GAP Abstand,
        // damit auch body2s Aufragzone (Baseline minus ~11px) sicher frei
        // bleibt.
        int16_t sepY = body1End - 6;
        int16_t body2Start = body1End + SEP_GAP;
        int16_t totalH = layoutWrapped(tft, BOX_X + 10, body2Start, TEXT_MAX_WIDTH, LINE_H, body2, 0, 0, 0, false);

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

        // BUGFIX (Alex' Meldung: "Update installiert"-Bestaetigung
        // flackerte sichtbar) - siehe ausfuehrlicher Kommentar in
        // confirmWarningScreen() oben: bewusst KEIN MenuStars::reset()/
        // update() mehr fuer diese fast bildschirmfuellende Box.
        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.drawRoundRect(BOX_X, BOX_Y, BOX_W, BOX_H, 6, accentColor);

            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(accentColor, TFT_BLACK);
            tft.setTextSize(titleTextSize);
            for (int i = 0; i < titleLineCount; i++) {
                tft.drawString(titleLines[i], BOX_X + BOX_W / 2, (int16_t)(TITLE_Y + i * LINE_H));
            }
            tft.setTextSize(1);
            tft.setTextDatum(TL_DATUM);

            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            layoutWrapped(tft, BOX_X + 10, VIEW_TOP, TEXT_MAX_WIDTH, LINE_H, body1, scrollY, VIEW_TOP, viewBottom, true);

            int16_t sepScreenY = sepY - scrollY;
            if (sepScreenY >= VIEW_TOP && sepScreenY <= viewBottom) {
                constexpr int16_t sepInset = 20;
                tft.drawFastHLine((int16_t)(BOX_X + 10 + sepInset), sepScreenY,
                                   (int16_t)(TEXT_MAX_WIDTH - 2 * sepInset), accentColor);
            }

            layoutWrapped(tft, BOX_X + 10, body2Start, TEXT_MAX_WIDTH, LINE_H, body2, scrollY, VIEW_TOP, viewBottom, true);

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
                if (okBtn.contains(tap.x, tap.y)) return;
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
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) return;
            delay(20);
        }
    }

    // Fortschrittsanzeige waehrend OtaUpdate::performUpdate() laeuft -
    // gleiches Namespace-globale-Zeiger-Prinzip wie progressTft oben (siehe
    // Settings-Backup-Fortschrittspunkte), da OtaUpdate::performUpdate()
    // ebenfalls einen einfachen C-Funktionszeiger erwartet, keine
    // Lambda-Capture erlaubt.
    TFT_eSPI* otaProgressTft = nullptr;

    // Layout-/Dedupe-Status fuer drawOtaProgress() unten - auf Namespace-
    // Ebene statt als function-static, damit resetOtaProgressLayout() sie
    // von aussen (runOtaUpdateScreen(), VOR dem ersten drawOtaProgress(0)-
    // Aufruf eines neuen Update-Versuchs) explizit zuruecksetzen kann. Ohne
    // diesen expliziten Reset gab es keine zuverlaessige Methode, "neuer
    // Versuch" von "normaler Aufruf mit percent==0" zu unterscheiden (siehe
    // Git-Historie: eine fruehere Version erkannte das faelschlich an
    // "percent==0", aber httpUpdate's Fortschritts-Callback liefert am
    // Downloadanfang oft MEHRERE Aufrufe mit noch abgerundet 0% - jeder
    // davon loeste faelschlich einen kompletten Band-Redraw aus, was genau
    // das gemeldete Flackern "bis ca. 2%" erklaerte).
    int16_t otaProgressPercentY = 0;
    int16_t otaProgressBarY = 0;
    int16_t otaProgressLastFillW = -1;
    int16_t otaProgressLastDrawnPercent = -1;
    // Zeitstempel des letzten TATSAECHLICHEN Prozent-/Balken-Redraws (Alex'
    // Meldung: "OTA-Download dauert seit dem Fortschrittsbalken spuerbar
    // laenger") - siehe Drossel-Kommentar unten in drawOtaProgress(). 0 hat
    // hier keine Sonderbedeutung (anders als otaProgressLastDrawnPercent's
    // -1), einfach nur der Startwert vor dem allerersten Redraw.
    uint32_t otaProgressLastDrawMs = 0;

    void resetOtaProgressLayout() {
        otaProgressLastDrawnPercent = -1;
        otaProgressLastFillW = -1;
        otaProgressLastDrawMs = 0;
    }

    // BUGFIX (Alex' Meldung: Download-Fortschrittsscreen flackert im Takt
    // der Prozentzahl): der Fortschritts-Callback von httpUpdate feuert pro
    // empfangenem Netzwerk-Chunk, nicht nur pro tatsaechlicher Prozent-
    // Aenderung (siehe ota_update.cpp::performUpdate()) - teilweise mehrere
    // echte Prozent-Aenderungen pro Sekunde in schnellen Download-Phasen
    // (per Diagnose-Log bestaetigt). Die fruehere Version loeschte bei
    // JEDEM Aufruf das KOMPLETTE Band (inkl. der eigentlich unveraenderten
    // Ueberschrift und des Hinweistexts) schwarz, bevor alles neu gezeichnet
    // wurde - das ergab bei mehreren Aufrufen pro Sekunde einen sichtbaren
    // Dauer-Flackerblitz des GESAMTEN Bands. Jetzt: Ueberschrift, Balken-
    // Rahmen und Hinweistext werden nur EINMAL pro Update-Versuch gezeichnet
    // (erkannt an einem Wechsel von otaProgressTft, siehe "freshStart"
    // unten), danach wird bei jedem weiteren Aufruf NUR noch die Prozent-
    // zahl (kleinflaechig geloescht+neu gezeichnet statt des ganzen Bands)
    // und die Balken-Fuellung (waechst nur, kein Loeschen noetig, da neue
    // Fuellung die alte immer vollstaendig ueberdeckt) aktualisiert.
    void drawOtaProgress(uint8_t percent) {
        if (!otaProgressTft) return;
        TFT_eSPI& t = *otaProgressTft;

        constexpr int16_t BAND_TOP = 20;
        constexpr int16_t X_MARGIN = 15;
        constexpr int16_t TEXT_MAX_WIDTH = Config::SCREEN_WIDTH - 2 * X_MARGIN;
        constexpr int16_t HEADING_LINE_H = 20;
        constexpr int16_t BAR_MARGIN = 24;
        constexpr int16_t BAR_W = Config::SCREEN_WIDTH - 2 * BAR_MARGIN;
        constexpr int16_t BAR_H = 18;
        // Grosszuegige Loesch-Box um die Size-3-Prozentzahl herum (deckt
        // Ziffernhoehe inkl. Unterlaenge sicher ab, siehe UiFont11pt).
        constexpr int16_t PERCENT_BOX_HALF_H = 14;

        // BUGFIX (Alex' Meldung: flackert weiterhin bis ca. 2%): "neuer
        // Versuch" wurde vorher zusaetzlich an "percent==0" erkannt - aber
        // httpUpdate's Fortschritts-Callback liefert am Downloadanfang oft
        // MEHRERE Aufrufe, bei denen der abgerundete Prozentwert noch 0
        // ist, bevor er auf 1 springt. Jeder dieser 0%-Aufrufe loeste
        // faelschlich wieder den kompletten Band-Redraw aus. Jetzt wird
        // "neuer Versuch" stattdessen EXPLIZIT von aussen signalisiert
        // (resetOtaProgressLayout(), von runOtaUpdateScreen() VOR dem
        // allerersten drawOtaProgress(0)-Aufruf jedes Versuchs aufgerufen) -
        // otaProgressLastDrawnPercent bleibt bis dahin auf seinem Ausgangs-
        // wert -1 stehen, ein echter Prozentwert (auch 0) erreicht diesen
        // Wert nie erneut.
        bool freshStart = (otaProgressLastDrawnPercent == -1);
        if (freshStart) {
            otaProgressLastFillW = -1;

            // Einmaliger Aufbau: Band leeren, Ueberschrift, Balken-Rahmen,
            // Hinweistext - alles, was sich waehrend des restlichen
            // Downloads nicht mehr aendert. Band reicht bis knapp an den
            // Bildschirmrand (statt vorher 260px), damit auch das jetzt
            // 84px grosse Logo (Alex' Wunsch: "300% groesser") beim naechsten
            // Versuch zuverlaessig mit geloescht wird.
            constexpr int16_t BAND_H = Config::SCREEN_HEIGHT - BAND_TOP - 4;
            t.fillRect(0, BAND_TOP, Config::SCREEN_WIDTH, BAND_H, TFT_BLACK);

            // Ueberschrift: deutlich groesser als frueher (Size 2 statt 1)
            // und weiter oben statt als kleiner Praefix direkt ueber der
            // Prozentzahl. OTA_INSTALLING_PREFIX ist inzwischen ein
            // vollstaendiger Satz (statt eines kurzen Fragments), der in
            // manchen Sprachen nicht in eine Zeile passt - deshalb
            // zeilenumbruchsicher ueber layoutWrapped() (gleiche Technik
            // wie beim OTA-Bestaetigungstext in confirmWarningScreen()
            // oben) statt eines einzelnen ungeschuetzten drawString()-
            // Aufrufs. layoutWrapped() liefert die tatsaechliche
            // Endposition zurueck, an der die naechsten Elemente
            // (Prozentzahl/Balken/Hinweis) dynamisch anschliessen - so
            // bleibt das Layout auch bei 1 vs. 2 Zeilen Ueberschrift
            // stimmig statt zu ueberlappen.
            t.setTextColor(UiTheme::accentColor(t), TFT_BLACK);
            t.setTextSize(2);
            int16_t headingY = (int16_t)(BAND_TOP + 14);
            int16_t headingEndY = layoutWrapped(t, X_MARGIN, headingY, TEXT_MAX_WIDTH, HEADING_LINE_H,
                                                 I18n::t(StringId::OTA_INSTALLING_PREFIX),
                                                 0, 0, Config::SCREEN_HEIGHT, true);

            otaProgressPercentY = (int16_t)(headingEndY + 22);
            // 34 statt vorher 24px Abstand zur Prozentzahl (Alex' Meldung:
            // Balken beruehrte die Prozentzahl, 10px mehr Luft noetig).
            otaProgressBarY = (int16_t)(otaProgressPercentY + 34);

            // Fortschrittsbalken-RAHMEN direkt unter der Prozentzahl -
            // gleicher abgerundeter Stil wie die Buttons (siehe
            // drawButton() oben, Eckenradius 4). Nur der Rahmen wird hier
            // einmalig gezeichnet, die Fuellung kommt weiter unten bei
            // jedem Aufruf dazu.
            t.fillRoundRect(BAR_MARGIN, otaProgressBarY, BAR_W, BAR_H, 4, TFT_BLACK);
            t.drawRoundRect(BAR_MARGIN, otaProgressBarY, BAR_W, BAR_H, 4, UiTheme::accentColor(t));

            // Dezenter Hinweistext ganz unten (OTA_INSTALLING_HINT) -
            // "Geraet waehrend des Updates bitte nicht ausstecken oder
            // ausschalten" o.ae., ebenfalls zeilenumbruchsicher ueber
            // layoutWrapped(), falls er in einer Sprache nicht in eine
            // Zeile passt.
            t.setTextSize(1);
            t.setTextColor(TFT_DARKGREY, TFT_BLACK);
            int16_t hintY = (int16_t)(otaProgressBarY + BAR_H + 16);
            int16_t hintEndY = layoutWrapped(t, X_MARGIN, hintY, TEXT_MAX_WIDTH, 14,
                                              I18n::t(StringId::OTA_INSTALLING_HINT),
                                              0, 0, Config::SCREEN_HEIGHT, true);
            t.setTextDatum(TL_DATUM);

            // Logo unten mittig (Alex' Wunsch, jetzt 300% groesser = 3x
            // 28px -> 84px) - Teil des einmaligen Aufbaus, da es sich
            // waehrend des Downloads nie aendert und sonst bei jedem
            // Prozent-Update unnoetig erneut gezeichnet wuerde. Position
            // dynamisch UNTER dem tatsaechlichen Ende des Hinweistexts
            // (layoutWrapped()-Rueckgabewert) statt an einer festen
            // Bildschirmposition - so kann das jetzt deutlich groessere
            // Logo den Hinweistext in keiner der 8 Sprachen ueberlappen,
            // selbst wenn dieser dort mal auf 2 Zeilen umbricht.
            constexpr int16_t LOGO_SIZE = 84;
            int16_t logoTopY = (int16_t)(hintEndY + 10);
            drawSmallAvatarLogo(t, Config::SCREEN_WIDTH / 2, logoTopY, LOGO_SIZE);
        }

        // Echte Aenderung? Sonst gibt es nichts zu aktualisieren (deckt den
        // Fall ab, dass der Chunk-Callback oefter feuert, als sich der
        // ganzzahlige Prozentwert tatsaechlich aendert).
        if (percent == otaProgressLastDrawnPercent) return;
        otaProgressLastDrawnPercent = percent;

        // BUGFIX (Alex' Meldung: OTA-Download dauert seit dem Fortschritts-
        // balken spuerbar laenger als vorher mit der reinen Prozentzahl) -
        // dieser Callback laeuft SYNCHRON auf demselben Download-Lese-Loop
        // wie httpUpdate.update() selbst (siehe ota_update.cpp): jede hier
        // verbrachte Millisekunde verzoegert den naechsten Netzwerk-Lese-
        // Aufruf direkt. drawString() mit dem eigenen FreeFont (UiFont11pt,
        // siehe CLAUDE.md) bei Size 3 sowie der wachsende fillRoundRect()-
        // Balken sind pro Aufruf zwar einzeln guenstig, laufen bei feiner
        // Chunk-Groesse aber leicht 100x waehrend eines einzigen Downloads -
        // das summiert sich spuerbar auf. Deshalb hier zusaetzlich zeitlich
        // gedrosselt (min. 150ms zwischen zwei tatsaechlichen Redraws), statt
        // bei JEDER Prozent-Aenderung sofort neu zu zeichnen. otaProgress-
        // LastDrawnPercent oben wird trotzdem bei JEDER Aenderung aktualisiert
        // (verhindert nur doppelte Arbeit bei identischem Prozentwert), die
        // Drossel greift NUR bei der eigentlichen Zeichenoperation - der
        // allererste Redraw (otaProgressLastDrawMs==0) sowie 100% (damit der
        // Endzustand garantiert sichtbar wird, auch wenn der letzte Chunk
        // innerhalb des Drossel-Fensters liegt) sind davon ausgenommen.
        constexpr uint32_t MIN_REDRAW_INTERVAL_MS = 150;
        uint32_t nowMs = millis();
        bool mustDraw = (percent >= 100) || (otaProgressLastDrawMs == 0);
        if (!mustDraw && (nowMs - otaProgressLastDrawMs) < MIN_REDRAW_INTERVAL_MS) return;
        otaProgressLastDrawMs = nowMs;

        // Prozentzahl: nur die kleine Box um den Text herum loeschen, NICHT
        // das ganze Band - vermeidet den Schwarz-Blitz bei jeder
        // Aktualisierung.
        t.fillRect(0, (int16_t)(otaProgressPercentY - PERCENT_BOX_HALF_H), Config::SCREEN_WIDTH,
                   (int16_t)(PERCENT_BOX_HALF_H * 2), TFT_BLACK);
        t.setTextDatum(MC_DATUM);
        t.setTextColor(UiTheme::accentColor(t), TFT_BLACK);
        t.setTextSize(3);
        t.drawString(String(percent) + "%", Config::SCREEN_WIDTH / 2, otaProgressPercentY);
        t.setTextDatum(TL_DATUM);
        t.setTextSize(1);

        // Balken-Fuellung waechst nur (percent steigt monoton) - die neue,
        // breitere Fuellung ueberdeckt die alte vollstaendig, kein
        // Loeschen noetig. Ueberspringt redundante Aufrufe, wenn sich die
        // gerundete Pixel-Breite trotz Prozent-Aenderung nicht veraendert
        // hat (z.B. bei 1%-Schritten auf einer 210px breiten Leiste).
        uint8_t clampedPercent = percent > 100 ? 100 : percent;
        int16_t fillW = (int16_t)((BAR_W - 4) * clampedPercent / 100);
        if (fillW > otaProgressLastFillW) {
            t.fillRoundRect((int16_t)(BAR_MARGIN + 2), (int16_t)(otaProgressBarY + 2), fillW, (int16_t)(BAR_H - 4), 3,
                             UiTheme::accentColor(t));
            otaProgressLastFillW = fillW;
        }
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
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
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
            infoScreen(tft, upToDateTitle, "", UiTheme::accentColor(tft), I18n::t(StringId::OK));
            return;
        }

        String title = String(I18n::t(StringId::OTA_UPDATE_AVAILABLE_PREFIX)) + info.latestVersion;
        bool confirmed = confirmWarningScreen(tft, title, I18n::t(StringId::OTA_CONFIRM_BODY), UiTheme::accentColor(tft));
        if (!confirmed) return;

        otaProgressTft = &tft;
        resetOtaProgressLayout(); // siehe dortiger Kommentar - Pflicht vor jedem neuen Versuch
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
            // Automatischer Neustart nach kurzer Lesepause, KEIN Button/
            // KEINE Touch-Warteschleife mehr (Alex' Entscheidung, nachdem
            // der vorherige "Jetzt neu starten"-Button-Screen ein Dauer-
            // Flackern verursachte - siehe drawOtaSuccessMessage()-Kommentar
            // oben fuer die ausfuehrliche Vorgeschichte/Begruendung).
            //
            // BEWUSST OHNE Changelog an dieser Stelle (war testweise kurz
            // drin, siehe Git-Historie): hier laeuft noch die ALTE, gerade
            // zu ersetzende Firmware - die kennt den Changelog-Text der NEU
            // heruntergeladenen Version gar nicht, der neue Code wird ja
            // erst nach ESP.restart() tatsaechlich ausgefuehrt. Stattdessen
            // zeigt main.cpp::showWhatsNewIfNeeded() den Changelog beim
            // naechsten Boot an, wenn wirklich schon die neue Firmware
            // laeuft (siehe dort).
            // GitHub-Stern-Hinweis und Auto-Neustart-Hinweis als
            // zusaetzliche Absaetze angehaengt (gleiches "\n\n"-Absatz-
            // Muster wie main.cpp::showWeatherInfo()).
            String successBody = String(I18n::t(StringId::OTA_SUCCESS_BODY)) + "\n\n" +
                                  I18n::t(StringId::OTA_AUTO_RESTART_HINT) + "\n\n" +
                                  I18n::t(StringId::OTA_GITHUB_STAR_HINT);
            drawOtaSuccessMessage(tft, I18n::t(StringId::OTA_UPDATE_SUCCESS), successBody,
                                  UiTheme::accentColor(tft));
            // Feste Lesepause statt einer Touch-Warteschleife - lang genug,
            // um den kurzen Text zu erfassen, kurz genug, um nicht
            // unnoetig zu nerven. Kein delay()/Watchdog-Risiko wie bei der
            // frueheren Touch-Schleife, da hier keine Bedingung wiederholt
            // geprueft wird.
            delay(4000);
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
    // FlightLed) - entstanden beim Aufraeumen der Flugoptionen-/
    // System-Seiten in grosse Kategorie-Buttons statt langer Einzelzeilen-
    // Listen (Alex' Wunsch, analog zum Hauptmenue). System und Flight sind
    // dadurch selbst jetzt auch Kategorie-Seiten (wie Main), keine flachen
    // Listen mehr. Das fruehere Page::FlightTools (Standort-Presets +
    // Beobachtungsalarm-Schalter) wurde wieder aufgeloest, siehe
    // Page::Flight unten.
    enum class Page {
        Main, Region, System, Flight, BackupReset,
        SystemDisplay, SystemTools,
        FlightLists, FlightStatsLogbook, FlightLed, FlightFilters
    };
}

void run(TFT_eSPI& tft, bool startAtFilters, bool startAtSystem) {
    Page page = startAtFilters ? Page::FlightFilters : (startAtSystem ? Page::System : Page::Main);
    bool done = false;
    MenuStars::reset();

    while (!done) {
        tft.fillScreen(TFT_BLACK);

        if (page == Page::Main) {
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
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
                // Inaktivitaets-Timeout - siehe SettingsStore::menuIdleTimeoutMs().
                // Bricht die AEUSSERE Seiten-Schleife (while (!done)) mit ab,
                // egal auf welcher Menue-Unterseite man gerade steht - kommt
                // dadurch beim Verlassen von run() automatisch beim
                // Radarscreen raus.
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
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
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
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
                // Inaktivitaets-Timeout - siehe SettingsStore::menuIdleTimeoutMs().
                // Bricht die AEUSSERE Seiten-Schleife (while (!done)) mit ab,
                // egal auf welcher Menue-Unterseite man gerade steht - kommt
                // dadurch beim Verlassen von run() automatisch beim
                // Radarscreen raus.
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
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
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_SYSTEM));

            // Kompakte Kategorie-Seite (3 grosse Buttons + Zurueck statt
            // vorher 11 einzelne Zeilen) - "Anzeige" und "Werkzeuge" fassen
            // die frueheren Einzelpunkte in zwei Untermenues zusammen (siehe
            // Page::SystemDisplay/Page::SystemTools unten). "Nach Update
            // suchen" bleibt bewusst ein eigener, sofort sichtbarer Button
            // (kein Untermenue, nur eine einzelne Aktion) - zeigt weiterhin
            // direkt Version + roten Punkt.
            //
            // Eigene, lokale Zeilenaufteilung statt catRowRect() fuer diese
            // Seite (der Update-Bereich ist bewusst hoeher als die anderen
            // Zeilen und traegt zusaetzlich den neuen LED-Schalter - passt
            // nicht mehr ins gleichfoermige catRowRect()-Raster). Endet
            // exakt bei SCREEN_HEIGHT-10, gleiche Bodenmarge wie
            // subMenuRowRect().
            constexpr int16_t SYS_START_Y = 28;
            constexpr int16_t SYS_GAP = 8;
            constexpr int16_t SYS_ROW_H = 50;
            // Update-Info (oben) und LED-Schalter (unten) teilen sich EINEN
            // gemeinsamen, durchgehenden Button-Rahmen (Alex' Wunsch - vorher
            // zwei getrennte Kaesten mit einer Linie dazwischen sah aus wie
            // zwei gestapelte Buttons statt einem zusammengehoerigen). Jede
            // Haelfte bleibt trotzdem unabhaengig antippbar, siehe
            // updateHalf/ledHalf unten.
            constexpr int16_t SYS_UPDATE_HALF_H = 52;
            constexpr int16_t SYS_LED_HALF_H = 52;

            Rect displayBtn = {10, SYS_START_Y, (int16_t)(Config::SCREEN_WIDTH - 20), SYS_ROW_H};
            Rect toolsBtn = {10, (int16_t)(displayBtn.y + SYS_ROW_H + SYS_GAP),
                              (int16_t)(Config::SCREEN_WIDTH - 20), SYS_ROW_H};
            // updateHalf = obere Haelfte des gemeinsamen Kastens (Version +
            // "Nach Update suchen"), ledHalf = untere Haelfte (LED-Schalter) -
            // beide zusammen ergeben den sichtbaren Aussenrahmen unten.
            Rect updateHalf = {10, (int16_t)(toolsBtn.y + SYS_ROW_H + SYS_GAP),
                                (int16_t)(Config::SCREEN_WIDTH - 20), SYS_UPDATE_HALF_H};
            Rect ledHalf = {10, (int16_t)(updateHalf.y + SYS_UPDATE_HALF_H),
                             (int16_t)(Config::SCREEN_WIDTH - 20), SYS_LED_HALF_H};
            Rect outerUpdateBox = {updateHalf.x, updateHalf.y, updateHalf.w,
                                    (int16_t)(SYS_UPDATE_HALF_H + SYS_LED_HALF_H)};
            Rect backBtn = {10, (int16_t)(ledHalf.y + SYS_LED_HALF_H + SYS_GAP),
                             (int16_t)(Config::SCREEN_WIDTH - 20), SYS_ROW_H};

            drawButton(tft, displayBtn, I18n::t(StringId::MENU_CATEGORY_DISPLAY));
            drawButton(tft, toolsBtn, I18n::t(StringId::MENU_CATEGORY_SYSTEM_TOOLS));

            // EIN gemeinsamer Rahmen fuer beide Haelften - kein zweiter
            // Aussenrahmen um die LED-Zeile mehr, keine durchgehende Linie
            // ueber die volle Breite zwischen den beiden Haelften.
            tft.fillRoundRect(outerUpdateBox.x, outerUpdateBox.y, outerUpdateBox.w, outerUpdateBox.h, 4, TFT_BLACK);
            tft.drawRoundRect(outerUpdateBox.x, outerUpdateBox.y, outerUpdateBox.w, outerUpdateBox.h, 4, UiTheme::accentColor(tft));

            // Obere Haelfte: Version/Update-Text, reiner Textinhalt ohne
            // eigenen Rahmen (der ist ja bereits der gemeinsame Aussenrahmen
            // oben) - gleiche Zwei-Zeilen-Optik wie vorher.
            // Zeigt "Update verfuegbar: vX.X.X" statt der laufenden Version,
            // sobald der Hintergrund-Check (oder ein vorheriger manueller
            // Check) eins gefunden hat - der eigentliche Tastendruck fuehrt
            // trotzdem immer noch zu einem frischen checkForUpdate()-Aufruf
            // in runOtaUpdateScreen(), diese Zeile ist nur eine Vorschau.
            String checkUpdateLine1 = OtaUpdate::isUpdateAvailable()
                ? String(I18n::t(StringId::OTA_UPDATE_AVAILABLE_PREFIX)) + OtaUpdate::availableVersion()
                : String(I18n::t(StringId::CHECK_UPDATE_VERSION_PREFIX)) + Config::APP_VERSION;
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            constexpr int16_t UPDATE_LINE_GAP = 14;
            tft.drawString(checkUpdateLine1, updateHalf.x + updateHalf.w / 2, updateHalf.y + updateHalf.h / 2 - UPDATE_LINE_GAP / 2);
            tft.drawString(I18n::t(StringId::MENU_CHECK_UPDATE), updateHalf.x + updateHalf.w / 2, updateHalf.y + updateHalf.h / 2 + UPDATE_LINE_GAP / 2);
            tft.setTextDatum(TL_DATUM);
            if (OtaUpdate::isUpdateAvailable()) {
                // Gleicher kleiner roter Punkt wie an den anderen Stellen
                // (Menu-Button, "System"-Kachel) - hier zusaetzlich zur
                // bereits geaenderten Textzeile oben, damit der Button auch
                // beim schnellen Ueberfliegen der Seite auffaellt. In die
                // OBERE LINKE Ecke verschoben (war vorher oben rechts) - die
                // obere rechte Ecke des gesamten Kastens gehoert jetzt dem
                // gemeinsamen "?"-Info-Button (siehe updateInfoBtn unten),
                // sonst wuerden sich beide ueberlappen.
                tft.fillCircle((int16_t)(updateHalf.x + 8), (int16_t)(updateHalf.y + 8), 4, TFT_RED);
                tft.drawCircle((int16_t)(updateHalf.x + 8), (int16_t)(updateHalf.y + 8), 4, TFT_BLACK);
            }

            // EIN gemeinsamer "?"-Info-Button oben rechts in der Ecke des
            // GESAMTEN Buttons (nicht mehr innerhalb der LED-Zeile) - deckt
            // ueber twoPartInfoScreen() jetzt BEIDE Themen ab (Update-Suche
            // + LED-Schalter), gleicher Eck-Stil wie auf anderen Screens mit
            // Seiten-Header-"?"-Button (z.B. location_presets_screen.cpp/
            // wifi_manage_screen.cpp).
            Rect updateInfoBtn = {(int16_t)(outerUpdateBox.x + outerUpdateBox.w - ROW_INFO_BTN_SIZE - ROW_INFO_BTN_PAD),
                                   (int16_t)(outerUpdateBox.y + ROW_INFO_BTN_PAD),
                                   ROW_INFO_BTN_SIZE, ROW_INFO_BTN_SIZE};
            drawButton(tft, updateInfoBtn, "?");

            // Kurzer interner Trennstrich zwischen den beiden Haelften - NUR
            // innerhalb des Rahmens, beruehrt nicht den linken/rechten Rand
            // (dezente Gliederung statt eines zweiten Aussenrahmens).
            {
                int16_t sepInset = 24;
                int16_t sepY = updateHalf.y + updateHalf.h;
                tft.drawFastHLine((int16_t)(outerUpdateBox.x + sepInset), sepY,
                                   (int16_t)(outerUpdateBox.w - 2 * sepInset), UiTheme::accentColor(tft));
            }

            // Untere Haelfte: LED-Schalter-Text, ebenfalls ohne eigenen
            // Rahmen. FEST auf zwei Zeilen aufgeteilt (Label / AN-AUS-
            // Status) statt einer einzigen zentrierten Zeile - bei
            // laengeren Uebersetzungen (z.B. Italienisch "Avviso LED
            // aggiornamento: ON") wurde eine einzige Zeile zu breit und
            // wirkte gequetscht, je nach Sprache unterschiedlich stark.
            // Zwei FESTE Zeilen (gleiches Prinzip wie die obere Haelfte mit
            // Version/"Nach Update suchen") sehen dadurch in JEDER der 6
            // Sprachen garantiert gleich aus, unabhaengig von der
            // jeweiligen Textlaenge. Der "?"-Button ist nicht mehr Teil
            // dieser Zeile (siehe updateInfoBtn oben) - Text darf deshalb
            // jetzt ueber die VOLLE Zeilenbreite zentriert werden.
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            // War vorher 14 - die Umrandung um den AN/AUS-Wert (siehe unten)
            // ragte dadurch bis in die Unterlaengen der Label-Zeile hinein
            // (z.B. das "g" in "aggiornamento"), sichtbare Ueberlappung. Auf
            // 24 vergroessert, damit auch bei Zeichen mit Unterlaenge
            // sicher Abstand bleibt.
            constexpr int16_t LED_LINE_GAP = 24;
            int16_t ledLabelY = ledHalf.y + ledHalf.h / 2 - LED_LINE_GAP / 2;
            int16_t ledValueY = ledHalf.y + ledHalf.h / 2 + LED_LINE_GAP / 2;
            int16_t ledCenterX = ledHalf.x + ledHalf.w / 2;
            tft.drawString(I18n::t(StringId::MENU_UPDATE_LED_SIGNAL), ledCenterX, ledLabelY);

            // AN/AUS-Wert bekommt eine eigene kleine Umrandung (gleicher
            // Stil wie der "?"-Button) - reiner Text ohne Rahmen sah nicht
            // wie ein antippbares Element aus, anders als bei den anderen
            // Ein/Aus-Schaltern im Projekt (volle Button-Flaeche mit Rahmen).
            String ledValueText = onOff(SettingsStore::updateLedSignalEnabled());
            int16_t ledValueTextW = tft.textWidth(ledValueText);
            // Box eng am Text orientiert statt am LED_LINE_GAP (Abstand
            // zwischen Label- und Wert-ZEILE, nicht die Texthoehe selbst) -
            // die Hoehe wurde dadurch vorher unnoetig gross (32px fuer eine
            // einzelne kurze Textzeile), wirkte klobig statt wie ein
            // kompakter Schalter. LED_VALUE_TEXT_H orientiert sich an der
            // ueblichen Ein-Zeilen-Texthoehe (LINE_H=16 an anderen Stellen
            // in dieser Datei), Padding knapp wie beim benachbarten
            // "?"-Button.
            constexpr int16_t LED_VALUE_TEXT_H = 16;
            constexpr int16_t LED_VALUE_PAD_X = 8;
            constexpr int16_t LED_VALUE_PAD_Y = 3;
            int16_t ledValueBoxW = ledValueTextW + 2 * LED_VALUE_PAD_X;
            int16_t ledValueBoxH = LED_VALUE_TEXT_H + 2 * LED_VALUE_PAD_Y;
            tft.drawRoundRect((int16_t)(ledCenterX - ledValueBoxW / 2), (int16_t)(ledValueY - ledValueBoxH / 2),
                               ledValueBoxW, ledValueBoxH, 4, UiTheme::accentColor(tft));
            tft.drawString(ledValueText, ledCenterX, ledValueY);
            tft.setTextDatum(TL_DATUM);

            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                // Inaktivitaets-Timeout - siehe SettingsStore::menuIdleTimeoutMs().
                // Bricht die AEUSSERE Seiten-Schleife (while (!done)) mit ab,
                // egal auf welcher Menue-Unterseite man gerade steht - kommt
                // dadurch beim Verlassen von run() automatisch beim
                // Radarscreen raus.
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            // "?"-Info-Button zuerst pruefen (kleine Flaeche oben rechts im
            // gesamten Update-Kasten) - sonst wuerde ein Tap darauf
            // faelschlich als Tap auf die obere Haelfte (Update-Suche
            // starten) gewertet. Deckt jetzt BEIDE Themen des Buttons ab
            // (Update-Suche + LED-Schalter), siehe twoPartInfoScreen() oben.
            if (updateInfoBtn.contains(tap.x, tap.y)) {
                twoPartInfoScreen(tft, I18n::t(StringId::UPDATE_BUTTON_INFO_TITLE),
                                   I18n::t(StringId::UPDATE_CHECK_INFO_BODY),
                                   I18n::t(StringId::UPDATE_LED_SIGNAL_INFO_BODY),
                                   UiTheme::accentColor(tft), I18n::t(StringId::OK));
            } else if (displayBtn.contains(tap.x, tap.y)) {
                page = Page::SystemDisplay;
            } else if (toolsBtn.contains(tap.x, tap.y)) {
                page = Page::SystemTools;
            } else if (updateHalf.contains(tap.x, tap.y)) {
                runOtaUpdateScreen(tft);
            } else if (ledHalf.contains(tap.x, tap.y)) {
                SettingsStore::setUpdateLedSignalEnabled(!SettingsStore::updateLedSignalEnabled());
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Main;
            }

        } else if (page == Page::SystemDisplay) {
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_DISPLAY));

            // Alles, was das Aussehen des Displays selbst betrifft
            // (Helligkeit, Timeout, Nachtmodus, Invertieren, Radar-
            // Farbschema) - vorher einzelne Zeilen auf der flachen System-
            // Liste, jetzt hier zusammengefasst (siehe Page::System oben).
            // "Radar-Darstellung" (RadarThemeScreen, enthaelt u.a. den
            // Radar-Puls/CRT-Phosphor-Effekt-Schalter) ganz nach oben, vor
            // "Helligkeit" (Alex' Wunsch nach besserer Auffindbarkeit).
            Rect radarThemeBtn  = subMenuRowRect(0, 8);
            Rect brightnessBtn  = subMenuRowRect(1, 8);
            Rect timeoutBtn     = subMenuRowRect(2, 8);
            // Direkt neben dem Bildschirm-Timeout-Button (Alex' Wunsch,
            // beide thematisch zusammengehoerig) - eigener Screen, siehe
            // menu_timeout_screen.cpp.
            Rect menuTimeoutBtn = subMenuRowRect(3, 8);
            Rect nightDimBtn    = subMenuRowRect(4, 8);
            Rect invertBtn      = subMenuRowRect(5, 8);
            // Fuer Tischmontage (GitHub-Meldung: Radarkreise "waschen" von
            // oben betrachtet aus, wegen der eingeschraenkten vertikalen
            // Blickwinkel des TFT-Panels) - dreht Bild UND Touch-Mapping um
            // 180 Grad, siehe SettingsStore::displayRotated180() und
            // TouchInput::setRotated180().
            Rect rotateBtn      = subMenuRowRect(6, 8);
            Rect backBtn        = subMenuRowRect(7, 8);

            drawButton(tft, radarThemeBtn, I18n::t(StringId::MENU_RADAR_THEME));
            drawButton(tft, brightnessBtn, brightnessLabel(SettingsStore::brightnessPercent()));
            drawButton(tft, timeoutBtn, screenTimeoutLabel(SettingsStore::screenTimeoutMinutes()));
            drawButton(tft, menuTimeoutBtn, menuTimeoutLabel(SettingsStore::menuIdleTimeoutSeconds()));
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
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
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
            } else if (menuTimeoutBtn.contains(tap.x, tap.y)) {
                MenuTimeoutScreen::run(tft);
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
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_SYSTEM_TOOLS));

            // Wartungs-/Einrichtungs-Funktionen, die nicht taeglich
            // gebraucht werden - Kalibrierung, Web-Livekarte-Info und
            // Sicherung & Reset (das bestehende Page::BackupReset-Untermenue
            // bleibt unveraendert, wird jetzt nur eine Ebene tiefer erreicht:
            // System > Werkzeuge > Sicherung & Reset).
            Rect calibBtn       = subMenuRowRect(0, 7);
            Rect webuiBtn       = subMenuRowRect(1, 7);
            Rect mqttBtn        = subMenuRowRect(2, 7);
            Rect connectionBtn  = subMenuRowRect(3, 7);
            Rect backupResetBtn = subMenuRowRect(4, 7);
            Rect aboutBtn       = subMenuRowRect(5, 7);
            Rect backBtn        = subMenuRowRect(6, 7);

            drawButton(tft, calibBtn, I18n::t(StringId::MENU_CALIBRATE));
            drawButton(tft, webuiBtn, I18n::t(StringId::MENU_LOGBOOK_WEBUI));
            drawButton(tft, mqttBtn, I18n::t(StringId::MENU_MQTT));
            drawButton(tft, connectionBtn, I18n::t(StringId::MENU_CONNECTION_STATUS));
            drawButton(tft, backupResetBtn, I18n::t(StringId::MENU_BACKUP_RESET));
            drawButton(tft, aboutBtn, I18n::t(StringId::MENU_ABOUT));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (calibBtn.contains(tap.x, tap.y)) {
                CalibrationScreen::run(tft);
            } else if (webuiBtn.contains(tap.x, tap.y)) {
                WebUiScreen::run(tft);
            } else if (mqttBtn.contains(tap.x, tap.y)) {
                MqttScreen::run(tft);
            } else if (connectionBtn.contains(tap.x, tap.y)) {
                ConnectionStatusScreen::run(tft);
            } else if (backupResetBtn.contains(tap.x, tap.y)) {
                page = Page::BackupReset;
            } else if (aboutBtn.contains(tap.x, tap.y)) {
                runGithubQrScreen(tft);
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::System;
            }

        } else if (page == Page::BackupReset) {
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
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
                // Inaktivitaets-Timeout - siehe SettingsStore::menuIdleTimeoutMs().
                // Bricht die AEUSSERE Seiten-Schleife (while (!done)) mit ab,
                // egal auf welcher Menue-Unterseite man gerade steht - kommt
                // dadurch beim Verlassen von run() automatisch beim
                // Radarscreen raus.
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
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
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_FLIGHT));

            // Kompakte Kategorie-Seite (6 grosse Buttons) - Flugzeugliste
            // und Beobachtungsliste stecken jetzt in einem eigenen
            // "Listen"-Untermenue (vorher direkt hier als Buttons), die
            // reinen Sichtbarkeitsfilter in einem eigenen "Anzeigefilter"-
            // Untermenue (vorher Teil von "Werkzeuge") - Alex' Wunsch nach
            // klarerer Trennung. Das fruehere "Werkzeuge"-Untermenue
            // (Page::FlightTools) wurde wieder aufgeloest, da nach
            // Entfernung des Beobachtungsalarm-Schalters nur noch
            // Standort-Presets uebrig blieb - der Button hier springt
            // deshalb direkt in LocationPresetsScreen::run() statt in ein
            // eigenes Untermenue.
            Rect listsBtn        = subMenuRowRect(0, 6);
            Rect statsLogbookBtn = subMenuRowRect(1, 6);
            Rect ledBtn          = subMenuRowRect(2, 6);
            Rect filtersBtn      = subMenuRowRect(3, 6);
            Rect locationBtn     = subMenuRowRect(4, 6);
            Rect backBtn         = subMenuRowRect(5, 6);

            drawButton(tft, listsBtn, I18n::t(StringId::MENU_CATEGORY_LISTS));
            drawButton(tft, statsLogbookBtn, I18n::t(StringId::MENU_CATEGORY_STATS_LOGBOOK));
            if (SettingsStore::flightLogbookEnabled()) {
                // Gleicher kleiner roter Punkt wie beim Update-Verfuegbar-
                // Hinweis (siehe Page::System oben) - zeigt schon auf dieser
                // Kategorie-Seite als reiner Status-/"Aufzeichnung laeuft"-
                // Punkt an, dass das Flugbuch gerade aktiv ist (und der
                // 24h-Sicherheits-Countdown implizit mitlaeuft, siehe
                // FlightLogbook::secondsUntilAutoOff()). Erscheint sofort
                // beim Einschalten, verschwindet sofort beim Ausschalten -
                // egal ob manuell oder durch die 24h-Abschaltung. Frueher
                // war das ein Alarm-Punkt fuer "wurde automatisch
                // abgeschaltet" (an flightLogbookAutoOffTriggered()
                // gekoppelt) - dieser Hinweis haengt jetzt nur noch am
                // "?"-Button in der Flugbuch-Zeile, siehe logbookAutoOffHint
                // weiter unten.
                tft.fillCircle((int16_t)(statsLogbookBtn.x + statsLogbookBtn.w - 8), (int16_t)(statsLogbookBtn.y + 8), 4, TFT_RED);
                tft.drawCircle((int16_t)(statsLogbookBtn.x + statsLogbookBtn.w - 8), (int16_t)(statsLogbookBtn.y + 8), 4, TFT_BLACK);
            }
            drawButton(tft, ledBtn, I18n::t(StringId::MENU_CATEGORY_LED));
            drawButton(tft, filtersBtn, I18n::t(StringId::MENU_CATEGORY_FILTERS));
            drawButton(tft, locationBtn, I18n::t(StringId::MENU_LOCATION_PRESETS));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
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
            } else if (locationBtn.contains(tap.x, tap.y)) {
                LocationPresetsScreen::run(tft);
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Main;
            }

        } else if (page == Page::FlightLists) {
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_LISTS));

            Rect aircraftListBtn = subMenuRowRect(0, 5);
            Rect liveTrafficBtn  = subMenuRowRect(1, 5);
            Rect watchlistBtn    = subMenuRowRect(2, 5);
            Rect squawkWatchBtn  = subMenuRowRect(3, 5);
            Rect backBtn         = subMenuRowRect(4, 5);

            drawButton(tft, aircraftListBtn, I18n::t(StringId::MENU_AIRCRAFT_LIST));
            drawButton(tft, liveTrafficBtn, I18n::t(StringId::MENU_LIVE_TRAFFIC));
            drawButton(tft, watchlistBtn, I18n::t(StringId::MENU_WATCHLIST));
            drawButton(tft, squawkWatchBtn, I18n::t(StringId::MENU_SQUAWK_WATCHLIST));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
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
            } else if (liveTrafficBtn.contains(tap.x, tap.y)) {
                LiveTrafficScreen::run(tft);
            } else if (watchlistBtn.contains(tap.x, tap.y)) {
                AircraftWatchlistScreen::run(tft);
            } else if (squawkWatchBtn.contains(tap.x, tap.y)) {
                SquawkWatchlistScreen::run(tft);
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Flight;
            }

        } else if (page == Page::FlightStatsLogbook) {
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
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
            drawButtonWithSubline(tft, logbookBtn, I18n::t(StringId::MENU_FLIGHT_LOGBOOK) + onOff(SettingsStore::flightLogbookEnabled()),
                                  SettingsStore::flightLogbookEnabled() ? logbookCountdownText() : "");
            // Punkt oben LINKS: reiner Status-/"Aufzeichnung laeuft"-Punkt,
            // zeigt an dass das Flugbuch aktiv ist (24h-Countdown laeuft
            // implizit mit, siehe FlightLogbook::secondsUntilAutoOff()) -
            // erscheint sofort beim Einschalten, verschwindet sofort beim
            // Ausschalten (manuell oder automatisch). Frueher war das ein
            // Alarm-Punkt fuer "wurde automatisch abgeschaltet"; diese
            // Bedeutung traegt jetzt nur noch der "?"-Button unten.
            if (SettingsStore::flightLogbookEnabled()) {
                tft.fillCircle((int16_t)(logbookBtn.x + 8), (int16_t)(logbookBtn.y + 8), 4, TFT_RED);
                tft.drawCircle((int16_t)(logbookBtn.x + 8), (int16_t)(logbookBtn.y + 8), 4, TFT_BLACK);
            }
            // "?"-Button oben RECHTS (wie beim ISS-Marker) - unveraendert:
            // Hinweis/Erklaerung auf die 24h-Sicherheitsabschaltung, NUR
            // solange sie tatsaechlich (und nicht durch bewusstes manuelles
            // Ausschalten) gegriffen hat, siehe
            // SettingsStore::flightLogbookAutoOffTriggered(). Ueberlappt
            // sich mit dem Punkt oben nie, da sich beide Bedingungen
            // gegenseitig ausschliessen (Punkt nur bei eingeschaltetem,
            // "?"-Hinweis nur bei automatisch ausgeschaltetem Flugbuch).
            bool logbookAutoOffHint = !SettingsStore::flightLogbookEnabled() && SettingsStore::flightLogbookAutoOffTriggered();
            if (logbookAutoOffHint) {
                drawRowInfoButton(tft, logbookBtn);
            }
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (statsBtn.contains(tap.x, tap.y)) {
                StatsScreen::run(tft);
            } else if (statsHistoryBtn.contains(tap.x, tap.y)) {
                StatsHistoryScreen::run(tft);
            } else if (logFilesBtn.contains(tap.x, tap.y)) {
                LogbookFilesScreen::run(tft);
            } else if (logbookAutoOffHint && rowInfoBtnRect(logbookBtn).contains(tap.x, tap.y)) {
                // "?"-Info-Button zuerst pruefen (kleine Flaeche innerhalb
                // der Flugbuch-Zeile) - sonst wuerde ein Tap darauf
                // faelschlich als Tap auf die ganze Zeile (Schalter
                // umlegen) gewertet, gleiches Prinzip wie beim ISS-Marker.
                infoScreen(tft, I18n::t(StringId::FLIGHT_LOGBOOK_AUTO_OFF_TITLE),
                           I18n::t(StringId::FLIGHT_LOGBOOK_AUTO_OFF_BODY),
                           UiTheme::accentColor(tft), I18n::t(StringId::OK));
            } else if (logbookBtn.contains(tap.x, tap.y)) {
                if (SettingsStore::flightLogbookEnabled()) {
                    // Ausschalten ist immer unbedenklich - keine Bestaetigung noetig.
                    SettingsStore::setFlightLogbookEnabled(false);
                    SettingsStore::setFlightLogbookEnabledAtEpoch(0);
                    SettingsStore::setFlightLogbookSessionFile("");
                    // Bewusstes manuelles Ausschalten - kein Hinweis-Punkt
                    // noetig (siehe SettingsStore::flightLogbookAutoOffTriggered()).
                    SettingsStore::setFlightLogbookAutoOffTriggered(false);
                } else if (confirmWarningScreen(tft, I18n::t(StringId::MENU_LOGBOOK_WARNING_TITLE),
                                                 I18n::t(StringId::MENU_LOGBOOK_WARNING_BODY))) {
                    SettingsStore::setFlightLogbookEnabled(true);
                    SettingsStore::setFlightLogbookEnabledAtEpoch((uint32_t)time(nullptr));
                    // Leerer Eintrag erzwingt eine frische Sitzungsdatei beim
                    // naechsten FlightLogbook::update() statt eine evtl. noch
                    // vorhandene alte Datei weiterzuschreiben.
                    SettingsStore::setFlightLogbookSessionFile("");
                    // Bewusstes manuelles Wiedereinschalten - Hinweis-Punkt
                    // einer evtl. vorherigen Auto-Abschaltung zuruecksetzen.
                    SettingsStore::setFlightLogbookAutoOffTriggered(false);
                }
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Flight;
            }

        } else if (page == Page::FlightLed) {
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_LED));

            Rect heartbeatBtn = subMenuRowRect(0, 6);
            Rect emergencyBtn = subMenuRowRect(1, 6);
            Rect proximityBtn = subMenuRowRect(2, 6);
            Rect proximityModeBtn = subMenuRowRect(3, 6);
            Rect webAudioBtn = subMenuRowRect(4, 6);
            Rect backBtn      = subMenuRowRect(5, 6);

            drawButton(tft, heartbeatBtn, I18n::t(StringId::MENU_LED_HEARTBEAT) + onOff(SettingsStore::ledHeartbeatEnabled()));
            drawButton(tft, emergencyBtn, I18n::t(StringId::MENU_EMERGENCY_ALERT) + onOff(SettingsStore::emergencyAlertEnabled()));
            drawButton(tft, proximityBtn, I18n::t(StringId::MENU_PROXIMITY_LED) + onOff(SettingsStore::proximityAlertEnabled()));
            // "Einfach"/"Intelligent"-Umschalter fuer die Naeherungsalarm-
            // Auswertungslogik (SettingsStore::proximityAlertSmartMode(),
            // siehe radar_screen.cpp::updateProximityAlert()) - wirkt nur,
            // wenn proximityBtn oben ebenfalls an ist, bleibt aber immer
            // sichtbar/bedienbar (keine dynamische Ein-/Ausblendung noetig).
            drawButton(tft, proximityModeBtn, String(I18n::t(StringId::MENU_PROXIMITY_ALERT_MODE)) +
                       I18n::t(SettingsStore::proximityAlertSmartMode() ? StringId::PROXIMITY_ALERT_MODE_SMART : StringId::PROXIMITY_ALERT_MODE_SIMPLE));
            drawRowInfoButton(tft, proximityModeBtn);
            // Web-Alarmton (Alex' Wunsch) - CYD hat keinen brauchbaren
            // Lautsprecher (bereits getestet/verworfen), der Ton laeuft
            // stattdessen per Web Audio API im Browser jedes Geraets, das
            // die Live-Radar-Webseite gerade offen hat (siehe
            // web_export_server.cpp). Dieser Schalter aktiviert/deaktiviert
            // die Funktion komplett, fuer ALLE Betrachter (SettingsStore::
            // webAudioAlertEnabled()).
            drawButton(tft, webAudioBtn, I18n::t(StringId::MENU_WEB_AUDIO_ALERT) + onOff(SettingsStore::webAudioAlertEnabled()));
            drawRowInfoButton(tft, webAudioBtn);
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            if (heartbeatBtn.contains(tap.x, tap.y)) {
                SettingsStore::setLedHeartbeatEnabled(!SettingsStore::ledHeartbeatEnabled());
            } else if (emergencyBtn.contains(tap.x, tap.y)) {
                SettingsStore::setEmergencyAlertEnabled(!SettingsStore::emergencyAlertEnabled());
            } else if (proximityBtn.contains(tap.x, tap.y)) {
                SettingsStore::setProximityAlertEnabled(!SettingsStore::proximityAlertEnabled());
            } else if (rowInfoBtnRect(proximityModeBtn).contains(tap.x, tap.y)) {
                // "?"-Info-Button zuerst pruefen (kleine Flaeche innerhalb
                // der Zeile) - sonst wuerde ein Tap darauf faelschlich als
                // Tap auf die ganze Zeile (Modus umschalten) gewertet,
                // gleiches Prinzip wie beim ISS-Marker/Flugbuch-Hinweis.
                infoScreen(tft, I18n::t(StringId::PROXIMITY_SMART_INFO_TITLE), I18n::t(StringId::PROXIMITY_SMART_INFO_BODY),
                           UiTheme::accentColor(tft), I18n::t(StringId::OK));
            } else if (proximityModeBtn.contains(tap.x, tap.y)) {
                SettingsStore::setProximityAlertSmartMode(!SettingsStore::proximityAlertSmartMode());
            } else if (rowInfoBtnRect(webAudioBtn).contains(tap.x, tap.y)) {
                infoScreen(tft, I18n::t(StringId::WEB_AUDIO_ALERT_INFO_TITLE), I18n::t(StringId::WEB_AUDIO_ALERT_INFO_BODY),
                           UiTheme::accentColor(tft), I18n::t(StringId::OK));
            } else if (webAudioBtn.contains(tap.x, tap.y)) {
                SettingsStore::setWebAudioAlertEnabled(!SettingsStore::webAudioAlertEnabled());
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Flight;
            }

        } else { // Page::FlightFilters
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
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
                if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            // "?"-Info-Button zuerst pruefen (kleine Flaeche innerhalb der
            // ISS-Marker-Zeile) - sonst wuerde ein Tap darauf faelschlich als
            // Tap auf die ganze Zeile (Schalter umlegen) gewertet, gleiches
            // Prinzip wie in radar_theme_screen.cpp.
            if (rowInfoBtnRect(issMarkerBtn).contains(tap.x, tap.y)) {
                infoScreen(tft, I18n::t(StringId::ISS_MARKER_INFO_TITLE), I18n::t(StringId::ISS_MARKER_INFO_BODY),
                           UiTheme::accentColor(tft), I18n::t(StringId::OK));
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
        }
    }
}

bool showInfoScreen(TFT_eSPI& tft, const String& title, const String& body,
                     uint16_t accentColor, const String& buttonLabel, bool ignoreIdleTimeout) {
    return infoScreen(tft, title, body, accentColor, buttonLabel, ignoreIdleTimeout);
}

int layoutTitleLines(TFT_eSPI& tft, const String& text, int16_t maxWidth,
                      String* outLines, int maxLines) {
    return wrapTitleLines(tft, text, maxWidth, outLines, maxLines);
}

}
