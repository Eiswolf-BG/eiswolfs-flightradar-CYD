#include "radar_theme_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "menu_screen.h"
#include "ui_theme.h"
#include "i18n.h"
#include "config.h"

// Einstell-Screen fuer die Radar-Darstellung (Menue > System > Radar-
// Darstellung). Oben ein einzelner "Farben"-Button, der den Unterscreen
// runColorsScreen() (siehe unten) mit den fuenf EXKLUSIVEN Farbschema-
// Optionen (Gruen/Amber/Blau/Rot/Lila, SettingsStore::radarThemeIndex())
// oeffnet - vorher standen hier drei einzelne Theme-Buttons direkt in
// dieser Zeilenliste, wurden aber mit den beiden neuen Farben (Rot/Lila)
// zu Fuenft und damit als eigener Unterscreen uebersichtlicher (Alex'
// Wunsch). Darunter weiterhin sechs UNABHAENGIGE, ankreuzbare Extras
// (CRT-Phosphor, Radar-Puls, Klassik-Radar, Militaer-/Behoerdenflug-
// Erkennung, Regen-Effekt, Overlay) - lassen sich mit JEDEM Farbschema
// kombinieren, deshalb eigene bool-Einstellungen statt weiterer Werte fuer
// radarThemeIndex(). Die Farbschema-Buttons im Unterscreen zeigen bewusst
// NICHT ihre jeweils eigene Farbe als Vorschau (alle nutzen denselben
// drawButton() mit der aktuell aktiven UiTheme::accentColor()) - nur der
// Textlabel ("Grün"/"Amber"/"Blau"/"Rot"/"Lila") unterscheidet sie, wie
// schon vor der projektweiten Ausweitung des Farbschemas.
//
// GRUNDSATZ (siehe CLAUDE.md): jedes Farbthema MUSS an die LED gebunden
// sein UND systemweit auf allen Screens wirken (inkl. Ruhebildschirm,
// Boot-Sequenz, Menues, Web-UI) - gilt fuer Rot/Lila genauso wie fuer die
// drei bestehenden Themen.
namespace RadarThemeScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool active = false) {
        uint16_t bg = active ? UiTheme::accentColor(tft) : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : UiTheme::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Echtes ankreuzbares Kaestchen links neben dem Label statt eines
    // Text-Buttons mit "An/Aus"-Suffix (Alex' ausdruecklicher Wunsch) - die
    // ganze Zeile bleibt trotzdem antippbar (nicht nur das Kaestchen
    // selbst), gleiche Rect.contains()-Flaeche wie bei den Theme-Buttons.
    void drawCheckboxRow(TFT_eSPI& tft, const Rect& r, const String& label, bool checked) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));

        constexpr int16_t BOX_SIZE = 22;
        int16_t boxX = r.x + 10;
        int16_t boxY = (int16_t)(r.y + (r.h - BOX_SIZE) / 2);
        if (checked) {
            tft.fillRoundRect(boxX, boxY, BOX_SIZE, BOX_SIZE, 3, UiTheme::accentColor(tft));
        } else {
            tft.drawRoundRect(boxX, boxY, BOX_SIZE, BOX_SIZE, 3, UiTheme::accentColor(tft));
        }

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.drawString(label, (int16_t)(boxX + BOX_SIZE + 10), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    // Kleiner "?"-Info-Button rechts in einer Kaestchen-Zeile (CRT-Phosphor/
    // Radar-Puls/Klassik-Radar) - bewusst deutlich kleiner als die
    // Standard-"?"-Buttons anderer Screens (dort 30x24, hier 20x20), damit
    // er MIT Abstand innerhalb der jeweiligen Zeile Platz findet, statt mit
    // deren eigenem Rahmen (drawCheckboxRow() oben) zu kollidieren. ROW_PAD
    // haelt den gleichen Abstand zum rechten Zeilenrand wie zur Zeile
    // selbst - siehe rowInfoBtnRect().
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

    // Zeilenhoehe aus dem verfuegbaren Platz errechnet (gleiches Muster wie
    // SYSTEM_ROW_H in menu_screen.cpp) statt fest verdrahtet - 8 Zeilen
    // ("Farben"-Button + 6 Kaestchen + Zurueck) muessen mit ca. 10px Reserve
    // zum unteren Rand aufs Display passen.
    constexpr uint8_t ROW_COUNT = 8;
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t START_Y = 40;
    constexpr int16_t END_Y = Config::SCREEN_HEIGHT - 10;
    constexpr int16_t ROW_H =
        (END_Y - START_Y - (ROW_COUNT - 1) * ROW_GAP) / ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
    }

    // Eigenes, unabhaengiges Zeilenraster fuer den Farben-Unterscreen (5
    // Themen + Zurueck = 6 Zeilen, mehr Platz pro Zeile als im
    // Hauptscreen) - dupliziert statt geteilt (CLAUDE.md-Konvention),
    // START_Y/END_Y/ROW_GAP oben werden aber mitgenutzt.
    constexpr uint8_t COLOR_ROW_COUNT = 6;
    constexpr int16_t COLOR_ROW_H =
        (END_Y - START_Y - (COLOR_ROW_COUNT - 1) * ROW_GAP) / COLOR_ROW_COUNT;

    Rect colorRowRect(uint8_t index) {
        return {10, (int16_t)(START_Y + index * (COLOR_ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), COLOR_ROW_H};
    }

    // Unterscreen mit den fuenf exklusiven Farbschema-Optionen (Gruen/
    // Amber/Blau/Rot/Lila) - gleiches 5-Wege-Auswahl-Muster wie vorher die
    // drei Buttons direkt im Hauptscreen, nur jetzt auf einem eigenen
    // Screen wegen der zwei neuen Optionen.
    void runColorsScreen(TFT_eSPI& tft) {
        constexpr uint8_t THEME_COUNT = 5;
        StringId themeLabels[THEME_COUNT] = {
            StringId::RADAR_THEME_GREEN, StringId::RADAR_THEME_AMBER, StringId::RADAR_THEME_BLUE,
            StringId::RADAR_THEME_RED, StringId::RADAR_THEME_PURPLE
        };

        bool done = false;
        MenuStars::reset();
        while (!done) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::RADAR_COLORS_TITLE));

            uint8_t currentTheme = SettingsStore::radarThemeIndex();
            Rect themeRects[THEME_COUNT];
            for (uint8_t i = 0; i < THEME_COUNT; i++) {
                themeRects[i] = colorRowRect(i);
                drawButton(tft, themeRects[i], I18n::t(themeLabels[i]), i == currentTheme);
            }
            Rect backBtn = colorRowRect(THEME_COUNT);
            drawButton(tft, backBtn, I18n::t(StringId::BACK));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
                if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
                MenuStars::update(tft);
                delay(20);
            }

            bool handled = false;
            for (uint8_t i = 0; i < THEME_COUNT && !handled; i++) {
                if (themeRects[i].contains(tap.x, tap.y)) {
                    SettingsStore::setRadarThemeIndex(i);
                    handled = true;
                }
            }
            if (!handled && backBtn.contains(tap.x, tap.y)) {
                done = true;
            }
        }
    }
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::RADAR_THEME_TITLE));

        Rect colorsRow = rowRect(0);
        drawButton(tft, colorsRow, I18n::t(StringId::RADAR_COLORS_BUTTON));

        Rect crtRow = rowRect(1);
        drawCheckboxRow(tft, crtRow, I18n::t(StringId::RADAR_THEME_CRT), SettingsStore::crtPhosphorEnabled());
        drawRowInfoButton(tft, crtRow);

        Rect pulseRow = rowRect(2);
        drawCheckboxRow(tft, pulseRow, I18n::t(StringId::RADAR_PULSE_TOGGLE), SettingsStore::radarPulseEnabled());
        drawRowInfoButton(tft, pulseRow);

        Rect classicRow = rowRect(3);
        drawCheckboxRow(tft, classicRow, I18n::t(StringId::MENU_CLASSIC_RADAR), SettingsStore::classicRadarEnabled());
        drawRowInfoButton(tft, classicRow);

        Rect militaryRow = rowRect(4);
        drawCheckboxRow(tft, militaryRow, I18n::t(StringId::MENU_MILITARY_SQUAWK), SettingsStore::militarySquawkDetectionEnabled());
        drawRowInfoButton(tft, militaryRow);

        Rect rainRow = rowRect(5);
        drawCheckboxRow(tft, rainRow, I18n::t(StringId::MENU_RAIN_EFFECT), SettingsStore::rainEffectEnabled());
        drawRowInfoButton(tft, rainRow);

        Rect eventCornerRow = rowRect(6);
        drawCheckboxRow(tft, eventCornerRow, I18n::t(StringId::MENU_EVENT_CORNER_OVERLAY), SettingsStore::eventCornerOverlayEnabled());
        drawRowInfoButton(tft, eventCornerRow);

        Rect backBtn = rowRect(7);
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }

        bool handled = false;
        if (colorsRow.contains(tap.x, tap.y)) {
            runColorsScreen(tft);
            MenuStars::reset();
            handled = true;
        }
        // "?"-Info-Buttons zuerst pruefen (kleine Flaeche innerhalb der
        // jeweiligen Zeile) - sonst wuerde ein Tap darauf faelschlich als
        // Tap auf die ganze Zeile (Schalter umlegen) gewertet.
        if (!handled && rowInfoBtnRect(crtRow).contains(tap.x, tap.y)) {
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::RADAR_THEME_CRT_INFO_TITLE),
                                        I18n::t(StringId::RADAR_THEME_CRT_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
            handled = true;
        }
        if (!handled && rowInfoBtnRect(pulseRow).contains(tap.x, tap.y)) {
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::RADAR_PULSE_INFO_TITLE),
                                        I18n::t(StringId::RADAR_PULSE_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
            handled = true;
        }
        if (!handled && rowInfoBtnRect(classicRow).contains(tap.x, tap.y)) {
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::RADAR_CLASSIC_INFO_TITLE),
                                        I18n::t(StringId::RADAR_CLASSIC_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
            handled = true;
        }
        if (!handled && rowInfoBtnRect(militaryRow).contains(tap.x, tap.y)) {
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::MILITARY_SQUAWK_INFO_TITLE),
                                        I18n::t(StringId::MILITARY_SQUAWK_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
            handled = true;
        }
        if (!handled && rowInfoBtnRect(rainRow).contains(tap.x, tap.y)) {
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::RAIN_EFFECT_INFO_TITLE),
                                        I18n::t(StringId::RAIN_EFFECT_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
            handled = true;
        }
        if (!handled && rowInfoBtnRect(eventCornerRow).contains(tap.x, tap.y)) {
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::EVENT_CORNER_OVERLAY_INFO_TITLE),
                                        I18n::t(StringId::EVENT_CORNER_OVERLAY_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
            handled = true;
        }
        if (!handled && crtRow.contains(tap.x, tap.y)) {
            SettingsStore::setCrtPhosphorEnabled(!SettingsStore::crtPhosphorEnabled());
            handled = true;
        }
        if (!handled && pulseRow.contains(tap.x, tap.y)) {
            SettingsStore::setRadarPulseEnabled(!SettingsStore::radarPulseEnabled());
            handled = true;
        }
        if (!handled && classicRow.contains(tap.x, tap.y)) {
            SettingsStore::setClassicRadarEnabled(!SettingsStore::classicRadarEnabled());
            handled = true;
        }
        if (!handled && militaryRow.contains(tap.x, tap.y)) {
            SettingsStore::setMilitarySquawkDetectionEnabled(!SettingsStore::militarySquawkDetectionEnabled());
            handled = true;
        }
        if (!handled && rainRow.contains(tap.x, tap.y)) {
            SettingsStore::setRainEffectEnabled(!SettingsStore::rainEffectEnabled());
            handled = true;
        }
        if (!handled && eventCornerRow.contains(tap.x, tap.y)) {
            SettingsStore::setEventCornerOverlayEnabled(!SettingsStore::eventCornerOverlayEnabled());
            handled = true;
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
