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
#include "aircraft_list_screen.h"
#include "about_screen.h"
#include "brightness_screen.h"
#include "language_screen.h"
#include "units_screen.h"
#include "settings_backup.h"
#include "settings_store.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"
#include <time.h>

namespace MenuScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // ROW_GAP/ROW_START_Y bleiben unveraendert (werden von flightRowRect()
    // weiter unten mitbenutzt, siehe FLIGHT_ROW_H).
    constexpr int16_t ROW_GAP = 1;
    constexpr int16_t ROW_START_Y = 18;

    // Region-Unterseite (Sprache/Einheiten/Zurueck, nur 3 Eintraege):
    // Zeilenhoehe/-abstand werden aus der tatsaechlich verfuegbaren
    // Bildschirmflaeche errechnet (gleiches Muster wie bei SYSTEM_ROW_H
    // weiter unten), statt die kleine, fuer volle Seiten (z.B.
    // Flugoptionen: 14 Eintraege) gedachte feste Hoehe (vorher 22px) zu
    // benutzen - die liess bei nur 3 Eintraegen fast den ganzen Bildschirm
    // leer und machte die Buttons winzig und schwer zu treffen.
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

    // Eigene, etwas kompaktere Zeilenhoehe NUR fuer die Flugoptionen-Seite:
    // die normale rowRect()-Hoehe (22px) liess bei 13 Eintraegen schon keinen
    // Platz mehr fuer einen weiteren - andere Seiten (Region/System) bleiben
    // bei der normalen Hoehe unveraendert, um dort nichts zu verschieben.
    constexpr int16_t FLIGHT_ROW_H = 20;
    Rect flightRowRect(uint8_t index) {
        return {10, (int16_t)(ROW_START_Y + index * (FLIGHT_ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), FLIGHT_ROW_H};
    }

    // Eigene, groesser berechnete Zeilenhoehe NUR fuer die System-Seite: mit
    // nur 10 Eintraegen (vs. z.B. 14 auf der Flugoptionen-Seite) liess die
    // normale rowRect()-Hoehe (22px) unten viel ungenutzten Platz frei. Die
    // Hoehe wird hier stattdessen aus der tatsaechlich verfuegbaren
    // Bildschirmflaeche errechnet, sodass die Buttons den Platz bis knapp
    // ueber den Bildschirmrand ausfuellen - andere Seiten bleiben bei ihren
    // eigenen, unveraenderten Zeilenhoehen.
    // War 10, bevor "Sicherung"/"Wiederherstellen" in die neue
    // "Sicherung & Reset"-Unterseite ausgelagert wurden (siehe
    // BACKUP_RESET_ROW_COUNT unten) und dort durch einen einzelnen
    // Ordner-Button ersetzt wurden: -2 (Backup/Restore raus) +1 (neuer
    // Ordner-Button) = 9.
    constexpr uint8_t SYSTEM_ROW_COUNT = 9;
    constexpr int16_t SYSTEM_ROW_GAP = 4;
    constexpr int16_t SYSTEM_START_Y = 18;
    constexpr int16_t SYSTEM_END_Y = Config::SCREEN_HEIGHT - 10;
    constexpr int16_t SYSTEM_ROW_H =
        (SYSTEM_END_Y - SYSTEM_START_Y - (SYSTEM_ROW_COUNT - 1) * SYSTEM_ROW_GAP) / SYSTEM_ROW_COUNT;
    Rect systemRowRect(uint8_t index) {
        return {10, (int16_t)(SYSTEM_START_Y + index * (SYSTEM_ROW_H + SYSTEM_ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), SYSTEM_ROW_H};
    }

    // Eigene Zeilenhoehe fuer die neue "Sicherung & Reset"-Unterseite (4
    // Eintraege: Sichern/Wiederherstellen/Zuruecksetzen/Zurueck) - gleiches
    // "aus verfuegbarem Platz errechnen"-Muster wie bei SYSTEM_ROW_H.
    // ROW_START_Y wird mitbenutzt (siehe oben, gemeinsam mit rowRect()/
    // flightRowRect()), nur GAP/COUNT/H sind eigene Werte.
    constexpr uint8_t BACKUP_RESET_ROW_COUNT = 4;
    constexpr int16_t BACKUP_RESET_ROW_GAP = 10;
    constexpr int16_t BACKUP_RESET_END_Y = Config::SCREEN_HEIGHT - 10;
    constexpr int16_t BACKUP_RESET_ROW_H =
        (BACKUP_RESET_END_Y - ROW_START_Y - (BACKUP_RESET_ROW_COUNT - 1) * BACKUP_RESET_ROW_GAP) / BACKUP_RESET_ROW_COUNT;
    Rect backupResetRowRect(uint8_t index) {
        return {10, (int16_t)(ROW_START_Y + index * (BACKUP_RESET_ROW_H + BACKUP_RESET_ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), BACKUP_RESET_ROW_H};
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

    String onOff(bool on) { return I18n::t(on ? StringId::ON : StringId::OFF); }

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

    // Warn-Ueberlage, die praktisch den kompletten Bildschirm einnimmt (nur
    // ein paar Pixel Rand) - urspruenglich nur fuers Einschalten des
    // Flugbuchs gebaut (erklaert, warum es sich nach 24h automatisch
    // wieder abschaltet), jetzt generisch mit uebergebenen Titel-/Text-
    // StringIds, damit sie auch fuer die "Einstellungen zuruecksetzen"-
    // Bestaetigung (Werksreset) wiederverwendet werden kann - beides sind
    // seltene, potenziell folgenreiche Aktionen, die dieselbe deutliche
    // Warnung verdienen. "Achtung!!!" (titleId) steht ganz oben, mit einer
    // Leerzeile Abstand zum Fliesstext (bodyId) darunter; der Text
    // scrollt bei Bedarf (laengere Uebersetzungen) ueber eigene Pfeil-
    // Buttons, OK/Zurueck bleiben dabei immer unten fix und
    // kollisionsfrei sichtbar. Sternchen laufen im Hintergrund mit, wie auf
    // allen anderen Menue-Screens (nur der Radar-Screen selbst spart sich
    // das wegen der CPU-Last durch Abfragen/Zeichnen). Gibt true zurueck,
    // wenn "OK" angetippt wurde, false bei "Zurueck".
    bool confirmWarningScreen(TFT_eSPI& tft, StringId titleId, StringId bodyId) {
        constexpr int16_t BOX_X = 4;
        constexpr int16_t BOX_Y = 4;
        constexpr int16_t BOX_W = Config::SCREEN_WIDTH - 2 * BOX_X;
        constexpr int16_t BOX_H = Config::SCREEN_HEIGHT - 2 * BOX_Y;
        constexpr int16_t TEXT_MAX_WIDTH = BOX_W - 20;
        constexpr int16_t LINE_H = 16;
        constexpr int16_t TITLE_Y = BOX_Y + 16;
        // Eine Leerzeile Abstand zwischen "Achtung!!!" und dem Fliesstext.
        constexpr int16_t VIEW_TOP = TITLE_Y + 12 + LINE_H;

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

        String body = I18n::t(bodyId);
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
            tft.drawRoundRect(BOX_X, BOX_Y, BOX_W, BOX_H, 6, TFT_RED);

            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setTextSize(2);
            tft.drawString(I18n::t(titleId), BOX_X + BOX_W / 2, TITLE_Y);
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
            MenuStars::update(tft);
            delay(20);
        }
    }

    enum class Page { Main, Region, System, Flight, BackupReset };
}

void run(TFT_eSPI& tft) {
    Page page = Page::Main;
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
            drawButton(tft, flightBtn, I18n::t(StringId::MENU_CATEGORY_FLIGHT));
            drawButton(tft, backBtn, I18n::t(StringId::BACK));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
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

            Rect calibBtn     = systemRowRect(0);
            Rect invertBtn    = systemRowRect(1);
            Rect brightnessBtn = systemRowRect(2);
            Rect timeoutBtn   = systemRowRect(3);
            Rect nightDimBtn  = systemRowRect(4);
            Rect webuiBtn     = systemRowRect(5);
            // "Sicherung & Reset" (Backup/Restore/Werksreset, siehe
            // Page::BackupReset unten) steht bewusst direkt ueber "Info",
            // das seinerseits als letzter Punkt direkt ueber dem Zurueck-
            // Button steht - so bleiben beide Positionen stabil, egal wie
            // viele weitere Punkte davor noch dazukommen.
            Rect backupResetBtn = systemRowRect(6);
            Rect aboutBtn     = systemRowRect(7);
            Rect backBtn      = systemRowRect(8);

            drawButton(tft, calibBtn, I18n::t(StringId::MENU_CALIBRATE));

            String invertLabel = SettingsStore::displayInverted()
                                      ? I18n::t(StringId::MENU_DISPLAY_INVERTED)
                                      : I18n::t(StringId::MENU_DISPLAY_NORMAL);
            drawButton(tft, invertBtn, invertLabel);

            drawButton(tft, brightnessBtn, brightnessLabel(SettingsStore::brightnessPercent()));
            drawButton(tft, timeoutBtn, screenTimeoutLabel(SettingsStore::screenTimeoutMinutes()));
            drawButton(tft, nightDimBtn, I18n::t(StringId::MENU_NIGHT_DIMMING) + onOff(SettingsStore::nightDimmingEnabled()));
            drawButton(tft, webuiBtn, I18n::t(StringId::MENU_LOGBOOK_WEBUI));
            drawButton(tft, backupResetBtn, I18n::t(StringId::MENU_BACKUP_RESET));
            drawButton(tft, aboutBtn, I18n::t(StringId::MENU_ABOUT));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                MenuStars::update(tft);
                delay(20);
            }

            if (calibBtn.contains(tap.x, tap.y)) {
                CalibrationScreen::run(tft);
            } else if (invertBtn.contains(tap.x, tap.y)) {
                bool newState = !SettingsStore::displayInverted();
                SettingsStore::setDisplayInverted(newState);
                tft.invertDisplay(newState);
            } else if (brightnessBtn.contains(tap.x, tap.y)) {
                BrightnessScreen::run(tft);
            } else if (timeoutBtn.contains(tap.x, tap.y)) {
                uint8_t current = SettingsStore::screenTimeoutMinutes();
                uint8_t next = (current >= 10) ? 0 : (current + 1);
                SettingsStore::setScreenTimeoutMinutes(next);
            } else if (nightDimBtn.contains(tap.x, tap.y)) {
                SettingsStore::setNightDimmingEnabled(!SettingsStore::nightDimmingEnabled());
            } else if (backupResetBtn.contains(tap.x, tap.y)) {
                page = Page::BackupReset;
            } else if (aboutBtn.contains(tap.x, tap.y)) {
                AboutScreen::run(tft);
            } else if (webuiBtn.contains(tap.x, tap.y)) {
                WebUiScreen::run(tft);
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Main;
            }

        } else if (page == Page::BackupReset) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_BACKUP_RESET));

            Rect backupBtn  = backupResetRowRect(0);
            Rect restoreBtn = backupResetRowRect(1);
            Rect resetBtn   = backupResetRowRect(2);
            Rect backBtn    = backupResetRowRect(3);

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
                MenuStars::update(tft);
                delay(20);
            }

            if (backupBtn.contains(tap.x, tap.y)) {
                bool ok = SettingsBackup::backup();
                showBriefMessage(tft, I18n::t(ok ? StringId::MENU_BACKUP_SAVED : StringId::MENU_BACKUP_FAILED),
                                 ok ? TFT_GREEN : TFT_RED);
            } else if (restoreBtn.contains(tap.x, tap.y)) {
                if (SettingsBackup::hasBackup()) {
                    bool ok = SettingsBackup::restore();
                    showBriefMessage(tft, I18n::t(ok ? StringId::MENU_RESTORED : StringId::MENU_RESTORE_FAILED),
                                     ok ? TFT_GREEN : TFT_RED);
                }
            } else if (resetBtn.contains(tap.x, tap.y)) {
                if (confirmWarningScreen(tft, StringId::MENU_LOGBOOK_WARNING_TITLE,
                                          StringId::MENU_FACTORY_RESET_WARNING_BODY)) {
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
                page = Page::System;
            }

        } else { // Page::Flight
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_FLIGHT));

            // Reine An/Aus-Schalter (Heartbeat, Notfall-Alarm, Naeherungs-LED,
            // Flugbuch, Beobachtungsalarm, Bodenfahrzeuge-Filter) stehen
            // bewusst gemeinsam am Ende der Liste, direkt ueber dem
            // Zurueck-Button - zuerst die LED-gesteuerten Alarme (Heartbeat,
            // Notfall, Naeherung; siehe LedAlert::Mode) zusammenhaengend,
            // danach die beiden nicht-LED-basierten Schalter Flugbuch und
            // Beobachtungsalarm sowie zuletzt der Bodenfahrzeuge-Filter.
            Rect aircraftListBtn = flightRowRect(0);
            Rect statsBtn      = flightRowRect(1);
            Rect statsHistoryBtn = flightRowRect(2);
            Rect logFilesBtn   = flightRowRect(3);
            Rect locationBtn   = flightRowRect(4);
            Rect airlineBtn    = flightRowRect(5);
            Rect watchlistBtn      = flightRowRect(6);
            Rect heartbeatBtn  = flightRowRect(7);
            Rect emergencyBtn  = flightRowRect(8);
            Rect proximityBtn  = flightRowRect(9);
            Rect logbookBtn    = flightRowRect(10);
            Rect watchlistAlertBtn = flightRowRect(11);
            Rect groundBtn     = flightRowRect(12);
            Rect backBtn       = flightRowRect(13);

            drawButton(tft, aircraftListBtn, I18n::t(StringId::MENU_AIRCRAFT_LIST));
            drawButton(tft, statsBtn, I18n::t(StringId::MENU_STATISTICS));
            drawButton(tft, statsHistoryBtn, I18n::t(StringId::MENU_STATS_HISTORY));
            drawButton(tft, logFilesBtn, I18n::t(StringId::MENU_LOGBOOK_FILES));
            drawButton(tft, locationBtn, I18n::t(StringId::MENU_LOCATION_PRESETS));
            drawButton(tft, airlineBtn, I18n::t(StringId::MENU_AIRLINE_FILTER));
            drawButton(tft, watchlistBtn, I18n::t(StringId::MENU_WATCHLIST));
            drawButton(tft, heartbeatBtn, I18n::t(StringId::MENU_LED_HEARTBEAT) + onOff(SettingsStore::ledHeartbeatEnabled()));
            drawButton(tft, emergencyBtn, I18n::t(StringId::MENU_EMERGENCY_ALERT) + onOff(SettingsStore::emergencyAlertEnabled()));
            drawButton(tft, proximityBtn, I18n::t(StringId::MENU_PROXIMITY_LED) + onOff(SettingsStore::proximityAlertEnabled()));
            drawButton(tft, logbookBtn, I18n::t(StringId::MENU_FLIGHT_LOGBOOK) + onOff(SettingsStore::flightLogbookEnabled()));
            drawButton(tft, watchlistAlertBtn, I18n::t(StringId::MENU_WATCHLIST_ALERT) + onOff(SettingsStore::watchlistAlertEnabled()));
            drawButton(tft, groundBtn, I18n::t(StringId::MENU_HIDE_GROUND) + onOff(SettingsStore::hideGroundVehicles()));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
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
            } else if (statsBtn.contains(tap.x, tap.y)) {
                StatsScreen::run(tft);
            } else if (statsHistoryBtn.contains(tap.x, tap.y)) {
                StatsHistoryScreen::run(tft);
            } else if (logFilesBtn.contains(tap.x, tap.y)) {
                LogbookFilesScreen::run(tft);
            } else if (locationBtn.contains(tap.x, tap.y)) {
                LocationPresetsScreen::run(tft);
            } else if (airlineBtn.contains(tap.x, tap.y)) {
                AirlineFilterScreen::run(tft);
            } else if (watchlistBtn.contains(tap.x, tap.y)) {
                AircraftWatchlistScreen::run(tft);
            } else if (heartbeatBtn.contains(tap.x, tap.y)) {
                SettingsStore::setLedHeartbeatEnabled(!SettingsStore::ledHeartbeatEnabled());
            } else if (emergencyBtn.contains(tap.x, tap.y)) {
                SettingsStore::setEmergencyAlertEnabled(!SettingsStore::emergencyAlertEnabled());
            } else if (proximityBtn.contains(tap.x, tap.y)) {
                SettingsStore::setProximityAlertEnabled(!SettingsStore::proximityAlertEnabled());
            } else if (logbookBtn.contains(tap.x, tap.y)) {
                if (SettingsStore::flightLogbookEnabled()) {
                    // Ausschalten ist immer unbedenklich - keine Bestaetigung noetig.
                    SettingsStore::setFlightLogbookEnabled(false);
                    SettingsStore::setFlightLogbookEnabledAtEpoch(0);
                    SettingsStore::setFlightLogbookSessionFile("");
                } else if (confirmWarningScreen(tft, StringId::MENU_LOGBOOK_WARNING_TITLE,
                                                 StringId::MENU_LOGBOOK_WARNING_BODY)) {
                    SettingsStore::setFlightLogbookEnabled(true);
                    SettingsStore::setFlightLogbookEnabledAtEpoch((uint32_t)time(nullptr));
                    // Leerer Eintrag erzwingt eine frische Sitzungsdatei beim
                    // naechsten FlightLogbook::update() statt eine evtl. noch
                    // vorhandene alte Datei weiterzuschreiben.
                    SettingsStore::setFlightLogbookSessionFile("");
                }
            } else if (watchlistAlertBtn.contains(tap.x, tap.y)) {
                SettingsStore::setWatchlistAlertEnabled(!SettingsStore::watchlistAlertEnabled());
            } else if (groundBtn.contains(tap.x, tap.y)) {
                SettingsStore::setHideGroundVehicles(!SettingsStore::hideGroundVehicles());
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Main;
            }
        }
    }
}

}
