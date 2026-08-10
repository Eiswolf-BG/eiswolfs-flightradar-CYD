#pragma once
#include <Arduino.h>

namespace Config {
    // Wird bei jedem Versions-Release von Karl aktualisiert (siehe
    // CLAUDE.md-Workflow "Standard-Workflow: Push & Release") - erscheint
    // im Info-Screen (Menue > System > Info) und muss zum jeweiligen
    // Git-Tag passen.
    constexpr const char* APP_VERSION = "2.7.3";

    // Display-Helligkeit (Menue > System > Helligkeit), in Prozent.
    // MIN bewusst nicht 0 - ein komplett dunkles Display koennte sonst wie
    // ein Defekt wirken statt wie eine Einstellung.
    constexpr uint8_t BRIGHTNESS_MIN_PERCENT = 10;
    constexpr uint8_t BRIGHTNESS_MAX_PERCENT = 100;
    constexpr uint8_t BRIGHTNESS_STEP_PERCENT = 10;

    // Nachtmodus (22-6 Uhr) dimmt relativ zur jeweils eingestellten normalen
    // Helligkeit, nicht auf einen festen Absolutwert - sonst waere der
    // Dimm-Effekt bei niedrig eingestellter Normalhelligkeit wirkungslos
    // oder wuerde das Display nachts sogar heller machen als tagsueber.
    constexpr uint8_t NIGHT_DIM_REDUCTION_PERCENT = 40;

    constexpr const char* IP_GEO_HOST = "ip-api.com";
    constexpr const char* IP_GEO_PATH = "/json/?fields=status,lat,lon,offset,countryCode";

    struct GpsPinPair { uint8_t rx; uint8_t tx; const char* label; };
    constexpr GpsPinPair GPS_PIN_CANDIDATES[] = {
        {22, 27, "G22/G27"}
    };
    constexpr uint8_t GPS_PIN_CANDIDATE_COUNT = 1;
    constexpr uint32_t GPS_BAUD = 9600;

    constexpr float RANGE_STEPS_KM[] = {10.0f, 25.0f, 50.0f, 100.0f};
    constexpr uint8_t RANGE_STEP_COUNT = 4;
    constexpr uint8_t DEFAULT_RANGE_INDEX = 1;

    constexpr const char* ADSB_API_HOST = "opendata.adsb.fi";
    constexpr uint16_t ADSB_API_PORT = 443;
    constexpr uint32_t FETCH_INTERVAL_MS = 8000;
    constexpr uint32_t HTTP_TIMEOUT_MS = 6000;

    constexpr float DEFAULT_PROXIMITY_ALERT_KM = 8.0f;

    constexpr float LED_ALERT_RADIUS_KM = 3.0f;
    constexpr uint32_t ALERT_RETRIGGER_COOLDOWN_MS = 30000;

    constexpr uint8_t MAX_TRACKED_AIRCRAFT = 40;

    constexpr const char* SD_ROOT_DIR              = "/Flightradar_cyd";
    constexpr const char* SD_AIRLINES_CSV          = "/Flightradar_cyd/airlines.csv";
    constexpr const char* SD_AIRCRAFT_TYPES_CSV    = "/Flightradar_cyd/aircraft_types.csv";
    constexpr const char* SD_AIRPORTS_CSV          = "/Flightradar_cyd/airports.csv";
    constexpr const char* SD_LOG_DIR               = "/Flightradar_cyd/logs";
    constexpr const char* SD_SCREENSHOT_DIR         = "/Flightradar_cyd/screenshots";
    constexpr const char* SD_SETTINGS_FILE         = "/Flightradar_cyd/config.txt";
    constexpr const char* SD_WIFI_CREDENTIALS_FILE = "/Flightradar_cyd/wifi.txt";
    constexpr const char* SD_CALIBRATION_FILE      = "/Flightradar_cyd/calibration.txt";

    constexpr uint8_t SD_SPI_CS_PIN   = 5;
    constexpr uint8_t SD_SPI_MOSI_PIN = 23;
    constexpr uint8_t SD_SPI_MISO_PIN = 19;
    constexpr uint8_t SD_SPI_CLK_PIN  = 18;

    constexpr uint8_t TOUCH_CLK_PIN  = 25;
    constexpr uint8_t TOUCH_CS_PIN   = 33;
    constexpr uint8_t TOUCH_MOSI_PIN = 32;
    constexpr uint8_t TOUCH_MISO_PIN = 39;
    constexpr uint8_t TOUCH_IRQ_PIN  = 36;

    constexpr int16_t SCREEN_WIDTH  = 240;
    constexpr int16_t SCREEN_HEIGHT = 320;

    constexpr float ZONE_BLUE_KM   = 25.0f;
    constexpr float ZONE_YELLOW_KM = 10.0f;
    constexpr float ZONE_AMBER_KM  = 5.0f;
    constexpr float ZONE_VISUAL_KM = 2.0f;

    constexpr uint16_t COLOR_LOW_ALT_THRESHOLD_FT  = 10000;
    constexpr uint16_t COLOR_MID_ALT_THRESHOLD_FT  = 30000;

    constexpr const char* EMERGENCY_SQUAWKS[] = {"7500", "7600", "7700"};
    constexpr uint8_t EMERGENCY_SQUAWK_COUNT = 3;

    constexpr uint8_t MAX_WIFI_NETWORKS = 3;
}