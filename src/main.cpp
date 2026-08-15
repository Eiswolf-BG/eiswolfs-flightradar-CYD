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
#include "sun_times.h"
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
#include "weather.h"
#include "i18n.h"
#include "first_run_welcome_screen.h"
#include "first_run_language_screen.h"
#include "first_run_location_screen.h"
#include "first_run_complete_screen.h"
#include "menu_stars.h"
#include "ui_font.h"
#include <math.h>

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
Weather::Condition lastRenderedWeather = Weather::Condition::Unknown;
bool forceRedraw = false;
bool wasEmergency = false;
bool bannerBlinkOn = false;

constexpr uint8_t BACKLIGHT_FULL = 255;
constexpr uint8_t BACKLIGHT_PWM_CHANNEL = 0;
uint32_t lastInteractionMs = 0;
bool screenDimmed = false;
bool nightDimActive = false;

// Ruhebildschirm (Menue > System > Ruhebildschirm, siehe SettingsStore::
// screensaverEnabled()) - true, waehrend statt des komplett dunklen
// Backlights ein gedimmter Sternenhimmel mit Uhrzeit angezeigt wird.
// Getrennt von screenDimmed, da NICHT jeder Timeout automatisch ein
// Ruhebildschirm ist (Default bleibt: Backlight komplett aus) - siehe
// loop() weiter unten.
bool screensaverShowing = false;
uint32_t lastScreensaverClockMs = 0;

// Wandelt die Nutzer-Helligkeit (Menue > System > Helligkeit, 10-100%) in
// einen PWM-Wert 0-255 um - ersetzt das bisher fest verdrahtete
// BACKLIGHT_FULL als "normale" Helligkeit ueberall unten.
uint8_t normalBacklightPwm() {
    return (uint8_t)((uint16_t)SettingsStore::brightnessPercent() * 255 / 100);
}

// Nachtmodus-Helligkeit RELATIV zur normalen Helligkeit (siehe
// Config::NIGHT_DIM_REDUCTION_PERCENT) statt eines festen Absolutwerts -
// so bleibt der Dimm-Effekt bei JEDER eingestellten Normalhelligkeit
// spuerbar, auch bei schon niedrig eingestellter Helligkeit.
uint8_t nightDimBacklightPwm() {
    uint32_t normal = normalBacklightPwm();
    return (uint8_t)(normal * (100 - Config::NIGHT_DIM_REDUCTION_PERCENT) / 100);
}

