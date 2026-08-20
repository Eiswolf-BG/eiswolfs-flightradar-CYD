#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>
#include <qrcode.h>

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
#include "wifi_manage_screen.h"
#include "menu_screen.h"
#include "timeout_screen.h"
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
#include "radar_logo.h"
#include "github_screen_logo_image.h"
#include "ui_font.h"
#include "changelog.h"
#include "ota_update.h"
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
bool lastRenderedUpdateAvailable = false;
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

// Eigene, deutlich staerkere Dimmstufe fuer den Ruhebildschirm (siehe
// Config::SCREENSAVER_DIM_REDUCTION_PERCENT) - die normale Nachtabsenkung
// war dafuer zu hell, da der Ruhebildschirm den ganzen Bildschirminhalt
// ersetzt und oft laengere Zeit laeuft.
uint8_t screensaverBacklightPwm() {
    uint32_t normal = normalBacklightPwm();
    return (uint8_t)(normal * (100 - Config::SCREENSAVER_DIM_REDUCTION_PERCENT) / 100);
}

struct Rect {
    int16_t x, y, w, h;
    bool contains(int16_t px, int16_t py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

Rect menuBtn = {Config::SCREEN_WIDTH - 90, 3, 54, 22};

// Tippbereich fuer den Projekt-Titel oben links ("Eiswolfs FR", siehe
// drawHeader()) - oeffnet den GitHub-QR-Code-Screen (runGithubQrScreen()
// unten). Bewusst grosszuegig bemessen (bis kurz vor weatherIconRect),
// nicht nur exakt der schmale Textbereich selbst - gleiches Prinzip wie
// bei weatherIconRect/wifiIconRect.
Rect titleRect = {0, 0, 100, 24};

// Platz, an dem frueher der Cam-Button war (siehe entfernte Screenshot-
// Funktion) - zeigt jetzt stattdessen ein kleines Wetter-Icon fuer den
// aktuell aktiven Standort (siehe weather.cpp/Weather::update()).
Rect weatherIconRect = {(int16_t)(menuBtn.x - 46), 3, 42, 22};

// Tippbereich fuer die WLAN-Balken oben rechts (siehe updateWifiIcon()/
// drawWifiIcon() unten, WIFI_ICON_W/H dort) - bewusst GROESSER als das
// eigentlich gezeichnete 22x14px-Icon (rechts vom Menu-Button, in der
// Luecke bis zum Bildschirmrand), damit das kleine Icon auch mit
// ungenauem Tippen zuverlaessig trifft - gleiches Prinzip wie bei
// weatherIconRect oben (auch dort ist der Tippbereich groesser als das
// sichtbare Icon). Ueberlappt menuBtn nicht (startet erst 2px rechts von
// dessen Ende).
Rect wifiIconRect = {(int16_t)(menuBtn.x + menuBtn.w + 2), 3,
                      (int16_t)(Config::SCREEN_WIDTH - (menuBtn.x + menuBtn.w + 2) - 2), 22};

// Tippbereich fuer die kleine Kopfzeilen-Uhr (siehe updateStatusLine()) -
// oeffnet den Bildschirm-Timeout-Screen (Menue > System > Bildschirm-
// Timeout), macht damit die komplette Kopfzeile reaktiv (Titel/Menu-
// Button/Wetter/WLAN sind es bereits, siehe titleRect/weatherIconRect/
// wifiIconRect). Breite (80px) entspricht der Breite, die
// updateStatusLine() beim Uhrzeit-Neuzeichnen jede Sekunde loescht
// (CLOCK_CLEAR_W dort). Startet aber bewusst erst UNTER titleRect (dessen
// Bereich bis y=24 reicht), nicht bei CLOCK_CLEAR_TOP (y=20) wie der
// geloeschte Bereich dort - sonst wuerde sich diese Tippzone mit titleRect
// ueberlappen und ein Tipp im Ueberlappungsbereich traefe wegen der
// if/else-Reihenfolge immer titleRect statt hier den Timeout-Screen zu
// oeffnen.
Rect clockRect = {0, (int16_t)(titleRect.y + titleRect.h), 80, (int16_t)(CONTENT_TOP - (titleRect.y + titleRect.h))};

void drawMenuButton() {
    // Folgt jetzt dem auf dem Radar-Screen gewaehlten Farbschema (Menue >
    // System > Radar-Farbschema) statt fest Gruen zu bleiben - der Button
    // gehoert visuell zum Radar-Screen (er ist nur dort dauerhaft sichtbar),
    // alle anderen Screens/Buttons im Projekt bleiben bewusst weiterhin
    // gruen, siehe Kommentar bei RadarScreen::themeColor().
    uint16_t color = RadarScreen::themeColor(tft);
    tft.fillRoundRect(menuBtn.x, menuBtn.y, menuBtn.w, menuBtn.h, 4, TFT_BLACK);
    tft.drawRoundRect(menuBtn.x, menuBtn.y, menuBtn.w, menuBtn.h, 4, color);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString("Menu", menuBtn.x + menuBtn.w / 2, menuBtn.y + menuBtn.h / 2);
    tft.setTextDatum(TL_DATUM);

    if (OtaUpdate::isUpdateAvailable()) {
        // Kleiner roter Punkt oben rechts am Menu-Button - "es gibt etwas
        // Neues zu sehen", analog zu App-Badges auf dem Smartphone. Bewusst
        // rein optisch (keine Zahl/kein Text), da hier ohnehin nur GENAU
        // EIN Zustand angezeigt werden muss (Update verfuegbar oder nicht).
        // Gleiche Bedingung wird auch fuer die Badges auf der "System"-
        // Kachel (menu_screen.cpp) und dem Update-Button selbst benutzt.
        tft.fillCircle((int16_t)(menuBtn.x + menuBtn.w - 3), (int16_t)(menuBtn.y + 3), 3, TFT_RED);
        tft.drawCircle((int16_t)(menuBtn.x + menuBtn.w - 3), (int16_t)(menuBtn.y + 3), 3, TFT_BLACK);
    }
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

// Textform von Weather::Condition (bisher nur als Icon gezeichnet, siehe
// drawWeatherIcon() oben) - fuer die Kurzvorhersage im Wetter-Info-Screen
// (showWeatherInfo() unten) gebraucht, wo ein kleines Icon keinen Platz hat.
const char* conditionLabel(Weather::Condition c) {
    switch (c) {
        case Weather::Condition::Clear:        return I18n::t(StringId::WEATHER_CONDITION_CLEAR);
        case Weather::Condition::PartlyCloudy:  return I18n::t(StringId::WEATHER_CONDITION_PARTLY_CLOUDY);
        case Weather::Condition::Cloudy:        return I18n::t(StringId::WEATHER_CONDITION_CLOUDY);
        case Weather::Condition::Rain:          return I18n::t(StringId::WEATHER_CONDITION_RAIN);
        case Weather::Condition::Snow:           return I18n::t(StringId::WEATHER_CONDITION_SNOW);
        case Weather::Condition::Thunderstorm:  return I18n::t(StringId::WEATHER_CONDITION_THUNDERSTORM);
        case Weather::Condition::Unknown:
        default:
            return "";
    }
}

// Kleines Info-Fenster beim Antippen des Wetter-Icons im Header - erklaert,
// dass das angezeigte Wetter immer zum aktuell aktiven Standort (bzw.
// aktivem Standort-Preset, siehe LocationManager::getHomeLocation())
// gehoert, gefolgt vom aktuellen Flugwetterbericht (METAR) und Sonnenauf-/
// -untergang. Nutzt den gemeinsamen, automatisch scrollbaren Info-Screen aus
// menu_screen.cpp (MenuScreen::showInfoScreen(), gleiches Muster wie z.B.
// beim OTA-Erfolgs-Screen) statt einer eigenen, fest positionierten Anzeige -
// die fruehere Annahme, der Text passe immer ohne Scrollen in die Box,
// stimmte nicht mehr, sobald ein laengerer METAR-Bericht (siehe
// Weather::currentMetar()) den Zurueck-Button ueberlappte (Alex'
// Fotomeldung). showInfoScreen() blendet bei Bedarf automatisch Pfeil-
// Buttons zum Scrollen ein, siehe dortiger Kommentar.
void showWeatherInfo(TFT_eSPI& tftRef) {
    String body = I18n::t(StringId::WEATHER_INFO_BODY);

    // METAR-Flugwetterbericht (siehe weather.cpp::currentMetar()) als
    // zusaetzlicher Absatz - "\n\n" erzeugt eine echte Leerzeile (siehe
    // menu_screen.cpp::layoutWrapped()), "\n" einen einfachen Zeilenumbruch
    // zwischen ICAO-Kennung und Rohtext.
    Weather::Metar metar = Weather::currentMetar();
    if (metar.available) {
        body += "\n\n";
        body += String(I18n::t(StringId::WEATHER_METAR_PREFIX)) + metar.icao + ":\n";
        body += metar.raw;
    }

    // Sonnenauf-/untergang fuer den aktuell aktiven Standort (gleiche
    // Berechnung wie isNightDimHours(), siehe dort) - passt thematisch gut
    // in den Wetter-Info-Screen und die Rechenlogik existierte bereits
    // (bisher nur intern fuer die Nachtdimmung genutzt, nie angezeigt).
    {
        double lat = 0, lon = 0;
        LocationManager::getHomeLocation(lat, lon);
        if (lat != 0.0 || lon != 0.0) {
            time_t now = time(nullptr);
            if (now > 8 * 3600 * 2) { // NTP-Zeit schon synchronisiert (siehe isNightDimHours())
                struct tm tmNow;
                localtime_r(&now, &tmNow);
                SunTimes::Result sun = SunTimes::compute(lat, lon, tmNow.tm_year + 1900, tmNow.tm_mon + 1,
                                                          tmNow.tm_mday, LocationManager::utcOffsetSeconds());
                if (sun.valid) {
                    String sunLine;
                    if (sun.alwaysDay) {
                        sunLine = I18n::t(StringId::WEATHER_POLAR_DAY);
                    } else if (sun.alwaysNight) {
                        sunLine = I18n::t(StringId::WEATHER_POLAR_NIGHT);
                    } else {
                        char sunriseBuf[6];
                        char sunsetBuf[6];
                        int sunriseMin = (int)roundf(sun.sunriseHour * 60.0f) % (24 * 60);
                        int sunsetMin = (int)roundf(sun.sunsetHour * 60.0f) % (24 * 60);
                        snprintf(sunriseBuf, sizeof(sunriseBuf), "%02d:%02d", sunriseMin / 60, sunriseMin % 60);
                        snprintf(sunsetBuf, sizeof(sunsetBuf), "%02d:%02d", sunsetMin / 60, sunsetMin % 60);
                        sunLine = String(I18n::t(StringId::WEATHER_SUNRISE_PREFIX)) + sunriseBuf + "   " +
                                  String(I18n::t(StringId::WEATHER_SUNSET_PREFIX)) + sunsetBuf;
                    }
                    body += "\n\n";
                    body += sunLine;
                }
            }
        }
    }

    // Kurzvorhersage (siehe Weather::currentForecast()) als letzter Absatz -
    // Temperatur in der gerade eingestellten Einheit (Celsius/Fahrenheit,
    // gleiche Umschaltung wie sonst im Projekt ueber
    // LocationManager::useMetricUnits()), gefolgt vom Wetterlage-Text
    // (conditionLabel() oben).
    Weather::Forecast forecast = Weather::currentForecast();
    if (forecast.available) {
        bool metric = LocationManager::useMetricUnits();
        float displayTemp = metric ? forecast.temperatureC : (forecast.temperatureC * 9.0f / 5.0f + 32.0f);
        char tempBuf[12];
        snprintf(tempBuf, sizeof(tempBuf), "%.0f°%s", displayTemp, metric ? "C" : "F");

        body += "\n\n";
        body += String(I18n::t(StringId::WEATHER_FORECAST_PREFIX)) + tempBuf + ", " + conditionLabel(forecast.condition);
    }

    MenuScreen::showInfoScreen(tftRef, I18n::t(StringId::WEATHER_INFO_TITLE), body, TFT_GREEN, I18n::t(StringId::BACK));
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

constexpr int16_t SCREENSAVER_LOGO_CY = 100;
constexpr float SCREENSAVER_LOGO_SCALE = 1.0f;

// Layout darunter, von oben nach unten: kleine graue Versionsnummer direkt
// unter dem Logo, dann - mit etwas Abstand - die grosse Uhrzeit, dann das
// Datum. Alle drei Y-Positionen sind Mittelpunkte (MC_DATUM).
constexpr int16_t SCREENSAVER_VERSION_CY = 192;
constexpr int16_t SCREENSAVER_CLOCK_CY = 236;
constexpr int16_t SCREENSAVER_DATE_CY = 278;

void drawScreensaverLogo() {
    RadarLogo::draw(tft, Config::SCREEN_WIDTH / 2, SCREENSAVER_LOGO_CY, SCREENSAVER_LOGO_SCALE);
}

// Kleine graue Versionsnummer unter dem Logo - dieselbe Config::APP_VERSION,
// die auch auf dem "Nach Update suchen"-Button im System-Menue steht (siehe
// menu_screen.cpp). Aendert sich nie waehrend der Ruhebildschirm aktiv ist,
// deshalb wie das Logo nur EINMAL beim Betreten gezeichnet, nicht jede
// Sekunde neu.
void drawScreensaverVersion() {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    String versionText = String("v") + Config::APP_VERSION;
    tft.drawString(versionText, Config::SCREEN_WIDTH / 2, SCREENSAVER_VERSION_CY);

    if (OtaUpdate::isUpdateAvailable()) {
        // Gleicher kleiner roter Punkt wie am Menu-Button/der "System"-
        // Kachel (siehe drawMenuButton()) - dezent rechts neben der
        // Versionsnummer platziert statt auf ihr, damit der ruhige
        // Ruhebildschirm nicht zu sehr "aufgeweckt" wird. Wird - wie die
        // Versionsnummer selbst - nur EINMAL beim Betreten des
        // Ruhebildschirms gezeichnet (siehe Kommentar oben); ein waehrend
        // des Ruhebildschirms neu gefundenes Update erscheint hier daher
        // erst beim naechsten Betreten des Ruhebildschirms.
        int16_t textHalfWidth = tft.textWidth(versionText) / 2;
        tft.fillCircle((int16_t)(Config::SCREEN_WIDTH / 2 + textHalfWidth + 8), SCREENSAVER_VERSION_CY, 3, TFT_RED);
    }

    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
}

// Datum GEMAESS DER AKTUELLEN SPRACHE formatiert (nicht der
// Einheiten-Einstellung wie die Uhrzeit unten) - Reihenfolge/Trennzeichen
// unterscheiden sich je Sprachraum: DE/TR mit Punkten TT.MM.JJJJ, FR/ES/IT
// mit Schraegstrichen TT/MM/JJJJ, EN im US-Format MM/TT/JJJJ (konsistent zur
// bereits bestehenden Kopplung "Englisch/Imperial -> 12h mit AM/PM" weiter
// unten sowie in updateStatusLine()). SettingsStore::language()-Werte wie in
// i18n.cpp/changelog.cpp: 0=EN, 1=DE, 2=FR, 3=TR, 4=ES, 5=IT.
void formatLocalizedDate(const struct tm& tmNow, char* out, size_t outLen) {
    int day = tmNow.tm_mday;
    int month = tmNow.tm_mon + 1;
    int year = tmNow.tm_year + 1900;

    switch (SettingsStore::language()) {
        case 0: // EN
            snprintf(out, outLen, "%02d/%02d/%04d", month, day, year);
            break;
        case 1: // DE
        case 3: // TR
            snprintf(out, outLen, "%02d.%02d.%04d", day, month, year);
            break;
        default: // FR, ES, IT
            snprintf(out, outLen, "%02d/%02d/%04d", day, month, year);
            break;
    }
}

// Grosse, zentrierte Uhrzeit + Datum fuer den Ruhebildschirm (siehe
// screensaverShowing oben) - eigene, deutlich groessere Variante der
// kleinen Kopfzeilen-Uhr aus updateStatusLine(), da diese bewusst kompakt
// gehalten ist. Loescht bei jedem Aufruf nur die eigenen schmalen Streifen
// um Uhrzeit/Datum (nicht den ganzen Bildschirm), damit die
// Sternenanimation und das Logo/die Versionsnummer darueber ungestoert
// stehen bleiben. Zeichnet nichts, solange die Uhrzeit noch nicht per NTP
// synchronisiert ist (gleiche Pruefung wie updateStatusLine()/
// isNightDimHours()).
// Zuletzt tatsaechlich GEZEICHNETE Uhrzeit/Datum-Strings (nicht nur
// berechnete) - drawScreensaverClock() wird zwar jede Sekunde AUFGERUFEN,
// soll den jeweiligen Streifen aber nur dann loeschen+neu zeichnen, wenn
// sich der Text seit dem letzten Mal wirklich geaendert hat (die Uhrzeit nur
// einmal pro Minute, das Datum quasi nur einmal pro Tag). Vorher wurden
// beide Streifen JEDE Sekunde blind schwarz uebermalt und neu gezeichnet,
// was als sichtbares, abwechselndes Flackern auffiel (siehe Alex' Meldung) -
// gleiches Prinzip wie bei den Detail-Panel-Zeilen in radar_screen.cpp
// (updateMarqueeLine(): "if (!forceFull && m.text == newText) return;").
// Auf leeren String zurueckgesetzt, wann immer der Ruhebildschirm neu
// betreten wird (siehe dortiges "lastScreensaverClockMs = 0;"), damit das
// erste Zeichnen nach dem Betreten immer passiert.
String lastScreensaverTimeText;
String lastScreensaverDateText;

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

    if (lastScreensaverTimeText != timeBuf) {
        lastScreensaverTimeText = timeBuf;

        // WICHTIG: TFT_eSPI zeichnet bei MC_DATUM + Freefont (UiFont11pt) UND
        // unterschiedlicher Text-/Hintergrundfarbe intern selbst ein
        // Hintergrund-Rechteck vor dem Text (siehe TFT_eSPI.cpp drawString()).
        // Dessen Hoehe basiert NICHT auf den tatsaechlich gezeichneten Ziffern,
        // sondern auf dem groessten Ascent/Descent des GESAMTEN Fonts
        // (glyph_ab/glyph_bb, einmalig von setFreeFont() ueber alle Zeichen
        // berechnet) - bei textSize(5) reicht dieses interne Rechteck bis zu
        // 45px unter SCREENSAVER_CLOCK_CY und ueberschrieb damit unsichtbar
        // einen Teil des darunterliegenden Datums, sobald sich die Uhrzeit
        // (nicht das Datum) aenderte. Das war die Ursache des "zusammen-
        // geschobenen" Datums im Ruhebildschirm. textSize(3) haelt dieses
        // interne Rechteck bei SCREENSAVER_CLOCK_CY=236 auf 218..263, also mit
        // 5px Abstand ueber dem Datums-Streifen (Band beginnt bei 268).
        constexpr int16_t CLOCK_BAND_H = 44;
        tft.fillRect(0, (int16_t)(SCREENSAVER_CLOCK_CY - CLOCK_BAND_H / 2), Config::SCREEN_WIDTH, CLOCK_BAND_H, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
        tft.setTextSize(3);
        tft.drawString(timeBuf, Config::SCREEN_WIDTH / 2, SCREENSAVER_CLOCK_CY);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
    }

    char dateBuf[12];
    formatLocalizedDate(tmNow, dateBuf, sizeof(dateBuf));
    if (lastScreensaverDateText != dateBuf) {
        lastScreensaverDateText = dateBuf;

        constexpr int16_t DATE_BAND_H = 20;
        tft.fillRect(0, (int16_t)(SCREENSAVER_DATE_CY - DATE_BAND_H / 2), Config::SCREEN_WIDTH, DATE_BAND_H, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.drawString(dateBuf, Config::SCREEN_WIDTH / 2, SCREENSAVER_DATE_CY);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
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

// Vollbild-QR-Code, der auf das GitHub-Repository des Projekts verlinkt -
// erreichbar durch Antippen des Projekt-Titels ("Eiswolfs FR") oben links
// im Header (siehe titleRect oben, Tap-Handling in loop()). Gleiches
// Zeichen-/Ablaufmuster wie radar_screen.cpp::runFlightQrScreen() (dort
// fuer den Live-Tracking-Link eines konkreten Fluges), hier aber ohne
// dynamischen Inhalt - Titel und Ziel-URL sind fest. Bewusst fest TFT_GREEN
// statt RadarScreen::themeColor() - alle anderen Vollbild-Screens in
// main.cpp (z.B. showWeatherInfo()) bleiben ebenfalls fest gruen, nur der
// persistente Menu-Header-Button folgt dem gewaehlten Farbschema (siehe
// Kommentar bei RadarScreen::themeColor() in radar_screen.h).
void runGithubQrScreen(TFT_eSPI& tftRef) {
    MenuStars::reset();
    tftRef.setTextSize(1);

    // Alex' eigenes Avatar-Bild (auf all seinen Social-Kanaelen genutzt)
    // statt des Radar-Logos vom Splashscreen - als 240x240px Graustufen-
    // RGB565-Bitmap eingebettet (siehe github_screen_logo_image.h), ueber
    // die VOLLE Bildschirmbreite bis knapp ueber den Zurueck-Button (Alex'
    // Wunsch: "Foto in voller Breite direkt auf den Zurueckbutton, 5px
    // darueber"). Der QR-Code liegt oben links im dunklen Bildbereich neben
    // dem Kopf, statt darunter Platz zu beanspruchen.
    constexpr int16_t BACK_BTN_H = 30;
    constexpr int16_t BACK_BTN_TOP = Config::SCREEN_HEIGHT - 40;
    constexpr int16_t LOGO_GAP_ABOVE_BTN = 5;
    constexpr int16_t LOGO_BOTTOM = BACK_BTN_TOP - LOGO_GAP_ABOVE_BTN;
    constexpr int16_t LOGO_TOP = LOGO_BOTTOM - GITHUB_SCREEN_LOGO_H;

    constexpr uint8_t QR_VERSION = 4;
    constexpr int16_t QR_SIZE_MODULES = 33; // Version 4: 4*4+17 = 33
    constexpr int16_t QR_BLOCK = 2;
    constexpr int16_t QR_QUIET = 2;
    constexpr int16_t QR_PIXEL_SIZE = (QR_SIZE_MODULES + 2 * QR_QUIET) * QR_BLOCK;
    // Ganz oben links in die Bildschirmecke (Alex' Wunsch: "sollte das Foto
    // nicht beruehren, ganz hoch ins linke Eck damit") - unabhaengig von
    // LOGO_TOP an der Bildschirmkante ausgerichtet, statt am Bildanfang, da
    // Alex' Foto bis dicht an die obere linke Ecke heranreicht.
    constexpr int16_t QR_X = 4;
    constexpr int16_t QR_Y = 4;

    Rect backBtn = {10, BACK_BTN_TOP, (int16_t)(Config::SCREEN_WIDTH - 20), BACK_BTN_H};

    // Feste Projekt-URL - 56 Zeichen, komfortabel innerhalb der 78-Byte-
    // Kapazitaet von QR-Version 4 bei ECC_LOW (gleiche Version wie beim
    // Flug-QR-Code oben, dessen URLs aehnlich lang sind).
    constexpr const char* GITHUB_URL = "https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD";

    uint8_t qrData[qrcode_getBufferSize(QR_VERSION)];
    QRCode qrcode;
    qrcode_initText(&qrcode, qrData, QR_VERSION, ECC_LOW, GITHUB_URL);

    tftRef.fillScreen(TFT_BLACK);
    // setSwapBytes(true) noetig, damit pushImage() unser Graustufen-Array
    // korrekt (statt farbstichig) darstellt - direkt danach wieder auf
    // false zurueckgesetzt, damit alle anderen Zeichenoperationen (Text,
    // fillRect etc.) unveraendert bleiben.
    tftRef.setSwapBytes(true);
    tftRef.pushImage(0, LOGO_TOP, GITHUB_SCREEN_LOGO_W, GITHUB_SCREEN_LOGO_H, GITHUB_SCREEN_LOGO);
    tftRef.setSwapBytes(false);

    tftRef.fillRect(QR_X, QR_Y, QR_PIXEL_SIZE, QR_PIXEL_SIZE, TFT_WHITE);
    for (uint8_t my = 0; my < qrcode.size; my++) {
        for (uint8_t mx = 0; mx < qrcode.size; mx++) {
            if (qrcode_getModule(&qrcode, mx, my)) {
                int16_t px = (int16_t)(QR_X + (QR_QUIET + mx) * QR_BLOCK);
                int16_t py = (int16_t)(QR_Y + (QR_QUIET + my) * QR_BLOCK);
                tftRef.fillRect(px, py, QR_BLOCK, QR_BLOCK, TFT_BLACK);
            }
        }
    }

    // Grauer statt gruener Button (Alex' Wunsch: "Zurueckbutton soll graue
    // Schrift und eine graue Umrandung haben") - passt zum zurueckhaltenden,
    // graustufigen Look dieses Screens (Foto + graue Sterne).
    tftRef.fillRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, TFT_BLACK);
    tftRef.drawRoundRect(backBtn.x, backBtn.y, backBtn.w, backBtn.h, 4, TFT_LIGHTGREY);
    tftRef.setTextDatum(MC_DATUM);
    tftRef.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tftRef.drawString(I18n::t(StringId::BACK), backBtn.x + backBtn.w / 2, backBtn.y + backBtn.h / 2);
    tftRef.setTextDatum(TL_DATUM);

    while (true) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            if (backBtn.contains(tap.x, tap.y)) return;
        }
        // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
        if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) return;
        // gray=true - graue statt gruene Sterne (Alex' Wunsch), siehe
        // Kommentar bei MenuStars::update().
        MenuStars::update(tftRef, true);
        delay(20);
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
    lastRenderedUpdateAvailable = OtaUpdate::isUpdateAvailable();
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

    // Gleiches Prinzip wie beim Wetter-Icon oben: der rote "Update
    // verfuegbar"-Punkt am Menu-Button wurde bisher nur bei einem vollen
    // drawHeader() neu gezeichnet, nicht periodisch - auf dem normalen,
    // nicht angetippten Radarscreen blieb er deshalb oft minutenlang
    // unsichtbar, obwohl der Hintergrund-Check (siehe ota_update.cpp)
    // laengst ein neues Release gefunden hatte. updateStatusLine() laeuft
    // hier jede Sekunde, unabhaengig vom Ruhebildschirm.
    bool updateAvailableNow = OtaUpdate::isUpdateAvailable();
    if (updateAvailableNow != lastRenderedUpdateAvailable) {
        lastRenderedUpdateAvailable = updateAvailableNow;
        drawMenuButton();
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

// Zeigt einmalig einen "Was ist neu?"-Changelog-Screen, wenn dieser Boot der
// ERSTE nach einem Firmware-Wechsel ist (SettingsStore::lastSeenVersion()
// weicht von der aktuellen Config::APP_VERSION ab) - egal ob der Wechsel per
// OTA-Update oder per USB-Neuflashen passierte. Bewusst NICHT direkt auf dem
// OTA-Erfolgs-Screen (siehe menu_screen.cpp::runOtaUpdateScreen()): dort
// laeuft noch die ALTE Firmware, die den Changelog-Text der neuen Version
// gar nicht kennen kann. Hier dagegen laeuft bereits die neue Firmware
// (dieser Aufruf passiert ja erst nach dem Neustart, in setup()) - ihr
// eigener, korrekt einkompilierter und mehrsprachiger changelogLatest()-Text
// ist deshalb garantiert der richtige.
//
// Der AUFRUF dieser Funktion sitzt bewusst ganz am ENDE von setup() (nach
// WLAN-/Standort-Aufbau, NetTask::begin() und dem allerersten Radarscreen-
// Aufbau), NICHT direkt danach am Anfang: Alex meldete, dass der "OK"-Button
// auf diesem Screen "spaet und ohne Meldung" reagierte und sich anfuehlte,
// als wuerde danach nochmal neu gestartet. Tatsaechlich lag es daran, dass
// der Screen vorher (an dieser fruehen Stelle in setup()) direkt gefolgt
// wurde vom Splash-Screen-Aufbau und dem bis zu 16s langen WLAN-Verbindungs-
// Wartezyklus weiter unten - der Tap auf "OK" wurde zwar sofort erkannt,
// aber danach blieb der Bildschirm durch diese langen Wartezyklen einfach
// unveraendert stehen (kein Feedback), bis der Splash-Screen (ein kompletter
// Bildschirm-Wipe, sieht fast identisch zum eigentlichen Boot-Blackscreen
// aus) auftauchte - das wirkte wie ein zweiter, unerklaerter Neustart.
// Jetzt laeuft dieser Screen ERST NACHDEM WLAN/Standort/NetTask/Radarscreen
// bereits vollstaendig aufgebaut sind - das System "laeuft" also schon, ein
// Tap auf "OK" fuehrt direkt und ohne weitere Wartezyklen dahinter zurueck
// zum (schon fertig gezeichneten) Radarscreen.
void showWhatsNewIfNeeded(TFT_eSPI& tftRef) {
    // otaJustInstalled() sofort konsumieren (auf false zuruecksetzen), egal
    // wie diese Funktion unten entscheidet - so bleibt das Flag garantiert
    // nie ueber diesen einen Boot hinaus "haengen" und kann sich nicht
    // versehentlich auf einen spaeteren, voellig unabhaengigen Neustart
    // auswirken.
    bool otaFlag = SettingsStore::otaJustInstalled();
    if (otaFlag) SettingsStore::setOtaJustInstalled(false);

    String lastSeen = SettingsStore::lastSeenVersion();
    bool versionChanged = (lastSeen != Config::APP_VERSION);
    if (versionChanged) {
        SettingsStore::setLastSeenVersion(Config::APP_VERSION);
    }

    // Nur zeigen, wenn BEIDES zutrifft: die Version hat sich geaendert UND
    // dieser Boot folgt direkt auf ein ueber das Menue ausgeloestes OTA-
    // Update (otaFlag) - ein einfaches Neuflashen per USB mit einer anderen
    // Versionsnummer (z.B. zu Testzwecken) soll den Changelog-Screen
    // AUSDRUECKLICH NICHT zeigen (Alex' Meldung: der Screen erschien nach
    // einem reinen Test-Flash, obwohl gar kein OTA-Update angestossen
    // wurde).
    if (!versionChanged || !otaFlag) return;

    String body = String(I18n::t(StringId::OTA_CHANGELOG_LABEL)) + "\n" + Config::changelogLatest();
    MenuScreen::showInfoScreen(tftRef, I18n::t(StringId::OTA_UPDATE_SUCCESS), body,
                                TFT_GREEN, I18n::t(StringId::OK));
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

    if (isFirstRun) {
        // Beim allerersten Start gibt es keine "vorherige Version", mit der
        // man einen Changelog sinnvoll vergleichen koennte - der Willkommens-
        // Ablauf weiter unten uebernimmt hier die Einfuehrung. Nur den
        // aktuellen Versionsstand vermerken, damit ab dem naechsten
        // Firmware-Wechsel showWhatsNewIfNeeded() korrekt greift.
        SettingsStore::setLastSeenVersion(Config::APP_VERSION);
    }
    // showWhatsNewIfNeeded() selbst wird bewusst NICHT hier (vor WLAN-/
    // Standort-Aufbau) aufgerufen, sondern erst ganz am Ende von setup(),
    // siehe dort - Begruendung ebenfalls dort.

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

    if (!isFirstRun) {
        showWhatsNewIfNeeded(tft);
        // showWhatsNewIfNeeded() zeichnet (falls es etwas anzuzeigen gab)
        // einen Vollbild-Overlay ueber den gerade fertig aufgebauten
        // Radarscreen - genau wie bei jedem anderen Vollbild-Overlay
        // (Menue, Wetter-Info, siehe loop()) muss danach explizit neu
        // gezeichnet werden, sonst haelt RadarScreen::render() faelschlich
        // Panel-Reste des Overlays fuer unveraendert und liesse sie stehen.
        RadarScreen::invalidatePanel();
        drawHeader();
        updateStatusLine();
        RadarScreen::render(tft, CONTENT_TOP);
    }

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
                // War beim Eintritt in den Ruhebildschirm noch ein Flugzeug-
                // Detail-Panel offen, hat der Ruhebildschirm es komplett
                // ueberschrieben - forceRedraw allein zeichnet zwar den
                // Radar-Kreis neu, aber RadarScreen::render() wuerde das
                // Panel selbst faelschlich als unveraendert annehmen (gleicher
                // Flugzeug-Hex-Code wie vorher) und nur die Zeilen mit
                // geaendertem Text neu zeichnen - Reste der Ruhebildschirm-
                // Uhr/des Datums blieben dann im Panel-Bereich sichtbar
                // stehen (Alex' Fotomeldung). Gleicher Grund/gleiche Loesung
                // wie beim Menue/Wetter-Info-Overlay weiter unten, siehe
                // RadarScreen::invalidatePanel().
                RadarScreen::invalidatePanel();
                drawHeader();
                updateStatusLine();
                forceRedraw = true;
            }
        }
    }

    if (tapped) {
        if (titleRect.contains(tap.x, tap.y)) {
            // Vollbild-QR-Code auf das GitHub-Repository - siehe
            // runGithubQrScreen() oben.
            runGithubQrScreen(tft);
            // Gleicher Grund wie bei den anderen Vollbild-Overlays unten -
            // auch dieser Screen ueberschreibt den kompletten Bildschirm.
            RadarScreen::invalidatePanel();
            drawHeader();
            updateStatusLine();
            forceRedraw = true;
        } else if (menuBtn.contains(tap.x, tap.y)) {
            MenuScreen::run(tft);
            // Menue lief als Vollbild-Screen und kann dabei ein evtl. noch
            // offenes Flugzeug-Detail-Panel komplett ueberschrieben haben -
            // ohne diesen Aufruf wuerde RadarScreen::render() faelschlich
            // annehmen, das Panel sei unveraendert noch da (gleicher Hex-
            // Code) und nur die Zeilen mit geaendertem Text neu zeichnen,
            // wodurch Reste des Menues (Buttons/Text) sichtbar stehen
            // blieben - siehe RadarScreen::invalidatePanel().
            RadarScreen::invalidatePanel();
            drawHeader();
            updateStatusLine();
            forceRedraw = true;
        } else if (weatherIconRect.contains(tap.x, tap.y)) {
            showWeatherInfo(tft);
            // Gleicher Grund wie beim Menue oben - auch der Wetter-Info-
            // Screen ist ein Vollbild-Overlay.
            RadarScreen::invalidatePanel();
            drawHeader();
            updateStatusLine();
            forceRedraw = true;
        } else if (wifiIconRect.contains(tap.x, tap.y)) {
            // Gleiche Vollbild-Overlay-WLAN-Einstellungen wie ueber Menue >
            // WLAN erreichbar (siehe menu_screen.cpp) - hier per Antippen
            // der WLAN-Balken direkt vom Radarscreen aus erreichbar, ohne
            // erst durchs Menue zu muessen.
            WifiManageScreen::run(tft);
            // Gleicher Grund wie beim Menue oben - auch dieser Screen ist
            // ein Vollbild-Overlay.
            RadarScreen::invalidatePanel();
            drawHeader();
            updateStatusLine();
            forceRedraw = true;
        } else if (clockRect.contains(tap.x, tap.y)) {
            // Uhr in der Kopfzeile antippen -> direkter Sprung zum
            // Bildschirm-Timeout-Screen (sonst nur ueber Menue > System >
            // Bildschirm-Timeout erreichbar) - macht damit die komplette
            // Kopfzeile reaktiv (Alex' Wunsch).
            TimeoutScreen::run(tft);
            // Gleicher Grund wie bei den anderen Vollbild-Overlays oben -
            // auch dieser Screen ueberschreibt den kompletten Bildschirm.
            RadarScreen::invalidatePanel();
            drawHeader();
            updateStatusLine();
            forceRedraw = true;
        } else if (tap.y >= CONTENT_TOP) {
            if (RadarScreen::handleTap(tft, tap.x, tap.y, CONTENT_TOP)) {
                forceRedraw = true;
            }
            if (RadarScreen::consumeHeaderRedrawFlag()) {
                drawHeader();
                updateStatusLine();
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
                ledcWrite(BACKLIGHT_PWM_CHANNEL, screensaverBacklightPwm());
                tft.fillScreen(TFT_BLACK);
                MenuStars::reset();
                drawScreensaverLogo();
                drawScreensaverVersion();
                lastScreensaverTimeText = "";
                lastScreensaverDateText = "";
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