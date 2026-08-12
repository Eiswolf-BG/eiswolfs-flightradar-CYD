#include "settings_backup.h"
#include "config.h"
#include "sd_mutex.h"
#include "sd_storage.h"
#include <SD.h>

namespace SettingsBackup {

namespace {
    constexpr const char* WIFI_BACKUP_FILE = "/Flightradar_cyd/wifi_backup.txt";
    constexpr const char* SETTINGS_BACKUP_FILE = "/Flightradar_cyd/config_backup.txt";

    bool copyFile(const char* srcPath, const char* dstPath) {
        if (!SD.exists(srcPath)) return false;

        File src = SD.open(srcPath, FILE_READ);
        if (!src) return false;

        File dst = SD.open(dstPath, FILE_WRITE);
        if (!dst) { src.close(); return false; }

        constexpr size_t BUF_SIZE = 512;
        static uint8_t buf[BUF_SIZE];
        while (src.available()) {
            size_t n = src.read(buf, BUF_SIZE);
            dst.write(buf, n);
            yield();
        }

        src.close();
        dst.close();
        return true;
    }
}

bool backup(void (*onStep)()) {
    if (!SdStorage::isMounted()) return false;
    SdMutex::Guard guard;

    if (onStep) onStep();
    bool okSettings = copyFile(Config::SD_SETTINGS_FILE, SETTINGS_BACKUP_FILE);
    if (onStep) onStep();
    bool okWifi = copyFile(Config::SD_WIFI_CREDENTIALS_FILE, WIFI_BACKUP_FILE);
    return okSettings || okWifi;
}

bool restore(void (*onStep)()) {
    if (!SdStorage::isMounted()) return false;
    SdMutex::Guard guard;

    if (onStep) onStep();
    bool okSettings = copyFile(SETTINGS_BACKUP_FILE, Config::SD_SETTINGS_FILE);
    if (onStep) onStep();
    bool okWifi = copyFile(WIFI_BACKUP_FILE, Config::SD_WIFI_CREDENTIALS_FILE);
    return okSettings || okWifi;
}

bool hasBackup() {
    if (!SdStorage::isMounted()) return false;
    SdMutex::Guard guard;
    return SD.exists(SETTINGS_BACKUP_FILE) || SD.exists(WIFI_BACKUP_FILE);
}

bool factoryReset() {
    if (!SdStorage::isMounted()) return false;

    bool ok;
    {
        // Guard-Block bewusst vor dem Neustart wieder verlassen (statt
        // ueber die Funktion hinweg zu halten) - reine Vorsicht, auch wenn
        // ESP.restart() den Chip ohnehin sofort zuruecksetzt.
        SdMutex::Guard guard;
        ok = SdStorage::deleteDirectoryRecursive(Config::SD_ROOT_DIR);
    }

    if (ok) {
        delay(200);
        ESP.restart();
        // Wird nie erreicht - ESP.restart() kehrt nicht zurueck.
    }
    return ok;
}

}