struct Rect {
    int16_t x, y, w, h;
    bool contains(int16_t px, int16_t py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

Rect menuBtn = {Config::SCREEN_WIDTH - 90, 3, 54, 22};

// Platz, an dem frueher der Cam-Button war (siehe entfernte Screenshot-
// Funktion) - zeigt jetzt stattdessen ein kleines Wetter-Icon fuer den
// aktuell aktiven Standort (siehe weather.cpp/Weather::update()).
Rect weatherIconRect = {(int16_t)(menuBtn.x - 46), 3, 42, 22};

void drawMenuButton() {
    tft.fillRoundRect(menuBtn.x, menuBtn.y, menuBtn.w, menuBtn.h, 4, TFT_BLACK);
    tft.drawRoundRect(menuBtn.x, menuBtn.y, menuBtn.w, menuBtn.h, 4, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Menu", menuBtn.x + menuBtn.w / 2, menuBtn.y + menuBtn.h / 2);
    tft.setTextDatum(TL_DATUM);
}

// Kleine, mit einfachen TFT_eSPI-Grundformen gezeichnete Wolke (kein
// Bild/Font noetig) - Basis fuer die Regen/Schnee/Gewitter-Varianten.
void drawCloudShape(int16_t cx, int16_t cy, uint16_t color) {
    tft.fillCircle(cx - 5, cy + 1, 3, color);
    tft.fillCircle(cx - 1, cy - 2, 4, color);
    tft.fillCircle(cx + 4, cy, 4, color);
    tft.fillRect(cx - 8, cy, 13, 3, color);
}

// Sonne mit ein paar Strahlen ringsum, ebenfalls nur aus Grundformen.
void drawSunShape(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    tft.fillCircle(cx, cy, r, color);
    for (uint8_t i = 0; i < 8; i++) {
        float angle = i * (PI / 4.0f);
        int16_t x1 = cx + (int16_t)((r + 2) * cosf(angle));
        int16_t y1 = cy + (int16_t)((r + 2) * sinf(angle));
        int16_t x2 = cx + (int16_t)((r + 4) * cosf(angle));
        int16_t y2 = cy + (int16_t)((r + 4) * sinf(angle));
        tft.drawLine(x1, y1, x2, y2, color);
    }
}

// Zeichnet das Wetter-Icon passend zur zuletzt abgefragten Wetterlage
// (Weather::current()) - bei Weather::Condition::Unknown (noch keine
// erfolgreiche Abfrage, z.B. kurz nach dem Booten) bleibt die Flaeche
// einfach leer, statt einen Platzhalter anzuzeigen.
void drawWeatherIcon() {
    int16_t cx = weatherIconRect.x + weatherIconRect.w / 2;
    int16_t cy = weatherIconRect.y + weatherIconRect.h / 2;

    tft.fillRect(weatherIconRect.x, weatherIconRect.y, weatherIconRect.w, weatherIconRect.h, TFT_BLACK);

    switch (Weather::current()) {
        case Weather::Condition::Clear:
            drawSunShape(cx, cy, 6, TFT_YELLOW);
            break;
        case Weather::Condition::PartlyCloudy:
            drawSunShape((int16_t)(cx - 4), (int16_t)(cy - 3), 4, TFT_YELLOW);
            drawCloudShape((int16_t)(cx + 3), (int16_t)(cy + 2), TFT_LIGHTGREY);
            break;
        case Weather::Condition::Cloudy:
            drawCloudShape(cx, cy, TFT_LIGHTGREY);
            break;
        case Weather::Condition::Rain:
            drawCloudShape(cx, (int16_t)(cy - 3), TFT_LIGHTGREY);
            tft.drawLine(cx - 4, cy + 4, cx - 6, cy + 8, TFT_SKYBLUE);
            tft.drawLine(cx,     cy + 4, cx - 2, cy + 8, TFT_SKYBLUE);
            tft.drawLine(cx + 4, cy + 4, cx + 2, cy + 8, TFT_SKYBLUE);
            break;
        case Weather::Condition::Snow:
            drawCloudShape(cx, (int16_t)(cy - 3), TFT_LIGHTGREY);
            tft.drawPixel(cx - 4, cy + 6, TFT_WHITE);
            tft.drawPixel(cx,     cy + 7, TFT_WHITE);
            tft.drawPixel(cx + 4, cy + 6, TFT_WHITE);
            break;
        case Weather::Condition::Thunderstorm:
            drawCloudShape(cx, (int16_t)(cy - 3), TFT_LIGHTGREY);
            tft.drawLine(cx,     cy + 3, cx - 3, cy + 7, TFT_YELLOW);
            tft.drawLine(cx - 3, cy + 7, cx + 1, cy + 7, TFT_YELLOW);
            tft.drawLine(cx + 1, cy + 7, cx - 2, cy + 11, TFT_YELLOW);
            break;
        case Weather::Condition::Unknown:
        default:
            break;
    }
}

// Einfacher Zeilenumbruch fuer Fliesstext, analog zu layoutWrapped() in
// menu_screen.cpp/webui_screen.cpp (siehe "keine geteilten Module"-Konvention
// - jede Screen-Datei hat ihre eigene kleine Kopie). Ohne Scroll-Unterstuetzung,
// da der Infotext des Wetter-Popups kurz genug ist, um immer komplett in die
// verfuegbare Boxhoehe zu passen.
int16_t weatherInfoLayoutWrapped(TFT_eSPI& tftRef, int16_t x, int16_t startY,
                                  int16_t maxWidth, int16_t lineHeight, const String& text) {
    int16_t y = startY;
    int32_t start = 0;
    int32_t len = text.length();
    while (start < len) {
        while (start < len && text[start] == ' ') start++;
        if (start >= len) break;

        String line = text.substring(start, len);
        while (tftRef.textWidth(line) > maxWidth) {
            int32_t lastSpace = line.lastIndexOf(' ');
            if (lastSpace <= 0) break;
            line = line.substring(0, lastSpace);
        }

        tftRef.setCursor(x, y);
        tftRef.print(line);
        y += lineHeight;
        start += line.length();
    }
    return y;
}

void weatherInfoDrawButton(TFT_eSPI& tftRef, const Rect& r, const String& label) {
    tftRef.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
    tftRef.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
    tftRef.setTextDatum(MC_DATUM);
    tftRef.setTextColor(TFT_GREEN, TFT_BLACK);
    tftRef.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
    tftRef.setTextDatum(TL_DATUM);
}

// Kleines Info-Fenster beim Antippen des Wetter-Icons im Header - erklaert,
// dass das angezeigte Wetter immer zum aktuell aktiven Standort (bzw.
// aktivem Standort-Preset, siehe LocationManager::getHomeLocation())
// gehoert. Gleicher geboxter Overlay-Stil wie confirmLogbookEnable() in
// menu_screen.cpp, hier aber ohne Warnfarbe und nur mit einem
// Zurueck-Button, da rein informativ (keine Bestaetigung noetig).
void showWeatherInfo(TFT_eSPI& tftRef) {
    constexpr int16_t BOX_X = 4;
    constexpr int16_t BOX_Y = 4;
    constexpr int16_t BOX_W = Config::SCREEN_WIDTH - 2 * BOX_X;
    constexpr int16_t BOX_H = Config::SCREEN_HEIGHT - 2 * BOX_Y;
    constexpr int16_t TEXT_MAX_WIDTH = BOX_W - 20;
    constexpr int16_t LINE_H = 16;
    constexpr int16_t TITLE_Y = BOX_Y + 16;
    // Eine Leerzeile Abstand zwischen Titel und Fliesstext.
    constexpr int16_t VIEW_TOP = TITLE_Y + 12 + LINE_H;

    constexpr int16_t BTN_H = 36;
    constexpr int16_t BOTTOM_MARGIN = 8;
    constexpr int16_t BACK_Y = BOX_Y + BOX_H - BOTTOM_MARGIN - BTN_H;

    String body = I18n::t(StringId::WEATHER_INFO_BODY);

    Rect backBtn = {(int16_t)(BOX_X + 10), BACK_Y, (int16_t)(BOX_W - 20), BTN_H};

    MenuStars::reset();

    auto redraw = [&]() {
        tftRef.fillScreen(TFT_BLACK);
        tftRef.drawRoundRect(BOX_X, BOX_Y, BOX_W, BOX_H, 6, TFT_GREEN);

        tftRef.setTextDatum(MC_DATUM);
        tftRef.setTextColor(TFT_GREEN, TFT_BLACK);
        tftRef.setTextSize(2);
        tftRef.drawString(I18n::t(StringId::WEATHER_INFO_TITLE), BOX_X + BOX_W / 2, TITLE_Y);
        tftRef.setTextSize(1);
        tftRef.setTextDatum(TL_DATUM);

        tftRef.setTextColor(TFT_GREEN, TFT_BLACK);
        weatherInfoLayoutWrapped(tftRef, BOX_X + 10, VIEW_TOP, TEXT_MAX_WIDTH, LINE_H, body);

        weatherInfoDrawButton(tftRef, backBtn, I18n::t(StringId::BACK));
    };

    redraw();

    while (true) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            if (backBtn.contains(tap.x, tap.y)) return;
        }
        MenuStars::update(tftRef);
        delay(20);
    }
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

// Grosse, zentrierte Uhrzeit fuer den Ruhebildschirm (siehe
// screensaverShowing oben) - eigene, groessere Variante der kleinen
// Kopfzeilen-Uhr aus updateStatusLine(), da diese bewusst kompakt gehalten
// ist. Loescht bei jedem Aufruf nur den eigenen schmalen Streifen in der
// Bildschirmmitte (nicht den ganzen Bildschirm), damit die Sternenanimation
// darum herum ungestoert weiterlaeuft. Zeichnet nichts, solange die Uhrzeit
// noch nicht per NTP synchronisiert ist (gleiche Pruefung wie
// updateStatusLine()/isNightDimHours()).
void drawScreensaverClock() {
    time_t now = time(nullptr);
    if (now <= 8 * 3600 * 2) return;

    struct tm tmNow;
    localtime_r(&now, &tmNow);
    char timeBuf[9];
    if (LocationManager::useMetricUnits()) {
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tmNow.tm_hour, tmNow.tm_min);
    } else {
        int hour12 = tmNow.tm_hour % 12;
        if (hour12 == 0) hour12 = 12;
        snprintf(timeBuf, sizeof(timeBuf), "%d:%02d%s", hour12, tmNow.tm_min,
                 tmNow.tm_hour < 12 ? "AM" : "PM");
    }

    constexpr int16_t CLOCK_BAND_H = 40;
    int16_t cy = Config::SCREEN_HEIGHT / 2;
    tft.fillRect(0, (int16_t)(cy - CLOCK_BAND_H / 2), Config::SCREEN_WIDTH, CLOCK_BAND_H, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
    tft.setTextSize(3);
    tft.drawString(timeBuf, Config::SCREEN_WIDTH / 2, cy);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
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
    drawMenuButton();
    updateWifiIcon();
    drawWeatherIcon();
    lastRenderedWeather = Weather::current();
}

void updateStatusLine() {
    if (wasEmergency) return;

    // Die Uhrzeit nutzt den global gesetzten 11pt-Font (setFreeFont() in
    // setup()) ueber setCursor()+print() - bei GFXFF-Fonts ist das
    // baseline-verankert, der Text waechst also nach OBEN (siehe
    // CLAUDE.md-Hinweis zu diesem Pitfall). Die Ziffern-Glyphen sind laut
    // Font-Metrik 8px hoch und reichen damit bis y=HEADER_TITLE_H-6 - also
    // OBERHALB des schmalen STATUS_LINE_H-Bereichs, der hier bisher allein
    // geloescht wurde. Dadurch blieben alte Ziffern-Reste stehen und
    // ueberlagerten sich mit den neuen (am sichtbarsten bei der letzten
    // Minutenziffer, die sich am haeufigsten aendert). Deshalb hier gezielt
    // NUR unter der Uhrzeit einen hoeheren Bereich loeschen - nicht die
    // ganze Zeile, sonst wuerde das jede Sekunde in den Menu-Button
    // hineinschneiden, der bis y=25 reicht.
    // Auf 80px verbreitert (vorher 50) - das 12h-Format mit AM/PM (siehe
    // unten) ist mit bis zu 7 Zeichen ("12:59PM") laenger als das feste
    // 5-Zeichen-24h-Format ("23:12") und wurde sonst nicht vollstaendig
    // geloescht (Ziffernreste blieben stehen). Der Bereich rechts daneben
    // war hier ohnehin leer, daher unkritisch.
    constexpr int16_t CLOCK_CLEAR_W = 80;
    constexpr int16_t CLOCK_CLEAR_TOP = HEADER_TITLE_H - 10;
    tft.fillRect(0, CLOCK_CLEAR_TOP, CLOCK_CLEAR_W, CONTENT_TOP - CLOCK_CLEAR_TOP, TFT_BLACK);
    tft.fillRect(CLOCK_CLEAR_W, HEADER_TITLE_H, Config::SCREEN_WIDTH - CLOCK_CLEAR_W, STATUS_LINE_H, TFT_BLACK);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);

    time_t now = time(nullptr);
    if (now > 8 * 3600 * 2) {
        struct tm tmNow;
        localtime_r(&now, &tmNow);
        char timeBuf[9];
        // 12h mit AM/PM bei Imperial (in den USA ueblich), 24h bei
        // Metrisch - dieselbe Einheiten-Einstellung (Menue > Einheiten),
        // die sonst Distanz/Hoehe steuert. Vorher immer fest 24h.
        if (LocationManager::useMetricUnits()) {
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tmNow.tm_hour, tmNow.tm_min);
        } else {
            int hour12 = tmNow.tm_hour % 12;
            if (hour12 == 0) hour12 = 12;
            snprintf(timeBuf, sizeof(timeBuf), "%d:%02d%s", hour12, tmNow.tm_min,
                     tmNow.tm_hour < 12 ? "AM" : "PM");
        }
        tft.setCursor(6, HEADER_TITLE_H + 2);
        tft.print(timeBuf);
    }

