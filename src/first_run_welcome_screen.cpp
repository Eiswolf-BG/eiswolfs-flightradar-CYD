#include "first_run_welcome_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"

namespace FirstRunWelcomeScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // Text bewusst hart codiert (Englisch) statt ueber i18n - dieser Screen
    // laeuft VOR der Sprachauswahl (siehe first_run_language_screen.cpp fuer
    // dasselbe Muster mit "Language / Sprache"), es ist also noch keine
    // Sprache gewaehlt.
    constexpr const char* TITLE = "Welcome to Eiswolfs Flightradar!";

    // Lokale Kopie, siehe Konvention in first_run_location_screen.cpp (jeder
    // Screen haelt seine eigenen kleinen Helfer statt eines gemeinsamen
    // Moduls).
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

    // Radar-Reticle wie im Splash-Screen (splash_screen.cpp::drawRadarReticle,
    // dort mit fest 80/54/29px) - lokale Kopie statt gemeinsames Modul
    // (splash_screen.cpp exportiert die Funktion ohnehin nicht, siehe
    // splash_screen.h). Radius wird hier von aussen uebergeben statt fest
    // kodiert, da auf diesem Screen (Titel + grosser Start-Button)
    // weniger vertikaler Platz uebrig bleibt als im reinen Splash-Screen.
    void drawRadarReticle(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius) {
        uint16_t dim = 0x0320;
        tft.drawCircle(cx, cy, radius, dim);
        tft.drawCircle(cx, cy, (int16_t)(radius * 2 / 3), dim);
        tft.drawCircle(cx, cy, (int16_t)(radius / 3), dim);
        tft.drawFastHLine((int16_t)(cx - radius), cy, (int16_t)(radius * 2), dim);
        tft.drawFastVLine(cx, (int16_t)(cy - radius), (int16_t)(radius * 2), dim);
    }

    // Button-Rahmen im gewohnten Stil (schwarz gefuellt, duenner gruener
    // Rand) - bewusst NICHT komplett gruen ausgefuellt wie der
    // "Flightradar"-Button in first_run_complete_screen.cpp, damit die
    // Sternchen im Hintergrund (MenuStars, ueberzeichnet ohnehin den ganzen
    // Bildschirm inkl. dieser Flaeche) durch den Button hindurch sichtbar
    // bleiben ("Button soll schwarz mit Sternen sein").
    void drawButtonFrame(TFT_eSPI& tft, const Rect& r) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 6, TFT_GREEN);
    }

    // "Start"-Text fett (2x mit 1px Versatz gezeichnet - gleiche Technik wie
    // beim "Flightradar"-Logo in first_run_complete_screen.cpp, der GFXFF-
    // Font hat keine echte Bold-Variante) und "atmend": die Farbe pulsiert
    // langsam zwischen einem dunkleren Gruen (passt zu den anderen gruenen
    // Elementen: Titel, Rahmen, Radar-Reticle) und einem hellen, leicht
    // ins Weissliche gehenden Gruen am Hoehepunkt - NUR "Start" erreicht
    // diesen helleren Ton, alles andere bleibt reines TFT_GREEN, dadurch
    // ist "Start" immer das Element, das sichtbar "herausleuchtet".
    void drawStartText(TFT_eSPI& tft, const Rect& r, uint16_t color) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(color, TFT_BLACK);
        tft.setTextSize(2);
        int16_t cx = r.x + r.w / 2;
        int16_t cy = r.y + r.h / 2;
        tft.drawString("Start", cx, cy);
        tft.drawString("Start", (int16_t)(cx + 1), cy);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
    }

    uint8_t breathePhase = 0;
    uint32_t lastBreatheMs = 0;
    constexpr uint32_t BREATHE_INTERVAL_MS = 20;
    constexpr uint8_t BREATHE_SPEED = 2;   // ~256/2 Ticks a 20ms => ~2.5s pro Atemzug
    constexpr uint8_t BREATHE_LOW_R = 0,   BREATHE_LOW_G = 170, BREATHE_LOW_B = 0;
    constexpr uint8_t BREATHE_HIGH_R = 110, BREATHE_HIGH_G = 255, BREATHE_HIGH_B = 110;

    void updateBreathe(TFT_eSPI& tft, const Rect& btn) {
        uint32_t now = millis();
        if (now - lastBreatheMs < BREATHE_INTERVAL_MS) return;
        lastBreatheMs = now;

        breathePhase = (uint8_t)(breathePhase + BREATHE_SPEED);
        uint8_t tri = (breathePhase < 128) ? (uint8_t)(breathePhase * 2)
                                            : (uint8_t)((255 - breathePhase) * 2);
        uint8_t r = (uint8_t)(BREATHE_LOW_R + ((BREATHE_HIGH_R - BREATHE_LOW_R) * (uint16_t)tri) / 255);
        uint8_t g = (uint8_t)(BREATHE_LOW_G + ((BREATHE_HIGH_G - BREATHE_LOW_G) * (uint16_t)tri) / 255);
        uint8_t b = (uint8_t)(BREATHE_LOW_B + ((BREATHE_HIGH_B - BREATHE_LOW_B) * (uint16_t)tri) / 255);
        drawStartText(tft, btn, tft.color565(r, g, b));
    }
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);

    int16_t textEndY = layoutWrapped(tft, 10, 14, (int16_t)(Config::SCREEN_WIDTH - 20), 18, TITLE);

    constexpr int16_t BTN_H = 90;
    constexpr int16_t BTN_MARGIN_BOTTOM = 14;
    int16_t btnY = (int16_t)(Config::SCREEN_HEIGHT - BTN_MARGIN_BOTTOM - BTN_H);
    Rect startBtn = {10, btnY, (int16_t)(Config::SCREEN_WIDTH - 20), BTN_H};

    // Radar-Grafik mittig zwischen Titel-Ende und Button-Anfang, Radius aus
    // dem tatsaechlich verfuegbaren Platz errechnet statt hart kodiert -
    // gleiche Lehre wie bei den anderen Ersteinrichtungs-Screens (Text-
    // /Layouthoehe zur Laufzeit messen statt anzunehmen).
    int16_t availTop = (int16_t)(textEndY + 6);
    int16_t availBottom = (int16_t)(btnY - 10);
    int16_t midY = (int16_t)(availTop + (availBottom - availTop) / 2);
    int16_t maxRadiusByHeight = (int16_t)((availBottom - availTop) / 2 - 4);
    int16_t maxRadiusByWidth = (int16_t)(Config::SCREEN_WIDTH / 2 - 20);
    int16_t radius = min(maxRadiusByHeight, maxRadiusByWidth);
    if (radius < 20) radius = 20;

    drawRadarReticle(tft, Config::SCREEN_WIDTH / 2, midY, radius);

    drawButtonFrame(tft, startBtn);
    drawStartText(tft, startBtn, TFT_GREEN);

    while (true) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            if (startBtn.contains(tap.x, tap.y)) return;
            continue;
        }
        MenuStars::update(tft);
        updateBreathe(tft, startBtn);
        delay(20);
    }
}

}
