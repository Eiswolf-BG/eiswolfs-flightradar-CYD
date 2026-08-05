#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "aircraft.h"
#include "radar_math.h"
#include "aircraft_table.h"
#include "airline_lookup.h"
#include "airline_filter.h"
#include "aircraft_watchlist.h"
#include "sd_storage.h"
#include "wifi_manager.h"
#include "location_manager.h"
#include "location_presets.h"
#include "adsb_client.h"
#include "touch_input.h"
#include "calibration_screen.h"
#include "wifi_setup_screen.h"
#include "menu_screen.h"
#include "settings_store.h"
#include "net_task.h"
#include "radar_screen.h"
#include "splash_screen.h"
#include "led_alert.h"
#include "flight_logbook.h"
#include "screenshot.h"
#include "i18n.h"
#include "first_run_language_screen.h"
#include "menu_stars.h"
#include "ui_font.h"

TFT_eSPI tft = TFT_eSPI();

constexpr int16_t HEADER_TITLE_H = 30;
constexpr int16_t STATUS_LINE_H = 12;
constexpr int16_t CONTENT_TOP = HEADER_TITLE_H + STATUS_LINE_H;
constexpr uint32_t POLL_INTERVAL_MS = 300;
constexpr uint32_t SWEEP_TICK_MS = 80;
constexpr uint32_t STATUS_LINE_UPDATE_MS = 1000;
uint32_t lastPollMs = 0;
uint32_t lastSweepMs = 0;
uint32_t lastStatusLineMs = 0;
uint32_t lastRenderedVersion = 0xFFFFFFFF;
bool forceRedraw = false;
bool wasEmergency = false;
bool bannerBlinkOn = false;

constexpr uint8_t BACKLIGHT_FULL = 255;
constexpr uint8_t BACKLIGHT_NIGHT_DIM = 90;
constexpr uint8_t BACKLIGHT_DIMMED = 12;
constexpr uint8_t BACKLIGHT_PWM_CHANNEL = 0;
uint32_t lastInteractionMs = 0;
bool screenDimmed = false;
bool nightDimActive = false;