    updateWifiIcon();

    // Icon nur bei tatsaechlicher Aenderung neu zeichnen (Weather::update()
    // laeuft im Hintergrund auf Core 0 und aktualisiert typischerweise nur
    // alle paar Minuten) - vermeidet unnoetiges Neuzeichnen jede Sekunde.
    Weather::Condition weatherNow = Weather::current();
    if (weatherNow != lastRenderedWeather) {
        lastRenderedWeather = weatherNow;
        drawWeatherIcon();
    }
}

// Prueft, ob gerade Nacht ist (fuer die Nachtdimmung). Nutzt den echten
// Sonnenauf-/untergang am aktiven Standort (siehe sun_times.h), NICHT mehr
// ein festes 22:00-06:00-Fenster - im Sommer war das vorher oft noch hell
// draussen, wenn schon gedimmt wurde, und im Winter blieb es nach 6 Uhr noch
// lange dunkel, ohne dass gedimmt wurde. Solange Standort oder Uhrzeit noch
// nicht bekannt sind (z.B. kurz nach dem Start, bevor NTP/GPS/IP-Geolocation
// fertig sind), faellt die Funktion auf das alte feste Fenster zurueck,
// damit die Nachtdimmung nicht komplett ausfaellt.
bool isNightDimHours() {
    time_t now = time(nullptr);
    if (now <= 8 * 3600 * 2) return false;

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

    // Fallback: Standort noch unbekannt.
    int hour = tmNow.tm_hour;
    return (hour >= 22 || hour < 6);
}

