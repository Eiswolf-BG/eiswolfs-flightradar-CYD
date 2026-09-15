#include "session_stats_screen.h"
#include "session_stats.h"
#include "settings_store.h"
#include "location_manager.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "units.h"
#include "ui_theme.h"

namespace SessionStatsScreen {

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

    // Wortweiser Zeilenumbruch anhand echter Pixelbreite (CLAUDE.md-Pflicht
    // fuer Text variabler Laenge) - lokal dupliziert, gleiches Muster wie in
    // mehreren anderen Screens dieser Session (z.B. system_status_screen.cpp).
    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                          int16_t lineHeight, const String& text) {
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

            tft.setCursor(x, y);
            tft.print(line);
            y += lineHeight;
            start += line.length();
        }
        return y;
    }
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();

    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        int16_t y = layoutWrapped(tft, 10, 14, Config::SCREEN_WIDTH - 20, 14, I18n::t(StringId::SESSION_STATS_TITLE));

        // Deutlich gedimmt, kleinere Schriftgroesse waere hier eleganter,
        // aber der eigene Font hat nur eine feste Groesse pro Screen (siehe
        // CLAUDE.md) - Dimmung allein reicht, um die Zeile klar als
        // Nebeninfo statt als weiteren Messwert erkennbar zu machen.
        tft.setTextColor(UiTheme::accentColorDimmed(tft, 0.55f), TFT_BLACK);
        y = layoutWrapped(tft, 10, (int16_t)(y + 4), Config::SCREEN_WIDTH - 20, 14,
                           I18n::t(StringId::SESSION_STATS_SINCE_BOOT_NOTE));
        y += 10;

        bool metric = LocationManager::useMetricUnits();
        SessionStats::Snapshot s = SessionStats::get();
        String noData = I18n::t(StringId::SESSION_STATS_NO_DATA);

        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        y = layoutWrapped(tft, 10, y, Config::SCREEN_WIDTH - 20, 18,
                           String(I18n::t(StringId::SESSION_STATS_UNIQUE_AIRCRAFT_PREFIX)) + s.uniqueAircraftCount);

        String closestValue = noData;
        if (s.hasClosest) {
            char buf[24];
            if (metric) {
                snprintf(buf, sizeof(buf), "%s (%.1fkm)", s.closestCallsign, s.closestDistanceKm);
            } else {
                snprintf(buf, sizeof(buf), "%s (%.1fnm)", s.closestCallsign, Units::kmToNm(s.closestDistanceKm));
            }
            closestValue = buf;
        }
        y = layoutWrapped(tft, 10, y, Config::SCREEN_WIDTH - 20, 18,
                           String(I18n::t(StringId::SESSION_STATS_CLOSEST_PREFIX)) + closestValue);

        String speedValue = noData;
        if (s.hasMaxSpeed) {
            char buf[16];
            if (metric) {
                snprintf(buf, sizeof(buf), "%.0fkm/h", Units::ktToKmh(s.maxSpeedKt));
            } else {
                snprintf(buf, sizeof(buf), "%.0fkt", s.maxSpeedKt);
            }
            speedValue = buf;
        }
        y = layoutWrapped(tft, 10, y, Config::SCREEN_WIDTH - 20, 18,
                           String(I18n::t(StringId::SESSION_STATS_MAX_SPEED_PREFIX)) + speedValue);

        String altValue = noData;
        if (s.hasMaxAlt) {
            char buf[16];
            if (metric) {
                snprintf(buf, sizeof(buf), "%ldm", (long)Units::feetToMeters((float)s.maxAltFt));
            } else {
                snprintf(buf, sizeof(buf), "%ldft", (long)s.maxAltFt);
            }
            altValue = buf;
        }
        y = layoutWrapped(tft, 10, y, Config::SCREEN_WIDTH - 20, 18,
                           String(I18n::t(StringId::SESSION_STATS_MAX_ALT_PREFIX)) + altValue);

        String topTypeValue = noData;
        if (s.hasTopType) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%s (%ux)", s.topType, (unsigned)s.topTypeCount);
            topTypeValue = buf;
        }
        y = layoutWrapped(tft, 10, y, Config::SCREEN_WIDTH - 20, 18,
                           String(I18n::t(StringId::SESSION_STATS_TOP_TYPE_PREFIX)) + topTypeValue);

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }
        if (done) break;

        if (backBtn.contains(tap.x, tap.y)) done = true;
    }
}

}