struct Rect {
    int16_t x, y, w, h;
    bool contains(int16_t px, int16_t py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

Rect menuBtn = {Config::SCREEN_WIDTH - 90, 3, 54, 22};
Rect camBtn = {(int16_t)(menuBtn.x - 46), 3, 42, 22};

void drawMenuButton() {
    tft.fillRoundRect(menuBtn.x, menuBtn.y, menuBtn.w, menuBtn.h, 4, TFT_BLACK);
    tft.drawRoundRect(menuBtn.x, menuBtn.y, menuBtn.w, menuBtn.h, 4, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Menu", menuBtn.x + menuBtn.w / 2, menuBtn.y + menuBtn.h / 2);
    tft.setTextDatum(TL_DATUM);
}

void drawCamButton() {
    tft.fillRoundRect(camBtn.x, camBtn.y, camBtn.w, camBtn.h, 4, TFT_BLACK);
    tft.drawRoundRect(camBtn.x, camBtn.y, camBtn.w, camBtn.h, 4, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Cam", camBtn.x + camBtn.w / 2, camBtn.y + camBtn.h / 2);
    tft.setTextDatum(TL_DATUM);
}

void drawWifiIcon(int16_t rightX, int16_t rowTop, int16_t rowH, int8_t rssi) {
    uint8_t level;
    if (rssi >= -55) level = 4;
    else if (rssi >= -65) level = 3;
    else if (rssi >= -75) level = 2;
    else if (rssi >= -85) level = 1;
    else level = 0;

    constexpr uint8_t BAR_W = 3;
    constexpr uint8_t BAR_GAP = 2;
    constexpr uint8_t BAR_COUNT = 4;
    int16_t baseline = rowTop + rowH - 1;
    int16_t x = rightX - BAR_COUNT * (BAR_W + BAR_GAP);

    for (uint8_t i = 0; i < BAR_COUNT; i++) {
        int16_t barH = 3 + i * 2;
        uint16_t color = (i < level) ? TFT_GREEN : TFT_DARKGREY;
        tft.fillRect(x, baseline - barH, BAR_W, barH, color);
        x += BAR_W + BAR_GAP;
    }
}

constexpr int16_t WIFI_ICON_W = 22;
constexpr int16_t WIFI_ICON_H = 14;

void updateWifiIcon() {
    int16_t iconX = Config::SCREEN_WIDTH - WIFI_ICON_W - 2;
    int16_t iconY = 2;
    tft.fillRect(iconX, iconY, WIFI_ICON_W, WIFI_ICON_H, TFT_BLACK);
    if (WiFi.status() == WL_CONNECTED) {
        drawWifiIcon(Config::SCREEN_WIDTH - 4, iconY, WIFI_ICON_H, WiFi.RSSI());
    }
}

void drawHeader() {
    tft.fillRect(0, 0, Config::SCREEN_WIDTH, CONTENT_TOP, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(6, 10);
    tft.println("Eiswolfs FR");
    drawCamButton();
    drawMenuButton();
    updateWifiIcon();
}

void updateStatusLine() {
    if (wasEmergency) return;

    tft.fillRect(0, HEADER_TITLE_H, Config::SCREEN_WIDTH, STATUS_LINE_H, TFT_BLACK);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);

    time_t now = time(nullptr);
    if (now > 8 * 3600 * 2) {
        struct tm tmNow;
        localtime_r(&now, &tmNow);
        char timeBuf[6];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tmNow.tm_hour, tmNow.tm_min);
        tft.setCursor(6, HEADER_TITLE_H + 2);
        tft.print(timeBuf);
    }

    updateWifiIcon();
}

// Prueft, ob die aktuelle Lokalzeit im Nachtdimm-Fenster (22:00-06:00) liegt.
// Ueber Mitternacht hinweg gerechnet. Solange die Uhrzeit noch nicht per NTP
// synchronisiert ist, wird false zurueckgegeben statt zu raten (gleiche
// Pruefung wie in updateStatusLine()).
bool isNightDimHours() {
    time_t now = time(nullptr);
    if (now <= 8 * 3600 * 2) return false;

    struct tm tmNow;
    localtime_r(&now, &tmNow);
    int hour = tmNow.tm_hour;
    return (hour >= 22 || hour < 6);
}

// Sanfte Nachtdimmung des Backlights zwischen 22:00 und 06:00 Uhr, sofern in
// den Einstellungen aktiviert. Der Inaktivitaets-Timeout (screenDimmed) hat
// Vorrang und wird hier nicht ueberschrieben.
void updateNightDimming() {
    if (screenDimmed) return;

    bool shouldDim = SettingsStore::nightDimmingEnabled() && isNightDimHours();
    if (shouldDim != nightDimActive) {
        ledcWrite(BACKLIGHT_PWM_CHANNEL, shouldDim ? BACKLIGHT_NIGHT_DIM : BACKLIGHT_FULL);
        nightDimActive = shouldDim;
    }
}

void takeScreenshotWithFeedback() {
    LedAlert::flashWhite();

    String filename = Screenshot::save(tft);

    tft.fillRect(0, 0, Config::SCREEN_WIDTH, CONTENT_TOP, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setCursor(6, 10);
    if (filename.length() > 0) {
        tft.print(String(I18n::t(StringId::SCREENSHOT_SAVED_PREFIX)) + filename);
    } else {
        tft.setTextColor(TFT_RED, TFT_NAVY);
        tft.print(I18n::t(StringId::SCREENSHOT_FAILED));
    }
    delay(1200);
    drawHeader();
    updateStatusLine();
}

void haltWithSdRequiredScreen() {
    tft.invertDisplay(true);

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    int16_t cx = Config::SCREEN_WIDTH / 2;
    int16_t cy = Config::SCREEN_HEIGHT / 2;

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString(I18n::t(StringId::SD_REQUIRED_LINE1), cx, cy - 40);
    tft.drawString(I18n::t(StringId::SD_REQUIRED_LINE2), cx, cy - 10);
    tft.drawString(I18n::t(StringId::SD_REQUIRED_LINE3), cx, cy + 20);

    tft.setTextSize(1);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(I18n::t(StringId::SD_REQUIRED_HINT), cx, cy + 60);
    tft.setTextDatum(TL_DATUM);

    while (true) {
        delay(1000);
    }
}

void updateEmergencyBanner(uint32_t nowMs) {
    RadarScreen::EmergencyInfo emergency = RadarScreen::checkEmergency();

    if (emergency.active) {
        bannerBlinkOn = !bannerBlinkOn;
        uint16_t bg = bannerBlinkOn ? TFT_RED : TFT_BLACK;
        tft.fillRect(0, 0, Config::SCREEN_WIDTH, CONTENT_TOP, bg);
        tft.setTextColor(TFT_WHITE, bg);
        tft.setTextSize(1);
        tft.setCursor(4, 10);
        tft.printf("EMERGENCY %s %s", emergency.squawk, emergency.callsign);
        wasEmergency = true;
    } else if (wasEmergency) {
        wasEmergency = false;
        drawHeader();
        updateStatusLine();
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);

    tft.init();
    tft.setRotation(0);
    tft.setFreeFont(&UiFont11pt);
    tft.invertDisplay(true);
    tft.fillScreen(TFT_BLACK);

    ledcSetup(BACKLIGHT_PWM_CHANNEL, 5000, 8);
    ledcAttachPin(TFT_BL, BACKLIGHT_PWM_CHANNEL);
    ledcWrite(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_FULL);

    TouchInput::begin();
    LedAlert::begin();

    bool sdOk = SdStorage::init();
    if (!sdOk) {
        haltWithSdRequiredScreen();
        return;
    }
    SdStorage::seedDefaultDataFiles();

    bool isFirstRun = !SD.exists(Config::SD_SETTINGS_FILE);

    SettingsStore::load();
    tft.invertDisplay(SettingsStore::displayInverted());

    WifiMgr::init();
    LocationPresets::init();
    AirlineFilter::init();
    AircraftWatchlist::init();

    SplashScreen::begin(tft);
    SplashScreen::setStatusLine(tft, 0, I18n::t(StringId::SPLASH_SD_OK), TFT_WHITE);

    if (!TouchInput::loadCalibration()) {
        CalibrationScreen::run(tft);
    }

    if (WifiMgr::networkCount() == 0) {
        WifiSetupScreen::run(tft);
    } else {
        SplashScreen::setStatusLine(tft, 1, I18n::t(StringId::SPLASH_CONNECTING_WIFI));
        WifiMgr::beginConnect();

        uint32_t waitStart = millis();
        while (WifiMgr::getState() == WifiMgr::State::Connecting && millis() - waitStart < 16000) {
            WifiMgr::update();
            MenuStars::update(tft);
            delay(100);
        }

        if (WifiMgr::getState() == WifiMgr::State::Connected) {
            SplashScreen::setStatusLine(tft, 1, String(I18n::t(StringId::SPLASH_WIFI_OK_PREFIX)) + WifiMgr::getIP());
        } else {
            SplashScreen::setStatusLine(tft, 1, I18n::t(StringId::SPLASH_WIFI_FAILED), TFT_RED);
        }
    }

    if (isFirstRun) {
        FirstRunLanguageScreen::run(tft);
        SplashScreen::begin(tft);
        SplashScreen::setStatusLine(tft, 0, I18n::t(StringId::SPLASH_SD_OK), TFT_WHITE);
    }

    AdsbClient::primeTime();

    SplashScreen::setStatusLine(tft, 2, I18n::t(StringId::SPLASH_GETTING_LOCATION));
    LocationManager::init();
    uint32_t locStart = millis();
    while (LocationManager::currentSource() == LocationManager::Source::None &&
           millis() - locStart < 8000) {
        LocationManager::requestIpLookupIfNeeded();
        MenuStars::update(tft);
        delay(200);
    }
    SplashScreen::setStatusLine(tft, 2, I18n::t(StringId::SPLASH_READY));

    if (LocationManager::hasUtcOffset()) {
        configTime(LocationManager::utcOffsetSeconds(), 0, "pool.ntp.org", "time.nist.gov");
    }

    AircraftTable::init();
    AirlineLookup::init();
    FlightLogbook::init();

    NetTask::begin();

    SplashScreen::waitRemaining(tft);

    drawHeader();
    updateStatusLine();
    RadarScreen::render(tft, CONTENT_TOP);

    lastInteractionMs = millis();
}

void loop() {
    TouchInput::Point tap;
    bool tapped = TouchInput::wasTapped(tap);

    if (tapped) {
        lastInteractionMs = millis();

        if (screenDimmed) {
            bool shouldNightDim = SettingsStore::nightDimmingEnabled() && isNightDimHours();
            ledcWrite(BACKLIGHT_PWM_CHANNEL, shouldNightDim ? BACKLIGHT_NIGHT_DIM : BACKLIGHT_FULL);
            nightDimActive = shouldNightDim;
            screenDimmed = false;
            tapped = false;
        }
    }

    if (tapped) {
        if (menuBtn.contains(tap.x, tap.y)) {
            MenuScreen::run(tft);
            drawHeader();
            updateStatusLine();
            forceRedraw = true;
        } else if (camBtn.contains(tap.x, tap.y)) {
            takeScreenshotWithFeedback();
        } else if (tap.y >= CONTENT_TOP) {
            if (RadarScreen::handleTap(tap.x, tap.y, CONTENT_TOP)) {
                forceRedraw = true;
            }
        }
    }

    if (forceRedraw || millis() - lastPollMs >= POLL_INTERVAL_MS) {
        lastPollMs = millis();
        uint32_t currentVersion = AircraftTable::version();
        if (forceRedraw || currentVersion != lastRenderedVersion) {
            lastRenderedVersion = currentVersion;
            forceRedraw = false;
            RadarScreen::render(tft, CONTENT_TOP);
            lastSweepMs = millis();
        }
    }

    uint32_t nowMs = millis();

    RadarScreen::updateProximityAlert(nowMs);

    if (nowMs - lastSweepMs >= SWEEP_TICK_MS) {
        uint32_t deltaMs = nowMs - lastSweepMs;
        lastSweepMs = nowMs;
        RadarScreen::tick(tft, CONTENT_TOP, deltaMs);
        updateEmergencyBanner(nowMs);
    }

    if (nowMs - lastStatusLineMs >= STATUS_LINE_UPDATE_MS) {
        lastStatusLineMs = nowMs;
        updateStatusLine();
        updateNightDimming();
    }

    uint8_t timeoutMin = SettingsStore::screenTimeoutMinutes();
    if (!screenDimmed && timeoutMin > 0) {
        uint32_t timeoutMs = (uint32_t)timeoutMin * 60000UL;
        if (nowMs - lastInteractionMs >= timeoutMs) {
            ledcWrite(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_DIMMED);
            screenDimmed = true;
        }
    }
}