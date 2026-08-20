#include "menu_stars.h"
#include "config.h"

namespace MenuStars {

namespace {
    constexpr uint8_t STAR_COUNT = 34;
    struct Star {
        int16_t x, y;
        uint8_t phase;
        uint8_t speed;
    };
    Star stars[STAR_COUNT];
    bool initialized = false;

    uint32_t lastUpdateMs = 0;
    constexpr uint32_t UPDATE_INTERVAL_MS = 60;

    void initStars() {
        randomSeed((uint32_t)esp_random());
        for (uint8_t i = 0; i < STAR_COUNT; i++) {
            stars[i].x = (int16_t)random(4, Config::SCREEN_WIDTH - 4);
            stars[i].y = (int16_t)random(4, Config::SCREEN_HEIGHT - 4);
            stars[i].phase = (uint8_t)random(0, 256);
            stars[i].speed = (uint8_t)(1 + random(0, 3));
        }
        initialized = true;
    }
}

void reset() {
    initialized = false;
}

void update(TFT_eSPI& tft, bool gray) {
    if (!initialized) initStars();

    uint32_t now = millis();
    if (now - lastUpdateMs < UPDATE_INTERVAL_MS) return;
    lastUpdateMs = now;

    for (uint8_t i = 0; i < STAR_COUNT; i++) {
        stars[i].phase += stars[i].speed;
        uint8_t bright = (stars[i].phase < 128)
            ? (uint8_t)(stars[i].phase * 2)
            : (uint8_t)((255 - stars[i].phase) * 2);
        uint16_t color = gray ? tft.color565(bright, bright, bright) : tft.color565(0, bright, 0);
        tft.drawPixel(stars[i].x, stars[i].y, color);
    }
}

}