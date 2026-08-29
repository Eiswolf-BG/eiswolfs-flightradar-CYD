#include "brightness_screen.h"
#include "settings_store.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "menu_screen.h"
#include "auto_brightness.h"
#include "config.h"
#include "i18n.h"
#include "ui_theme.h"

namespace BrightnessScreen {

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

    // Ankreuzbare Zeile fuer "Auto-Helligkeit" - gleiches Muster wie
    // drawCheckboxRow() in radar_theme_screen.cpp (dort bewusst dupliziert
    // statt geteilt, siehe CLAUDE.md-Konvention "jeder Screen unabhaengig
    // lauffaehig"), hier fuer diesen Screen erneut kopiert.
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

    // Kleiner "?"-Info-Button rechts in der Auto-Helligkeit-Zeile - gleiches
    // Prinzip/gleiche Groesse wie in radar_theme_screen.cpp/menu_screen.cpp
    // (siehe CLAUDE.md-Konvention "jeder neue Ein/Aus-Schalter bekommt einen
    // '?'-Info-Button in derselben Zeile").
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

    // Gleicher PWM-Kanal wie in main.cpp (dort in setup() mit ledcSetup()
    // initialisiert) - hier bewusst als eigene Konstante dupliziert statt
    // geteilt, siehe CLAUDE.md-Konvention ("jeder Screen unabhaengig").
    constexpr uint8_t BACKLIGHT_PWM_CHANNEL = 0;

    void applyLive(uint8_t percent) {
        uint8_t pwm = (uint8_t)((uint16_t)percent * 255 / 100);
        ledcWrite(BACKLIGHT_PWM_CHANNEL, pwm);
    }
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();

    while (!done) {
        bool autoOn = SettingsStore::autoBrightnessEnabled();
        // Frischer Messwert bei JEDEM Neuzeichnen (also nach jedem Tap) -
        // dieser Screen aktualisiert sich sonst nicht laufend im Hintergrund
        // (kein eigener Redraw-Timer, gleiches Prinzip wie der Rest des
        // Screens), das reicht aber, um beim Betreten/nach einem Tap einen
        // aktuellen Wert zu zeigen statt eines veralteten.
        if (autoOn) AutoBrightness::update();
        uint8_t percent = autoOn ? AutoBrightness::currentPercent() : SettingsStore::brightnessPercent();

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::BRIGHTNESS_TITLE));

        Rect autoRow = {10, 34, (int16_t)(Config::SCREEN_WIDTH - 20), 30};
        drawCheckboxRow(tft, autoRow, I18n::t(StringId::MENU_AUTO_BRIGHTNESS), autoOn);
        drawRowInfoButton(tft, autoRow);

        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(3);
        tft.drawString(buf, Config::SCREEN_WIDTH / 2, 130);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);

        // Manuelle -/+ Regler ergeben bei aktiver Auto-Helligkeit keinen
        // Sinn (der Sensor bestimmt den Wert) - deshalb weder gezeichnet
        // noch im Tap-Handling unten beruecksichtigt, statt sie sichtbar,
        // aber wirkungslos zu lassen.
        Rect minusBtn = {10, 190, 100, 60};
        Rect plusBtn  = {(int16_t)(Config::SCREEN_WIDTH - 110), 190, 100, 60};
        if (!autoOn) {
            drawButton(tft, minusBtn, "-");
            drawButton(tft, plusBtn, "+");
        }

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50),
                         (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
            if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }

        if (rowInfoBtnRect(autoRow).contains(tap.x, tap.y)) {
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::AUTO_BRIGHTNESS_INFO_TITLE),
                                        I18n::t(StringId::AUTO_BRIGHTNESS_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
        } else if (autoRow.contains(tap.x, tap.y)) {
            bool newAutoOn = !autoOn;
            SettingsStore::setAutoBrightnessEnabled(newAutoOn);
            // Sofort anwenden statt bis zum naechsten 1-Sekunden-Takt in
            // main.cpp::loop() zu warten - gleiches Live-Vorschau-Prinzip
            // wie bei den manuellen -/+ Tasten unten (applyLive()).
            if (newAutoOn) {
                AutoBrightness::update();
                applyLive(AutoBrightness::currentPercent());
            } else {
                applyLive(SettingsStore::brightnessPercent());
            }
        } else if (!autoOn && minusBtn.contains(tap.x, tap.y)) {
            uint8_t next = (percent > Config::BRIGHTNESS_MIN_PERCENT + Config::BRIGHTNESS_STEP_PERCENT - 1)
                ? (uint8_t)(percent - Config::BRIGHTNESS_STEP_PERCENT)
                : Config::BRIGHTNESS_MIN_PERCENT;
            SettingsStore::setBrightnessPercent(next);
            applyLive(next);
        } else if (!autoOn && plusBtn.contains(tap.x, tap.y)) {
            uint8_t next = (percent + Config::BRIGHTNESS_STEP_PERCENT <= Config::BRIGHTNESS_MAX_PERCENT)
                ? (uint8_t)(percent + Config::BRIGHTNESS_STEP_PERCENT)
                : Config::BRIGHTNESS_MAX_PERCENT;
            SettingsStore::setBrightnessPercent(next);
            applyLive(next);
        } else if (backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}
