#include "net_task.h"
#include "config.h"
#include "wifi_manager.h"
#include "location_manager.h"
#include "adsb_client.h"
#include "aircraft_table.h"
#include "aircraft.h"
#include "settings_store.h"
#include "aircraft_details.h"
#include "flight_logbook.h"
#include "led_alert.h"
#include "web_export_server.h"
#include "weather.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

namespace NetTask {

namespace {
    TaskHandle_t taskHandle = nullptr;
    uint32_t lastFetchMs = 0;

    Aircraft tempTable[Config::MAX_TRACKED_AIRCRAFT];

    bool webServerStarted = false;

    void taskFunc(void*) {
        for (;;) {
            WifiMgr::update();
            LocationManager::update();

            if (!webServerStarted && WifiMgr::getState() == WifiMgr::State::Connected) {
                WebExportServer::begin();
                webServerStarted = true;
            }
            if (webServerStarted) {
                WebExportServer::update();
            }

            AircraftDetails::update();
            Weather::update();

            if (millis() - lastFetchMs >= Config::FETCH_INTERVAL_MS) {
                lastFetchMs = millis();

                if (WifiMgr::getState() == WifiMgr::State::Connected) {
                    LocationManager::requestIpLookupIfNeeded();

                    double lat = 0, lon = 0;
                    LocationManager::getHomeLocation(lat, lon);

                    float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];

                    auto result = AdsbClient::fetch(lat, lon, rangeKm,
                                                     tempTable, Config::MAX_TRACKED_AIRCRAFT);

                    if (result.ok) {
                        AircraftTable::lock();
                        memcpy(AircraftTable::raw(), tempTable,
                               sizeof(Aircraft) * Config::MAX_TRACKED_AIRCRAFT);
                        AircraftTable::postFetchUpdate(lat, lon);
                        AircraftTable::unlock();

                        FlightLogbook::update();

                        // Kurzer gruener LED-Blitz als "Herzschlag" - zeigt,
                        // dass gerade eine Abfrage gelaufen ist. Wird von
                        // LedAlert::update() automatisch ignoriert, solange
                        // ein Naeherungs-/Notfall-Alarm aktiv ist.
                        if (SettingsStore::ledHeartbeatEnabled()) {
                            LedAlert::pulseHeartbeat(millis());
                        }
                    } else {
                        Serial.printf("[NetTask] Abfrage fehlgeschlagen (HTTP %d)\n", result.httpCode);
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void begin() {
    xTaskCreatePinnedToCore(
        taskFunc,
        "NetTask",
        20480,
        nullptr,
        1,
        &taskHandle,
        0
    );
}

void pause() {
    if (taskHandle) vTaskSuspend(taskHandle);
}

void resume() {
    if (taskHandle) vTaskResume(taskHandle);
}

}