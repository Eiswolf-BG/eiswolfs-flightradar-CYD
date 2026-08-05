#include "menu_screen.h"
#include "touch_input.h"
#include "calibration_screen.h"
#include "wifi_manage_screen.h"
#include "stats_screen.h"
#include "logbook_files_screen.h"
#include "location_presets_screen.h"
#include "airline_filter_screen.h"
#include "aircraft_watchlist_screen.h"
#include "language_screen.h"
#include "units_screen.h"
#include "settings_backup.h"
#include "settings_store.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"

namespace MenuScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr int16_t ROW_H = 22;
    constexpr int16_t ROW_GAP = 1;
    constexpr int16_t ROW_START_Y = 18;

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(ROW_START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
    }

    constexpr int16_t CAT_ROW_H = 50;
    constexpr int16_t CAT_ROW_GAP = 10;
    constexpr int16_t CAT_START_Y = 30;

    Rect catRowRect(uint8_t index) {
        return {10, (int16_t)(CAT_START_Y + index * (CAT_ROW_H + CAT_ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), CAT_ROW_H};
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

    void showBriefMessage(TFT_eSPI& tft, const String& msg, uint16_t color) {
        tft.fillRect(0, Config::SCREEN_HEIGHT - 18, Config::SCREEN_WIDTH, 18, TFT_BLACK);
        tft.setTextColor(color, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(msg, Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT - 9);
        tft.setTextDatum(TL_DATUM);
        delay(1200);
    }

    enum class Page { Main, Region, System, Flight };
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

            Rect calibBtn     = rowRect(0);
            Rect invertBtn    = rowRect(1);
            Rect timeoutBtn   = rowRect(2);
            Rect nightDimBtn  = rowRect(3);
            Rect backupBtn    = rowRect(4);
            Rect restoreBtn   = rowRect(5);
            Rect backBtn      = rowRect(6);

            drawButton(tft, calibBtn, I18n::t(StringId::MENU_CALIBRATE));

            String invertLabel = SettingsStore::displayInverted()
                                      ? I18n::t(StringId::MENU_DISPLAY_INVERTED)
                                      : I18n::t(StringId::MENU_DISPLAY_NORMAL);
            drawButton(tft, invertBtn, invertLabel);

            drawButton(tft, timeoutBtn, screenTimeoutLabel(SettingsStore::screenTimeoutMinutes()));
            drawButton(tft, nightDimBtn, I18n::t(StringId::MENU_NIGHT_DIMMING) + onOff(SettingsStore::nightDimmingEnabled()));
            drawButton(tft, backupBtn, I18n::t(StringId::MENU_BACKUP));
            drawButton(tft, restoreBtn, I18n::t(StringId::MENU_RESTORE));
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
            } else if (timeoutBtn.contains(tap.x, tap.y)) {
                uint8_t current = SettingsStore::screenTimeoutMinutes();
                uint8_t next = (current >= 10) ? 0 : (current + 1);
                SettingsStore::setScreenTimeoutMinutes(next);
            } else if (nightDimBtn.contains(tap.x, tap.y)) {
                SettingsStore::setNightDimmingEnabled(!SettingsStore::nightDimmingEnabled());
            } else if (backupBtn.contains(tap.x, tap.y)) {
                bool ok = SettingsBackup::backup();
                showBriefMessage(tft, I18n::t(ok ? StringId::MENU_BACKUP_SAVED : StringId::MENU_BACKUP_FAILED),
                                 ok ? TFT_GREEN : TFT_RED);
            } else if (restoreBtn.contains(tap.x, tap.y)) {
                if (SettingsBackup::hasBackup()) {
                    bool ok = SettingsBackup::restore();
                    showBriefMessage(tft, I18n::t(ok ? StringId::MENU_RESTORED : StringId::MENU_RESTORE_FAILED),
                                     ok ? TFT_GREEN : TFT_RED);
                }
            } else if (backBtn.contains(tap.x, tap.y)) {
                page = Page::Main;
            }

        } else { // Page::Flight
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::MENU_CATEGORY_FLIGHT));

            Rect statsBtn      = rowRect(0);
            Rect logFilesBtn   = rowRect(1);
            Rect logbookBtn    = rowRect(2);
            Rect heartbeatBtn  = rowRect(3);
            Rect proximityBtn  = rowRect(4);
            Rect emergencyBtn  = rowRect(5);
            Rect locationBtn   = rowRect(6);
            Rect airlineBtn    = rowRect(7);
            Rect watchlistBtn      = rowRect(8);
            Rect watchlistAlertBtn = rowRect(9);
            Rect groundBtn     = rowRect(10);
            Rect backBtn       = rowRect(11);

            drawButton(tft, statsBtn, I18n::t(StringId::MENU_STATISTICS));
            drawButton(tft, logFilesBtn, I18n::t(StringId::MENU_LOGBOOK_FILES));
            drawButton(tft, logbookBtn, I18n::t(StringId::MENU_FLIGHT_LOGBOOK) + onOff(SettingsStore::flightLogbookEnabled()));
            drawButton(tft, heartbeatBtn, I18n::t(StringId::MENU_LED_HEARTBEAT) + onOff(SettingsStore::ledHeartbeatEnabled()));
            drawButton(tft, proximityBtn, I18n::t(StringId::MENU_PROXIMITY_LED) + onOff(SettingsStore::proximityAlertEnabled()));
            drawButton(tft, emergencyBtn, I18n::t(StringId::MENU_EMERGENCY_ALERT) + onOff(SettingsStore::emergencyAlertEnabled()));
            drawButton(tft, locationBtn, I18n::t(StringId::MENU_LOCATION_PRESETS));
            drawButton(tft, airlineBtn, I18n::t(StringId::MENU_AIRLINE_FILTER));
            drawButton(tft, watchlistBtn, I18n::t(StringId::MENU_WATCHLIST));
            drawButton(tft, watchlistAlertBtn, I18n::t(StringId::MENU_WATCHLIST_ALERT) + onOff(SettingsStore::watchlistAlertEnabled()));
            drawButton(tft, groundBtn, I18n::t(StringId::MENU_HIDE_GROUND) + onOff(SettingsStore::hideGroundVehicles()));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_ARROW));

            TouchInput::Point tap;
            while (true) {
                if (TouchInput::wasTapped(tap)) break;
                MenuStars::update(tft);
                delay(20);
            }

            if (statsBtn.contains(tap.x, tap.y)) {
                StatsScreen::run(tft);
            } else if (logFilesBtn.contains(tap.x, tap.y)) {
                LogbookFilesScreen::run(tft);
            } else if (logbookBtn.contains(tap.x, tap.y)) {
                SettingsStore::setFlightLogbookEnabled(!SettingsStore::flightLogbookEnabled());
            } else if (heartbeatBtn.contains(tap.x, tap.y)) {
                SettingsStore::setLedHeartbeatEnabled(!SettingsStore::ledHeartbeatEnabled());
            } else if (proximityBtn.contains(tap.x, tap.y)) {
                SettingsStore::setProximityAlertEnabled(!SettingsStore::proximityAlertEnabled());
            } else if (emergencyBtn.contains(tap.x, tap.y)) {
                SettingsStore::setEmergencyAlertEnabled(!SettingsStore::emergencyAlertEnabled());
            } else if (locationBtn.contains(tap.x, tap.y)) {
                LocationPresetsScreen::run(tft);
            } else if (airlineBtn.contains(tap.x, tap.y)) {
                AirlineFilterScreen::run(tft);
            } else if (watchlistBtn.contains(tap.x, tap.y)) {
                AircraftWatchlistScreen::run(tft);
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