// Sanfte Nachtdimmung des Backlights zwischen Sonnenuntergang und
// Sonnenaufgang (siehe isNightDimHours()), sofern in den Einstellungen
// aktiviert. Der Inaktivitaets-Timeout (screenDimmed) hat Vorrang und wird
// hier nicht ueberschrieben.
void updateNightDimming() {
    if (screenDimmed) return;

    bool shouldDim = SettingsStore::nightDimmingEnabled() && isNightDimHours();
    if (shouldDim != nightDimActive) {
        ledcWrite(BACKLIGHT_PWM_CHANNEL, shouldDim ? nightDimBacklightPwm() : normalBacklightPwm());
        nightDimActive = shouldDim;
    }
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
    tft.setRotation(Config::DISPLAY_ROTATION);
    tft.setFreeFont(&UiFont11pt);
    tft.invertDisplay(true);
    tft.fillScreen(TFT_BLACK);

    ledcSetup(BACKLIGHT_PWM_CHANNEL, 5000, 8);
    ledcAttachPin(TFT_BL, BACKLIGHT_PWM_CHANNEL);
    ledcWrite(BACKLIGHT_PWM_CHANNEL, normalBacklightPwm());

    TouchInput::begin();
    LedAlert::begin();

    bool sdOk = SdStorage::init();
    if (!sdOk) {
        haltWithSdRequiredScreen();
        return;
    }

    bool isFirstRun = !SD.exists(Config::SD_SETTINGS_FILE);

    // The Flightradar folder structure and default data files are
    // deliberately NOT created yet on a first boot - that happens further
    // down, after the Start button on the Welcome screen has been tapped
    // (the button is "the gate to the app": nothing should exist on the
    // card before that). On every later boot the structure already
    // exists anyway - creating it immediately here is harmless,
    // ensureDir()/writeIfAbsent() are idempotent.
    if (!isFirstRun) {
        SdStorage::createStructure();
        SdStorage::seedDefaultDataFiles();
    }

    SettingsStore::load();
    tft.invertDisplay(SettingsStore::displayInverted());
    // Der erste ledcWrite() oben (vor SettingsStore::load()) kannte die
    // gespeicherte Helligkeit noch nicht und hat den Default (100%)
    // angewendet - hier mit dem jetzt geladenen Wert korrigieren, gleiches
    // Nachziehen wie bei invertDisplay() direkt drueber.
    ledcWrite(BACKLIGHT_PWM_CHANNEL, normalBacklightPwm());

    WifiMgr::init();
    LocationPresets::init();
    AirlineFilter::init();
    AircraftWatchlist::init();

    SplashScreen::begin(tft);
    // Sofort einen ersten Sternen-Frame zeichnen, statt erst auf die naechste
    // Warteschleife (WLAN/Standort) zu warten - sonst blieb der Sternenhimmel
    // bei schnellem Boot (gespeichertes WLAN, schnelle Standortermittlung)
    // fast die ganze Splash-Anzeige ueber unsichtbar und "poppte" erst kurz
    // vor dem Radarscreen auf.
    MenuStars::update(tft);
    SplashScreen::setStatusLine(tft, 0, I18n::t(StringId::SPLASH_SD_OK), TFT_WHITE);

    // Willkommens-Screen laeuft nur beim allerersten Start (isFirstRun) und
    // bewusst VOR der Touch-Kalibrierung - der erste Eindruck, bevor man
    // ueberhaupt zum Kalibrieren aufgefordert wird. Englisch hart kodiert,
    // da die Sprachauswahl (FirstRunLanguageScreen) erst danach kommt,
    // siehe first_run_welcome_screen.cpp.
    if (isFirstRun) {
        // Creating the folder structure, seeding default data files, AND
        // showing the loading indicator during those (noticeably slow) SD
        // accesses now all happen directly inside
        // FirstRunWelcomeScreen::run() (triggered by the Start tap) - see
        // there for why (button otherwise looked frozen, repeated tapping
        // let a tap leak into calibration).
        FirstRunWelcomeScreen::run(tft);
    }

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
        // Erst NACH der Sprachauswahl (Texte erscheinen dann gleich in der
        // richtigen Sprache) und NACH dem WLAN-Verbindungsversuch weiter
        // oben (die Adresssuche braucht eine Internetverbindung) -
        // ueberspringbar, siehe first_run_location_screen.cpp.
        FirstRunLocationScreen::run(tft);
        // Abschluss-Screen: bestaetigt das Ende der Ersteinrichtung, nennt
        // den SD-Ordner mit allen gespeicherten Daten und weist auf den
        // automatischen WLAN-Verbindungsaufbau ab dem naechsten Start hin.
        FirstRunCompleteScreen::run(tft);
        SplashScreen::begin(tft);
        MenuStars::update(tft);
        SplashScreen::setStatusLine(tft, 0, I18n::t(StringId::SPLASH_SD_OK), TFT_WHITE);
    }

    AdsbClient::primeTime();

    SplashScreen::setStatusLine(tft, 2, I18n::t(StringId::SPLASH_GETTING_LOCATION));
    LocationManager::init();
    uint32_t locStart = millis();
    // Bewusst NICHT nur warten, solange noch GAR KEINE Position bekannt ist:
    // ab dem zweiten Boot ist meistens schon eine gespeicherte Position da
    // (Source::Persisted, siehe LocationManager::init()), wodurch diese
    // Schleife sofort uebersprungen wurde und requestIpLookupIfNeeded() nie
    // wieder lief. Genau die liefert aber auch die UTC-Zeitzonenverschiebung
    // (inkl. Sommer-/Winterzeit) - ohne sie blieb die Uhr dauerhaft auf UTC
    // stehen (in Mitteleuropa je nach Jahreszeit 1-2h falsch).
    while ((LocationManager::currentSource() == LocationManager::Source::None ||
            !LocationManager::hasUtcOffset()) &&
           millis() - locStart < 8000) {
        LocationManager::requestIpLookupIfNeeded();
        MenuStars::update(tft);
        delay(200);
    }
    SplashScreen::setStatusLine(tft, 2, I18n::t(StringId::SPLASH_READY));

    // Sobald die IP-Geolocation einen UTC-Offset geliefert hat, die
    // Zeitzone entsprechend setzen - Zeitstempel (Flugbuch, Log-Dateinamen)
    // zeigen dann die ECHTE Ortszeit statt UTC, automatisch weltweit richtig.
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
            ledcWrite(BACKLIGHT_PWM_CHANNEL, shouldNightDim ? nightDimBacklightPwm() : normalBacklightPwm());
            nightDimActive = shouldNightDim;
            screenDimmed = false;
            tapped = false;

            if (screensaverShowing) {
                // Der Ruhebildschirm hat den kompletten Bildschirminhalt
                // ueberschrieben (Sternenhimmel + Uhrzeit) - anders als beim
                // normalen Timeout (nur Backlight aus, Inhalt blieb im
                // TFT-Speicher unveraendert stehen) muss hier explizit neu
                // gezeichnet werden.
                screensaverShowing = false;
                drawHeader();
                updateStatusLine();
                forceRedraw = true;
            }
        }
    }

    if (tapped) {
        if (menuBtn.contains(tap.x, tap.y)) {
            MenuScreen::run(tft);
            drawHeader();
            updateStatusLine();
            forceRedraw = true;
        } else if (weatherIconRect.contains(tap.x, tap.y)) {
            showWeatherInfo(tft);
            drawHeader();
            updateStatusLine();
            forceRedraw = true;
        } else if (tap.y >= CONTENT_TOP) {
            if (RadarScreen::handleTap(tap.x, tap.y, CONTENT_TOP)) {
                forceRedraw = true;
            }
        }
    }

    // Waehrend der Ruhebildschirm aktiv ist (screensaverShowing), duerfen
    // Radar-Rendering/Sweep/Statuszeile den Bildschirm NICHT mehr
    // ueberschreiben - sonst wuerde der naechste Aircraft-Update-Zyklus
    // (oder der Sweep-Tick) den gedimmten Sternenhimmel jederzeit wieder mit
    // dem vollen Radarbild uebermalen, waehrend gleichzeitig
    // drawScreensaverClock() im Sekundentakt seinen Streifen drueberzeichnet
    // - das Ergebnis war sichtbares Geflacker zwischen Radarbild und Uhr
    // statt eines ruhigen Sternenhimmels.
    if (!screensaverShowing) {
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
    }

    uint32_t nowMs = millis();

    RadarScreen::updateProximityAlert(nowMs);

    if (!screensaverShowing) {
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
    }

    uint8_t timeoutMin = SettingsStore::screenTimeoutMinutes();
    if (!screenDimmed && timeoutMin > 0) {
        uint32_t timeoutMs = (uint32_t)timeoutMin * 60000UL;
        if (nowMs - lastInteractionMs >= timeoutMs) {
            if (SettingsStore::screensaverEnabled()) {
                // Ruhebildschirm statt komplett dunkel (Menue > System >
                // Ruhebildschirm, AUS per Default) - gedimmter
                // Sternenhimmel mit grosser Uhrzeit statt des Backlights
                // ganz aus, fuer alle, die trotz Inaktivitaets-Timeout noch
                // etwas auf dem Display sehen wollen.
                screensaverShowing = true;
                ledcWrite(BACKLIGHT_PWM_CHANNEL, nightDimBacklightPwm());
                tft.fillScreen(TFT_BLACK);
                MenuStars::reset();
                lastScreensaverClockMs = 0; // sofortiges erstes Zeichnen erzwingen
            } else {
                // Bewusst ganz aus (0) statt nur gedimmt - fuer den
                // Bildschirm-Timeout (Menue > System > Timeout). Dimmen
                // uebernimmt bereits der separate Nachtmodus
                // (updateNightDimming()); der Inaktivitaets-Timeout soll das
                // Display wirklich abschalten, z.B. fuer Plane-Spotter, die nur
                // das Flugbuch mitlaufen lassen wollen, ohne den Screen zu
                // brauchen. Ein Antippen weckt ihn ueber den screenDimmed-Zweig
                // oben wieder auf die eingestellte Helligkeit (bzw. Nachtmodus-
                // Helligkeit, falls gerade Nachtstunden sind).
                ledcWrite(BACKLIGHT_PWM_CHANNEL, 0);
            }
            screenDimmed = true;
        }
    }

    if (screenDimmed && screensaverShowing) {
        MenuStars::update(tft);
        if (nowMs - lastScreensaverClockMs >= 1000) {
            lastScreensaverClockMs = nowMs;
            drawScreensaverClock();
        }
    }
}