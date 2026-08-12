#include "first_run_welcome_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "sd_storage.h"
#include <math.h>

namespace FirstRunWelcomeScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // Text intentionally hardcoded (English) instead of via i18n - this
    // screen runs BEFORE language selection (see first_run_language_screen.cpp
    // for the same pattern with "Language / Sprache"), so no language has
    // been chosen yet.
    constexpr const char* TITLE = "Welcome to Eiswolfs Flightradar!";

    // Centered, bold (1px-offset technique, same as the "Flightradar"
    // button in first_run_complete_screen.cpp) word-wrapped title -
    // variant of layoutWrapped() (see convention in
    // first_run_location_screen.cpp: every screen keeps its own small
    // helpers instead of a shared module), but using MC_DATUM instead of
    // a left-aligned cursor since the title should be bigger and centered
    // here (this screen represents the app).
    int16_t drawCenteredWrappedBold(TFT_eSPI& tft, int16_t cx, int16_t startY,
                                     int16_t maxWidth, int16_t lineHeight,
                                     uint8_t textSize, const String& text) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(textSize);
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
            int16_t lineCy = (int16_t)(y + lineHeight / 2);
            tft.drawString(line, cx, lineCy);
            tft.drawString(line, (int16_t)(cx + 1), lineCy);
            y += lineHeight;
            start += line.length();
        }
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
        return y;
    }

    // ---- Radar graphic, based on the splash screen (splash_screen.cpp) ----
    // Deliberately duplicated locally (no shared UI modules between
    // screens, see convention in first_run_location_screen.cpp), but
    // extended with a "cy" parameter: the splash screen hardcodes
    // everything for cy=174, here the same graphic (circle, mini jets,
    // mini helicopters, big airplane silhouette) needs to sit at a
    // different vertical position since this screen also needs room for
    // a title and a button. Radius deliberately a bit smaller than on the
    // splash screen (72 instead of 80, same ring ratios kept) - at the
    // full splash radius it looked too cramped between title and button.
    void drawRadarReticle(TFT_eSPI& tft, int16_t cx, int16_t cy) {
        uint16_t dim = 0x0320;
        tft.drawCircle(cx, cy, 72, dim);
        tft.drawCircle(cx, cy, 49, dim);
        tft.drawCircle(cx, cy, 26, dim);
        tft.drawFastHLine((int16_t)(cx - 72), cy, 144, dim);
        tft.drawFastVLine(cx, (int16_t)(cy - 72), 144, dim);
    }

    void rotatePoint(int16_t x, int16_t y, float sinA, float cosA,
                      float px, float py, int16_t& outX, int16_t& outY) {
        outX = x + (int16_t)lroundf(px * cosA - py * sinA);
        outY = y + (int16_t)lroundf(px * sinA + py * cosA);
    }

    // Identical to splash_screen.cpp::drawMiniJet (already takes an
    // absolute center point, no change needed).
    void drawMiniJet(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color, float angleDeg) {
        float rad = angleDeg * (PI / 180.0f);
        float s = sinf(rad), c = cosf(rad);

        int16_t nx, ny, flx, fly, frx, fry;
        rotatePoint(x, y, s, c, 0, -11, nx, ny);
        rotatePoint(x, y, s, c, -3, 11, flx, fly);
        rotatePoint(x, y, s, c, 3, 11, frx, fry);
        tft.fillTriangle(nx, ny, flx, fly, frx, fry, color);

        int16_t wlx, wly, wrx, wry, wcx, wcy;
        rotatePoint(x, y, s, c, -13, 3, wlx, wly);
        rotatePoint(x, y, s, c, 13, 3, wrx, wry);
        rotatePoint(x, y, s, c, 0, -3, wcx, wcy);
        tft.fillTriangle(wlx, wly, wrx, wry, wcx, wcy, color);

        int16_t tlx, tly, trx, try_, ttx, tty;
        rotatePoint(x, y, s, c, -5, 8, tlx, tly);
        rotatePoint(x, y, s, c, 5, 8, trx, try_);
        rotatePoint(x, y, s, c, 0, 13, ttx, tty);
        tft.fillTriangle(tlx, tly, trx, try_, ttx, tty, color);
    }

    // Identical to splash_screen.cpp::drawMiniHeli (already takes an
    // absolute center point, no change needed).
    void drawMiniHeli(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color, float angleDeg) {
        float rad = angleDeg * (PI / 180.0f);
        float s = sinf(rad), c = cosf(rad);

        int16_t rcx, rcy;
        rotatePoint(x, y, s, c, 0, -7, rcx, rcy);
        tft.drawCircle(rcx, rcy, 6, color);

        int16_t b1x, b1y, b2x, b2y;
        rotatePoint(x, y, s, c, -7, -7, b1x, b1y);
        rotatePoint(x, y, s, c, 7, -7, b2x, b2y);
        tft.drawLine(b1x, b1y, b2x, b2y, color);
        rotatePoint(x, y, s, c, 0, -13, b1x, b1y);
        rotatePoint(x, y, s, c, 0, -1, b2x, b2y);
        tft.drawLine(b1x, b1y, b2x, b2y, color);

        int16_t cx1, cy1, cx2, cy2, cx3, cy3;
        rotatePoint(x, y, s, c, 0, -6, cx1, cy1);
        rotatePoint(x, y, s, c, -4, 4, cx2, cy2);
        rotatePoint(x, y, s, c, 4, 4, cx3, cy3);
        tft.fillTriangle(cx1, cy1, cx2, cy2, cx3, cy3, color);

        int16_t tbx1, tby1, tbx2, tby2;
        rotatePoint(x, y, s, c, 0, 4, tbx1, tby1);
        rotatePoint(x, y, s, c, 0, 12, tbx2, tby2);
        tft.drawLine(tbx1, tby1, tbx2, tby2, color);

        int16_t fx1, fy1, fx2, fy2, fx3, fy3;
        rotatePoint(x, y, s, c, -3, 10, fx1, fy1);
        rotatePoint(x, y, s, c, 3, 10, fx2, fy2);
        rotatePoint(x, y, s, c, 0, 15, fx3, fy3);
        tft.fillTriangle(fx1, fy1, fx2, fy2, fx3, fy3, color);
    }

    // splash_screen.cpp::drawAirplane hardcodes all coordinates absolutely
    // (fixed for its cy=174) - here recomputed relative to a passed-in cy
    // instead, so the exact same shape results for any cy (see comment
    // above drawRadarReticle).
    void drawAirplane(TFT_eSPI& tft, int16_t cx, int16_t cy) {
        uint16_t color = TFT_GREEN;

        tft.drawTriangle((int16_t)(cx + 15), (int16_t)(cy - 32),
                          (int16_t)(cx - 15), (int16_t)(cy + 26),
                          (int16_t)(cx - 10), (int16_t)(cy + 28), color);

        tft.drawTriangle((int16_t)(cx + 5), (int16_t)(cy - 10),
                          (int16_t)(cx - 47), (int16_t)(cy + 1),
                          (int16_t)(cx - 3), (int16_t)(cy + 5), color);
        tft.drawTriangle((int16_t)(cx + 5), (int16_t)(cy - 10),
                          (int16_t)(cx + 30), (int16_t)(cy + 37),
                          (int16_t)(cx - 3), (int16_t)(cy + 5), color);

        tft.drawTriangle((int16_t)(cx - 11), (int16_t)(cy + 24),
                          (int16_t)(cx - 23), (int16_t)(cy + 27),
                          (int16_t)(cx - 6), (int16_t)(cy + 36), color);
    }

    // Bundles reticle + all planes/helicopters into exactly the graphic
    // shown on the splash screen ("radar bigger with all the aircraft,
    // exactly like on the splash screen").
    void drawRadarGroup(TFT_eSPI& tft, int16_t cx, int16_t cy) {
        drawRadarReticle(tft, cx, cy);
        uint16_t dimGreen = 0x0320;
        drawMiniJet(tft, (int16_t)(cx - 45), (int16_t)(cy - 39), dimGreen, 25.0f);
        drawMiniJet(tft, (int16_t)(cx + 50), (int16_t)(cy - 14), dimGreen, 160.0f);
        drawMiniHeli(tft, (int16_t)(cx - 35), (int16_t)(cy + 41), dimGreen, 250.0f);
        drawMiniHeli(tft, (int16_t)(cx + 45), (int16_t)(cy - 59), dimGreen, 100.0f);
        drawAirplane(tft, cx, cy);
    }

    // Button frame in the usual style (black fill, thin green border,
    // rounded corners) - deliberately NOT fully green-filled like the
    // "Flightradar" button in first_run_complete_screen.cpp, so the
    // background stars (MenuStars, drawn across the whole screen anyway)
    // stay visible through the button ("button should be black with
    // stars").
    void drawButtonFrame(TFT_eSPI& tft, const Rect& r) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, TFT_GREEN);
    }

    // MC_DATUM centers based on the font metrics, but the built-in
    // TFT_eSPI font renders a few pixels too low with it (a known quirk of
    // that font) - hence a small manual correction offset upward.
    constexpr int16_t START_TEXT_Y_OFFSET = -3;

    // "Start" text bold (drawn twice with a 1px offset) and "breathing" -
    // see updateBreathe() below for the color logic.
    void drawStartText(TFT_eSPI& tft, const Rect& r, uint16_t color) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(color, TFT_BLACK);
        tft.setTextSize(2);
        int16_t cx = r.x + r.w / 2;
        int16_t cy = (int16_t)(r.y + r.h / 2 + START_TEXT_Y_OFFSET);
        tft.drawString("Start", cx, cy);
        tft.drawString("Start", (int16_t)(cx + 1), cy);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
    }

    uint8_t breathePhase = 0;
    uint32_t lastBreatheMs = 0;
    constexpr uint32_t BREATHE_INTERVAL_MS = 20;
    constexpr uint8_t BREATHE_SPEED = 2;   // ~256/2 ticks at 20ms => ~2.5s per breath
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

    // Loading indicator: three dots that light up green one by one -
    // replaces the "Start" text in the button while the SD card is
    // accessed in the background (creating the folder structure + seeding
    // default data files, see runStartSequence()). Without this immediate
    // feedback the button looked "frozen" after being tapped and people
    // tapped again - which caused a tap to get stuck mid-SD-access and be
    // picked up as the first calibration tap at the wrong position (see
    // runStartSequence()).
    void drawLoadingDots(TFT_eSPI& tft, const Rect& r, uint8_t litCount) {
        tft.fillRect((int16_t)(r.x + 2), (int16_t)(r.y + 2), (int16_t)(r.w - 4), (int16_t)(r.h - 4), TFT_BLACK);
        int16_t cy = (int16_t)(r.y + r.h / 2);
        constexpr int16_t SPACING = 16;
        int16_t startX = (int16_t)(r.x + r.w / 2 - SPACING);
        for (uint8_t i = 0; i < 3; i++) {
            int16_t dx = (int16_t)(startX + i * SPACING);
            if (i < litCount) {
                tft.fillCircle(dx, cy, 4, TFT_GREEN);
            } else {
                tft.drawCircle(dx, cy, 4, TFT_GREEN);
            }
        }
    }

    // Runs when "Start" is tapped: immediately shows the loading indicator
    // (instead of the "Start" text), then does the noticeably slow SD
    // accesses (create folder structure + seed default data files - see
    // sd_storage.cpp), and finally drains any lingering touch state before
    // the next screen (usually touch calibration) starts polling for taps.
    //
    // The touch flush at the end is not cosmetic: without it, impatient
    // repeated tapping on the (unresponsive during the SD access) button
    // could leave a touch still "down" right as the next screen starts
    // polling - e.g. getting picked up on the calibration screen as a
    // (wrongly positioned) first target tap, which then skewed the whole
    // calibration and, as a knock-on effect, caused taps elsewhere (like
    // in the WiFi network list) to land in the wrong place too.
    void runStartSequence(TFT_eSPI& tft, const Rect& r) {
        drawLoadingDots(tft, r, 0);

        SdStorage::createStructure();
        drawLoadingDots(tft, r, 1);

        SdStorage::seedDefaultDataFiles();
        drawLoadingDots(tft, r, 2);

        delay(150);
        drawLoadingDots(tft, r, 3);
        delay(150);

        // Wait out and discard any lingering touch - see comment above.
        while (TouchInput::rawPoint().touched) {
            delay(10);
        }
        delay(150);
    }
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);

    // Title: bigger (size 2), bold, centered - "the headline and logo are
    // more important", so much more prominent than the previous small
    // left-aligned version.
    int16_t textEndY = drawCenteredWrappedBold(tft, (int16_t)(Config::SCREEN_WIDTH / 2), 14,
                                                 (int16_t)(Config::SCREEN_WIDTH - 20),
                                                 24, 2, TITLE);

    // Button size computed from the actual width/height of the "Start"
    // text instead of hardcoded ("make the button size variable") -
    // keeps the button compact and centered instead of spanning the
    // full screen width like before.
    tft.setTextSize(2);
    int16_t startTextW = tft.textWidth("Start");
    tft.setTextSize(1);
    constexpr int16_t BTN_PAD_X = 26;
    constexpr int16_t BTN_PAD_Y = 14;
    constexpr int16_t BTN_TEXT_H = 16; // default-font glyph height at setTextSize(2)
    constexpr int16_t BTN_MARGIN_BOTTOM = 16;
    int16_t btnW = (int16_t)(startTextW + 1 + BTN_PAD_X * 2); // +1 for the bold offset
    int16_t btnH = (int16_t)(BTN_TEXT_H + BTN_PAD_Y * 2);
    int16_t btnY = (int16_t)(Config::SCREEN_HEIGHT - BTN_MARGIN_BOTTOM - btnH);
    int16_t btnX = (int16_t)((Config::SCREEN_WIDTH - btnW) / 2);
    Rect startBtn = {btnX, btnY, btnW, btnH};

    // Radar graphic: gets more room than before thanks to the more compact
    // centered title and the smaller button, centered in the remaining
    // space - exactly the same graphic as the splash screen
    // (drawRadarGroup), just with a dynamically computed cy instead of
    // splash_screen.cpp's hardcoded cy=174.
    int16_t availTop = (int16_t)(textEndY + 4);
    int16_t availBottom = (int16_t)(btnY - 8);
    int16_t cy = (int16_t)(availTop + (availBottom - availTop) / 2);

    drawRadarGroup(tft, (int16_t)(Config::SCREEN_WIDTH / 2), cy);

    drawButtonFrame(tft, startBtn);
    drawStartText(tft, startBtn, TFT_GREEN);

    while (true) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            if (startBtn.contains(tap.x, tap.y)) {
                runStartSequence(tft, startBtn);
                return;
            }
            continue;
        }
        MenuStars::update(tft);
        updateBreathe(tft, startBtn);
        delay(20);
    }
}

}
