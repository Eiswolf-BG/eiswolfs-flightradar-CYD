#include "settings_store.h"
#include "config.h"
#include "sd_mutex.h"
#include <SD.h>
#include <cstring>

namespace SettingsStore {

namespace {
    uint8_t rangeIdx = Config::DEFAULT_RANGE_INDEX;
    bool inverted = true; // Dieses Board braucht invertDisplay(true) fuer korrekte Farben (siehe main.cpp)
    // AUS per Default - Tischmontage (180 Grad) ist ein bewusstes Opt-in
    // ueber Menue > System > Anzeige, siehe settings_store.h.
    bool rotated180 = false;
    uint8_t brightnessPct = Config::BRIGHTNESS_MAX_PERCENT;
    bool autoBrightnessOn = false;
    bool emergencyAlertOn = true;
    bool proximityAlertOn = true;
    // AUS per Default = "Einfach"-Modus (bisheriges Verhalten unveraendert).
    bool proximityAlertSmartOn = false;
    // MUSS bei einer frischen Installation aus sein - sonst schreibt sich
    // die SD-Karte unbemerkt voll (siehe Bestaetigungsdialog beim
    // Einschalten in menu_screen.cpp + 24h-Auto-Aus in flight_logbook.cpp).
    bool flightLogbookOn = false;
    uint32_t logbookEnabledAtEpoch = 0;
    char logbookSessionFile[16] = {0};
    // AUS per Default - wird nur von FlightLogbook::checkAutoOff() auf AN
    // gesetzt, wenn die 24h-Sicherheitsabschaltung tatsaechlich greift, und
    // von jedem manuellen Antippen des Flugbuch-Schalters wieder auf AUS
    // zurueckgesetzt (siehe menu_screen.cpp).
    bool logbookAutoOffTriggered = false;
    uint16_t peakTrafficCountVal = 0;
    char peakTrafficDateVal[11] = {0};
    uint32_t peakTrafficEpochVal = 0;
    bool ledHeartbeatOn = true;
    // AN per Default (gleiches Verhalten wie die anderen Alarm-Toggles auf
    // dem LED-Alerts-Screen) - steuert den Web-Alarmton auf der Live-Radar-
    // Webseite bei Watchlist-/Notfall-Treffern (siehe web_export_server.cpp,
    // handleRadarJson()). Kein Geraete-eigener Ton (CYD hat keinen
    // brauchbaren Lautsprecher, bereits getestet/verworfen) - der Ton laeuft
    // stattdessen per Web Audio API im Browser jedes Betrachters, der die
    // Webseite gerade offen hat.
    bool webAudioAlertOn = true;
    uint8_t screenTimeoutMin = 0;
    bool nightDimmingOn = true;
    bool screensaverOn = false;
    bool hideGroundVehiclesOn = true;
    bool onlyHelicoptersOn = false;
    bool onlyLowAltitudeOn = false;
    bool airlineFilterShowOnlyOn = false;
    uint8_t languageIdx = 0;
    uint8_t unitsModeVal = 0;
    // AN per Default (IATA, z.B. "FRA") - bei Aviation-Enthusiasten
    // gelaeufiger als ICAO, betrifft nur frische/zurueckgesetzte Geraete,
    // bestehende gespeicherte Einstellungen bleiben davon unberuehrt.
    // Abschaltbar ueber Menue > Land/Region > Einheiten, siehe
    // settings_store.h.
    bool iataAirportCodesOn = true;
    uint8_t radarThemeIdx = 0;
    // Zwei unabhaengige, ankreuzbare Radar-Extras (radar_theme_screen.cpp) -
    // AUS per Default, siehe Kommentar in settings_store.h.
    bool crtPhosphorOn = false;
    // TESTWEISE - Default auf AN geaendert (siehe Absprache mit Karl).
    // Betrifft nur frische/zurueckgesetzte Geraete ohne gespeicherten Wert -
    // bestehende Geraete behalten ihren in den Preferences gespeicherten
    // Wert, der in applyKeyValue() unten weiterhin Vorrang hat.
    bool radarPulseOn = true;
    bool issMarkerOn = true;
    bool classicRadarOn = false;
    bool militarySquawkDetectionOn = false;
    bool followMeModeOn = false;
    bool perfAutoTuningOn = true;
    bool rainEffectOn = true;
    // AN per Default - bisheriges Verhalten (dreimal Magenta bei
    // verfuegbarem Update, siehe radar_screen.cpp) bleibt unveraendert,
    // solange der Nutzer nicht aktiv abschaltet.
    bool updateLedSignalOn = true;
    // AN per Default - die neue Ereignis-Ecke (Militaer/Squawk-Wachposten/
    // Watchlist/Airline-Filter, siehe radar_screen.cpp::drawEventCorner())
    // soll wie die uebrigen Radar-Darstellung-Extras direkt nutzbar sein,
    // ohne dass man sie erst suchen/aktivieren muss.
    bool eventCornerOverlayOn = true;
    // AN per Default - der Weltkarten-Punktraster-Hintergrund unter dem
    // Radarkreis (radar_screen.cpp::drawWorldMap()) war bisher unbedingt
    // gezeichnet, bleibt fuer bestehende Nutzer also optisch unveraendert,
    // bis jemand aktiv abschaltet (Alex' Meldung: manche Nutzer finden den
    // Hintergrund ablenkend).
    bool worldMapBackgroundOn = true;
    bool mqttOn = false;
    // "host:port" als ein Feld, siehe Kommentar in settings_store.h.
    char mqttBrokerBuf[64] = {0};
    char mqttUserBuf[33] = {0};
    char mqttPassBuf[33] = {0};
    bool ntfyPushOn = false;
    char ntfyPushTopicBuf[48] = {0};
    // "Flight Stories" - automatische Ereignis-Meldungen (Militaer-/
    // Hubschrauber-Sichtung, Tiefflug), siehe radar_screen.cpp::
    // updateProximityAlert(). Eigener Schalter statt an ntfyPushOn
    // gekoppelt, da nicht jeder, der den normalen Notfall-/Watchlist-Push
    // nutzt, auch diese haeufigeren, weniger kritischen Meldungen will.
    // Default AUS (Alex' Wunsch).
    bool ntfyFlightStoriesOn = false;
    char lastSeenVersionBuf[16] = {0};
    // Default 120s = Config::MENU_IDLE_TIMEOUT_MS (bisheriger fester Wert) -
    // damit aendert sich fuer niemanden ungefragt etwas, bis der neue
    // Regler (menu_timeout_screen.cpp) aktiv genutzt wird. 0 = "Nie" (kein
    // automatischer Ruecksprung), analog zu screenTimeoutMin oben.
    uint16_t menuIdleTimeoutSec = 120;

    // MUSS auf SD persistiert werden (nicht nur im RAM halten): wird kurz
    // vor ESP.restart() gesetzt und erst im NAECHSTEN Boot-Zyklus gelesen -
    // ein Neustart loescht den RAM komplett, siehe main.cpp::
    // showWhatsNewIfNeeded().
    bool otaJustInstalledFlag = false;

    // Vom "Update installieren"-Screen gesetzt (menu_screen.cpp::
    // runOtaUpdateScreen()), direkt vor einem gezielten Neustart, BEVOR der
    // eigentliche Download ueberhaupt beginnt - siehe main.cpp::setup(),
    // das dieses Flag ganz frueh (vor WLAN-Manager/NetTask/Radarscreen)
    // ausliest und bei true sofort in MenuScreen::runPendingOtaInstall()
    // springt, statt normal weiterzubooten. Grund (Alex' Diagnose im Chat,
    // maxAlloc-Messung): der Download+Update.begin() lief bisher immer aus
    // der laufenden Sitzung heraus, deren Heap durch Stunden normalen
    // Betriebs (ADS-B/Wetter/ISS/MQTT/ntfy) bereits fragmentiert war -
    // "Updater.cpp: malloc failed" trotz gesund aussehendem freeHeap. Ein
    // frischer Neustart unmittelbar vor dem Download hat einen praktisch
    // unfragmentierten Heap. MUSS auf SD persistiert werden (nicht nur im
    // RAM halten), gleiches Prinzip wie otaJustInstalledFlag oben.
    bool otaPendingInstallFlag = false;
    // Direkter Download-Link (GitHub "browser_download_url" fuer
    // firmware.bin) - bereits bekannt aus der vorherigen checkForUpdate()-
    // Abfrage in der laufenden Sitzung, spart nach dem Neustart eine
    // zweite API-Abfrage. OtaUpdate::CheckInfo::downloadUrl ist char[192]
    // (siehe ota_update.h) - hier ebenso dimensioniert.
    char otaPendingInstallUrlBuf[192] = {0};

    void applyKeyValue(const String& key, const String& value) {
        if (key == "range_index") {
            int v = value.toInt();
            if (v >= 0 && v < Config::RANGE_STEP_COUNT) {
                rangeIdx = (uint8_t)v;
            }
        } else if (key == "invert") {
            inverted = (value.toInt() != 0);
        } else if (key == "rotate_180") {
            rotated180 = (value.toInt() != 0);
        } else if (key == "brightness_percent") {
            int v = value.toInt();
            if (v >= Config::BRIGHTNESS_MIN_PERCENT && v <= Config::BRIGHTNESS_MAX_PERCENT) {
                brightnessPct = (uint8_t)v;
            }
        } else if (key == "auto_brightness") {
            autoBrightnessOn = (value.toInt() != 0);
        } else if (key == "emergency_alert") {
            emergencyAlertOn = (value.toInt() != 0);
        } else if (key == "proximity_alert") {
            proximityAlertOn = (value.toInt() != 0);
        } else if (key == "proximity_alert_smart") {
            proximityAlertSmartOn = (value.toInt() != 0);
        } else if (key == "flight_logbook") {
            flightLogbookOn = (value.toInt() != 0);
        } else if (key == "logbook_enabled_at") {
            logbookEnabledAtEpoch = (uint32_t)value.toInt();
        } else if (key == "logbook_session_file") {
            strncpy(logbookSessionFile, value.c_str(), sizeof(logbookSessionFile) - 1);
            logbookSessionFile[sizeof(logbookSessionFile) - 1] = 0;
        } else if (key == "logbook_auto_off_triggered") {
            logbookAutoOffTriggered = (value.toInt() != 0);
        } else if (key == "peak_traffic_count") {
            int v = value.toInt();
            if (v >= 0 && v <= 0xFFFF) peakTrafficCountVal = (uint16_t)v;
        } else if (key == "peak_traffic_date") {
            strncpy(peakTrafficDateVal, value.c_str(), sizeof(peakTrafficDateVal) - 1);
            peakTrafficDateVal[sizeof(peakTrafficDateVal) - 1] = 0;
        } else if (key == "peak_traffic_epoch") {
            peakTrafficEpochVal = (uint32_t)value.toInt();
        } else if (key == "web_audio_alert") {
            webAudioAlertOn = (value.toInt() != 0);
        } else if (key == "led_heartbeat") {
            ledHeartbeatOn = (value.toInt() != 0);
        } else if (key == "screen_timeout_min") {
            int v = value.toInt();
            if (v >= 0 && v <= Config::SCREEN_TIMEOUT_MAX_MINUTES) screenTimeoutMin = (uint8_t)v;
        } else if (key == "menu_idle_timeout_sec") {
            int v = value.toInt();
            if (v == 0 || (v >= Config::MENU_IDLE_TIMEOUT_MIN_SECONDS && v <= Config::MENU_IDLE_TIMEOUT_MAX_SECONDS)) {
                menuIdleTimeoutSec = (uint16_t)v;
            }
        } else if (key == "night_dimming") {
            nightDimmingOn = (value.toInt() != 0);
        } else if (key == "screensaver") {
            screensaverOn = (value.toInt() != 0);
        } else if (key == "hide_ground_vehicles") {
            hideGroundVehiclesOn = (value.toInt() != 0);
        } else if (key == "only_helicopters") {
            onlyHelicoptersOn = (value.toInt() != 0);
        } else if (key == "only_low_altitude") {
            onlyLowAltitudeOn = (value.toInt() != 0);
        } else if (key == "airline_filter_show_only") {
            airlineFilterShowOnlyOn = (value.toInt() != 0);
        } else if (key == "language") {
            int v = value.toInt();
            if (v >= 0 && v <= 7) languageIdx = (uint8_t)v;
        } else if (key == "units_mode") {
            int v = value.toInt();
            if (v >= 0 && v <= 2) unitsModeVal = (uint8_t)v;
        } else if (key == "airport_code_iata") {
            iataAirportCodesOn = (value.toInt() != 0);
        } else if (key == "radar_theme") {
            int v = value.toInt();
            if (v >= 0 && v <= 4) radarThemeIdx = (uint8_t)v;
        } else if (key == "crt_phosphor") {
            crtPhosphorOn = (value.toInt() != 0);
        } else if (key == "radar_pulse") {
            radarPulseOn = (value.toInt() != 0);
        } else if (key == "iss_marker") {
            issMarkerOn = (value.toInt() != 0);
        } else if (key == "classic_radar") {
            classicRadarOn = (value.toInt() != 0);
        } else if (key == "military_squawk_detection") {
            militarySquawkDetectionOn = (value.toInt() != 0);
        } else if (key == "follow_me_mode") {
            followMeModeOn = (value.toInt() != 0);
        } else if (key == "perf_auto_tuning") {
            perfAutoTuningOn = (value.toInt() != 0);
        } else if (key == "rain_effect") {
            rainEffectOn = (value.toInt() != 0);
        } else if (key == "update_led_signal") {
            updateLedSignalOn = (value.toInt() != 0);
        } else if (key == "event_corner_overlay") {
            eventCornerOverlayOn = (value.toInt() != 0);
        } else if (key == "world_map_background") {
            worldMapBackgroundOn = (value.toInt() != 0);
        } else if (key == "mqtt_enabled") {
            mqttOn = (value.toInt() != 0);
        } else if (key == "mqtt_broker") {
            strncpy(mqttBrokerBuf, value.c_str(), sizeof(mqttBrokerBuf) - 1);
            mqttBrokerBuf[sizeof(mqttBrokerBuf) - 1] = 0;
        } else if (key == "mqtt_user") {
            strncpy(mqttUserBuf, value.c_str(), sizeof(mqttUserBuf) - 1);
            mqttUserBuf[sizeof(mqttUserBuf) - 1] = 0;
        } else if (key == "mqtt_pass") {
            strncpy(mqttPassBuf, value.c_str(), sizeof(mqttPassBuf) - 1);
            mqttPassBuf[sizeof(mqttPassBuf) - 1] = 0;
        } else if (key == "ntfy_push_enabled") {
            ntfyPushOn = (value.toInt() != 0);
        } else if (key == "ntfy_flight_stories_enabled") {
            ntfyFlightStoriesOn = (value.toInt() != 0);
        } else if (key == "ntfy_push_topic") {
            strncpy(ntfyPushTopicBuf, value.c_str(), sizeof(ntfyPushTopicBuf) - 1);
            ntfyPushTopicBuf[sizeof(ntfyPushTopicBuf) - 1] = 0;
        } else if (key == "last_seen_version") {
            strncpy(lastSeenVersionBuf, value.c_str(), sizeof(lastSeenVersionBuf) - 1);
            lastSeenVersionBuf[sizeof(lastSeenVersionBuf) - 1] = 0;
        } else if (key == "ota_just_installed") {
            otaJustInstalledFlag = (value.toInt() != 0);
        } else if (key == "ota_pending_install") {
            otaPendingInstallFlag = (value.toInt() != 0);
        } else if (key == "ota_pending_install_url") {
            strncpy(otaPendingInstallUrlBuf, value.c_str(), sizeof(otaPendingInstallUrlBuf) - 1);
            otaPendingInstallUrlBuf[sizeof(otaPendingInstallUrlBuf) - 1] = 0;
        }
    }
}

void load() {
    SdMutex::Guard guard;

    if (!SD.exists(Config::SD_SETTINGS_FILE)) return;

    File f = SD.open(Config::SD_SETTINGS_FILE, FILE_READ);
    if (!f) return;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        int eq = line.indexOf('=');
        if (eq < 0) continue;

        String key = line.substring(0, eq);
        String value = line.substring(eq + 1);
        key.trim();
        value.trim();
        applyKeyValue(key, value);
    }
    f.close();

    // Sicherheitsregel: das Flugbuch darf nach einem Neustart nur dann
    // aktiv bleiben, wenn auch ein gueltiger Einschalt-Zeitstempel
    // vorhanden ist - kein Zeitstempel bedeutet garantiert AUS, egal was in
    // "flight_logbook" steht (verhindert unbemerktes Weiterlaufen z.B. nach
    // einem Firmware-Update oder einer manuell bearbeiteten Datei).
    if (flightLogbookOn && logbookEnabledAtEpoch == 0) {
        flightLogbookOn = false;
    }
}

void save() {
    SdMutex::Guard guard;

    File f = SD.open(Config::SD_SETTINGS_FILE, FILE_WRITE);
    if (!f) return;
    f.printf("range_index=%d\n", rangeIdx);
    f.printf("invert=%d\n", inverted ? 1 : 0);
    f.printf("rotate_180=%d\n", rotated180 ? 1 : 0);
    f.printf("brightness_percent=%d\n", brightnessPct);
    f.printf("auto_brightness=%d\n", autoBrightnessOn ? 1 : 0);
    f.printf("emergency_alert=%d\n", emergencyAlertOn ? 1 : 0);
    f.printf("proximity_alert=%d\n", proximityAlertOn ? 1 : 0);
    f.printf("proximity_alert_smart=%d\n", proximityAlertSmartOn ? 1 : 0);
    f.printf("flight_logbook=%d\n", flightLogbookOn ? 1 : 0);
    f.printf("logbook_enabled_at=%lu\n", (unsigned long)logbookEnabledAtEpoch);
    f.printf("logbook_session_file=%s\n", logbookSessionFile);
    f.printf("logbook_auto_off_triggered=%d\n", logbookAutoOffTriggered ? 1 : 0);
    f.printf("peak_traffic_count=%u\n", peakTrafficCountVal);
    f.printf("peak_traffic_date=%s\n", peakTrafficDateVal);
    f.printf("peak_traffic_epoch=%lu\n", (unsigned long)peakTrafficEpochVal);
    f.printf("led_heartbeat=%d\n", ledHeartbeatOn ? 1 : 0);
    f.printf("web_audio_alert=%d\n", webAudioAlertOn ? 1 : 0);
    f.printf("screen_timeout_min=%d\n", screenTimeoutMin);
    f.printf("menu_idle_timeout_sec=%u\n", menuIdleTimeoutSec);
    f.printf("night_dimming=%d\n", nightDimmingOn ? 1 : 0);
    f.printf("screensaver=%d\n", screensaverOn ? 1 : 0);
    f.printf("hide_ground_vehicles=%d\n", hideGroundVehiclesOn ? 1 : 0);
    f.printf("only_helicopters=%d\n", onlyHelicoptersOn ? 1 : 0);
    f.printf("only_low_altitude=%d\n", onlyLowAltitudeOn ? 1 : 0);
    f.printf("airline_filter_show_only=%d\n", airlineFilterShowOnlyOn ? 1 : 0);
    f.printf("language=%d\n", languageIdx);
    f.printf("units_mode=%d\n", unitsModeVal);
    f.printf("airport_code_iata=%d\n", iataAirportCodesOn ? 1 : 0);
    f.printf("radar_theme=%d\n", radarThemeIdx);
    f.printf("crt_phosphor=%d\n", crtPhosphorOn ? 1 : 0);
    f.printf("radar_pulse=%d\n", radarPulseOn ? 1 : 0);
    f.printf("iss_marker=%d\n", issMarkerOn ? 1 : 0);
    f.printf("classic_radar=%d\n", classicRadarOn ? 1 : 0);
    f.printf("military_squawk_detection=%d\n", militarySquawkDetectionOn ? 1 : 0);
    f.printf("follow_me_mode=%d\n", followMeModeOn ? 1 : 0);
    f.printf("perf_auto_tuning=%d\n", perfAutoTuningOn ? 1 : 0);
    f.printf("rain_effect=%d\n", rainEffectOn ? 1 : 0);
    f.printf("update_led_signal=%d\n", updateLedSignalOn ? 1 : 0);
    f.printf("event_corner_overlay=%d\n", eventCornerOverlayOn ? 1 : 0);
    f.printf("world_map_background=%d\n", worldMapBackgroundOn ? 1 : 0);
    f.printf("mqtt_enabled=%d\n", mqttOn ? 1 : 0);
    f.printf("mqtt_broker=%s\n", mqttBrokerBuf);
    f.printf("mqtt_user=%s\n", mqttUserBuf);
    f.printf("mqtt_pass=%s\n", mqttPassBuf);
    f.printf("ntfy_push_enabled=%d\n", ntfyPushOn ? 1 : 0);
    f.printf("ntfy_flight_stories_enabled=%d\n", ntfyFlightStoriesOn ? 1 : 0);
    f.printf("ntfy_push_topic=%s\n", ntfyPushTopicBuf);
    f.printf("last_seen_version=%s\n", lastSeenVersionBuf);
    f.printf("ota_just_installed=%d\n", otaJustInstalledFlag ? 1 : 0);
    f.printf("ota_pending_install=%d\n", otaPendingInstallFlag ? 1 : 0);
    f.printf("ota_pending_install_url=%s\n", otaPendingInstallUrlBuf);
    f.close();
}

uint8_t rangeIndex() { return rangeIdx; }

void setRangeIndex(uint8_t idx) {
    if (idx < Config::RANGE_STEP_COUNT) {
        rangeIdx = idx;
        save();
    }
}

bool displayInverted() { return inverted; }

void setDisplayInverted(bool inv) {
    inverted = inv;
    save();
}

bool displayRotated180() { return rotated180; }

void setDisplayRotated180(bool rot) {
    rotated180 = rot;
    save();
}

uint8_t brightnessPercent() { return brightnessPct; }

void setBrightnessPercent(uint8_t percent) {
    if (percent >= Config::BRIGHTNESS_MIN_PERCENT && percent <= Config::BRIGHTNESS_MAX_PERCENT) {
        brightnessPct = percent;
        save();
    }
}

bool autoBrightnessEnabled() { return autoBrightnessOn; }

void setAutoBrightnessEnabled(bool on) {
    autoBrightnessOn = on;
    save();
}

bool emergencyAlertEnabled() { return emergencyAlertOn; }

void setEmergencyAlertEnabled(bool on) {
    emergencyAlertOn = on;
    save();
}

bool proximityAlertEnabled() { return proximityAlertOn; }

void setProximityAlertEnabled(bool on) {
    proximityAlertOn = on;
    save();
}

bool proximityAlertSmartMode() { return proximityAlertSmartOn; }

void setProximityAlertSmartMode(bool on) {
    proximityAlertSmartOn = on;
    save();
}

bool flightLogbookEnabled() { return flightLogbookOn; }

void setFlightLogbookEnabled(bool on) {
    flightLogbookOn = on;
    save();
}

uint32_t flightLogbookEnabledAtEpoch() { return logbookEnabledAtEpoch; }

void setFlightLogbookEnabledAtEpoch(uint32_t epoch) {
    logbookEnabledAtEpoch = epoch;
    save();
}

String flightLogbookSessionFile() { return String(logbookSessionFile); }

void setFlightLogbookSessionFile(const String& label) {
    strncpy(logbookSessionFile, label.c_str(), sizeof(logbookSessionFile) - 1);
    logbookSessionFile[sizeof(logbookSessionFile) - 1] = 0;
    save();
}

bool flightLogbookAutoOffTriggered() { return logbookAutoOffTriggered; }

void setFlightLogbookAutoOffTriggered(bool on) {
    logbookAutoOffTriggered = on;
    save();
}

uint16_t peakTrafficCount() { return peakTrafficCountVal; }

void setPeakTrafficCount(uint16_t count) {
    peakTrafficCountVal = count;
    save();
}

String peakTrafficDate() { return String(peakTrafficDateVal); }

void setPeakTrafficDate(const String& date) {
    strncpy(peakTrafficDateVal, date.c_str(), sizeof(peakTrafficDateVal) - 1);
    peakTrafficDateVal[sizeof(peakTrafficDateVal) - 1] = 0;
    save();
}

uint32_t peakTrafficEpoch() { return peakTrafficEpochVal; }

void setPeakTrafficEpoch(uint32_t epoch) {
    peakTrafficEpochVal = epoch;
    save();
}

bool ledHeartbeatEnabled() { return ledHeartbeatOn; }

void setLedHeartbeatEnabled(bool on) {
    ledHeartbeatOn = on;
    save();
}

bool webAudioAlertEnabled() { return webAudioAlertOn; }

void setWebAudioAlertEnabled(bool on) {
    webAudioAlertOn = on;
    save();
}

uint8_t screenTimeoutMinutes() { return screenTimeoutMin; }

void setScreenTimeoutMinutes(uint8_t minutes) {
    if (minutes <= Config::SCREEN_TIMEOUT_MAX_MINUTES) {
        screenTimeoutMin = minutes;
        save();
    }
}

uint16_t menuIdleTimeoutSeconds() { return menuIdleTimeoutSec; }

void setMenuIdleTimeoutSeconds(uint16_t seconds) {
    if (seconds == 0 || (seconds >= Config::MENU_IDLE_TIMEOUT_MIN_SECONDS && seconds <= Config::MENU_IDLE_TIMEOUT_MAX_SECONDS)) {
        menuIdleTimeoutSec = seconds;
        save();
    }
}

// Convenience-Helfer fuer alle Timeout-Check-Stellen im Projekt (ersetzt
// die frueher dort direkt verwendete Konstante Config::MENU_IDLE_TIMEOUT_MS)
// - rechnet den eingestellten Sekundenwert in Millisekunden um und behandelt
// 0 ("Nie") als Sonderfall: liefert dafuer den groesstmoeglichen uint32_t-
// Wert, damit ein "TouchInput::msSinceLastTap() >= menuIdleTimeoutMs()"-
// Vergleich praktisch nie auslöst (msSinceLastTap() muesste dafuer laenger
// als ca. 49 Tage seit dem letzten Tap vergangen sein - kein realistisches
// Szenario, das Geraet wird lange vorher neu gestartet/der Timer laeuft
// durch millis()-Ueberlauf ohnehin regelmaessig zurueck).
uint32_t menuIdleTimeoutMs() {
    if (menuIdleTimeoutSec == 0) return UINT32_MAX;
    return (uint32_t)menuIdleTimeoutSec * 1000UL;
}

bool nightDimmingEnabled() { return nightDimmingOn; }

void setNightDimmingEnabled(bool on) {
    nightDimmingOn = on;
    save();
}

bool screensaverEnabled() { return screensaverOn; }

void setScreensaverEnabled(bool on) {
    screensaverOn = on;
    save();
}

bool hideGroundVehicles() { return hideGroundVehiclesOn; }

void setHideGroundVehicles(bool on) {
    hideGroundVehiclesOn = on;
    save();
}

bool onlyHelicopters() { return onlyHelicoptersOn; }

void setOnlyHelicopters(bool on) {
    onlyHelicoptersOn = on;
    save();
}

bool onlyLowAltitude() { return onlyLowAltitudeOn; }

void setOnlyLowAltitude(bool on) {
    onlyLowAltitudeOn = on;
    save();
}

bool airlineFilterShowOnlyMode() { return airlineFilterShowOnlyOn; }

void setAirlineFilterShowOnlyMode(bool showOnly) {
    airlineFilterShowOnlyOn = showOnly;
    save();
}

uint8_t language() { return languageIdx; }

void setLanguage(uint8_t lang) {
    if (lang <= 7) {
        languageIdx = lang;
        save();
    }
}

uint8_t unitsMode() { return unitsModeVal; }

void setUnitsMode(uint8_t mode) {
    if (mode <= 2) {
        unitsModeVal = mode;
        save();
    }
}

bool useIataAirportCodes() { return iataAirportCodesOn; }

void setUseIataAirportCodes(bool on) {
    iataAirportCodesOn = on;
    save();
}

uint8_t radarThemeIndex() { return radarThemeIdx; }

void setRadarThemeIndex(uint8_t idx) {
    if (idx <= 4) {
        radarThemeIdx = idx;
        save();
    }
}

bool crtPhosphorEnabled() { return crtPhosphorOn; }

void setCrtPhosphorEnabled(bool on) {
    crtPhosphorOn = on;
    save();
}

bool radarPulseEnabled() { return radarPulseOn; }

void setRadarPulseEnabled(bool on) {
    radarPulseOn = on;
    save();
}

bool classicRadarEnabled() { return classicRadarOn; }

void setClassicRadarEnabled(bool on) {
    classicRadarOn = on;
    save();
}

bool militarySquawkDetectionEnabled() { return militarySquawkDetectionOn; }

void setMilitarySquawkDetectionEnabled(bool on) {
    militarySquawkDetectionOn = on;
    save();
}

bool followMeModeEnabled() { return followMeModeOn; }

void setFollowMeModeEnabled(bool on) {
    followMeModeOn = on;
    save();
}

bool perfAutoTuningEnabled() { return perfAutoTuningOn; }

void setPerfAutoTuningEnabled(bool on) {
    perfAutoTuningOn = on;
    save();
}

bool rainEffectEnabled() { return rainEffectOn; }

void setRainEffectEnabled(bool on) {
    rainEffectOn = on;
    save();
}

bool issMarkerEnabled() { return issMarkerOn; }

void setIssMarkerEnabled(bool on) {
    issMarkerOn = on;
    save();
}

bool updateLedSignalEnabled() { return updateLedSignalOn; }

void setUpdateLedSignalEnabled(bool on) {
    updateLedSignalOn = on;
    save();
}

bool eventCornerOverlayEnabled() { return eventCornerOverlayOn; }

void setEventCornerOverlayEnabled(bool on) {
    eventCornerOverlayOn = on;
    save();
}

bool worldMapBackgroundEnabled() { return worldMapBackgroundOn; }

void setWorldMapBackgroundEnabled(bool on) {
    worldMapBackgroundOn = on;
    save();
}

bool mqttEnabled() { return mqttOn; }

void setMqttEnabled(bool on) {
    mqttOn = on;
    save();
}

String mqttBroker() { return String(mqttBrokerBuf); }

void setMqttBroker(const String& hostPort) {
    strncpy(mqttBrokerBuf, hostPort.c_str(), sizeof(mqttBrokerBuf) - 1);
    mqttBrokerBuf[sizeof(mqttBrokerBuf) - 1] = 0;
    save();
}

String mqttUsername() { return String(mqttUserBuf); }

void setMqttUsername(const String& user) {
    strncpy(mqttUserBuf, user.c_str(), sizeof(mqttUserBuf) - 1);
    mqttUserBuf[sizeof(mqttUserBuf) - 1] = 0;
    save();
}

String mqttPassword() { return String(mqttPassBuf); }

void setMqttPassword(const String& pass) {
    strncpy(mqttPassBuf, pass.c_str(), sizeof(mqttPassBuf) - 1);
    mqttPassBuf[sizeof(mqttPassBuf) - 1] = 0;
    save();
}

bool ntfyPushEnabled() { return ntfyPushOn; }

void setNtfyPushEnabled(bool on) {
    ntfyPushOn = on;
    save();
}

bool ntfyFlightStoriesEnabled() { return ntfyFlightStoriesOn; }

void setNtfyFlightStoriesEnabled(bool on) {
    ntfyFlightStoriesOn = on;
    save();
}

String ntfyPushTopic() { return String(ntfyPushTopicBuf); }

void setNtfyPushTopic(const String& topic) {
    strncpy(ntfyPushTopicBuf, topic.c_str(), sizeof(ntfyPushTopicBuf) - 1);
    ntfyPushTopicBuf[sizeof(ntfyPushTopicBuf) - 1] = 0;
    save();
}

String lastSeenVersion() { return String(lastSeenVersionBuf); }

void setLastSeenVersion(const String& version) {
    strncpy(lastSeenVersionBuf, version.c_str(), sizeof(lastSeenVersionBuf) - 1);
    lastSeenVersionBuf[sizeof(lastSeenVersionBuf) - 1] = 0;
    save();
}

bool otaJustInstalled() { return otaJustInstalledFlag; }

void setOtaJustInstalled(bool value) {
    otaJustInstalledFlag = value;
    save();
}

bool otaPendingInstall() { return otaPendingInstallFlag; }

const char* otaPendingInstallUrl() { return otaPendingInstallUrlBuf; }

void setOtaPendingInstall(const char* url) {
    otaPendingInstallFlag = true;
    strncpy(otaPendingInstallUrlBuf, url, sizeof(otaPendingInstallUrlBuf) - 1);
    otaPendingInstallUrlBuf[sizeof(otaPendingInstallUrlBuf) - 1] = 0;
    save();
}

void clearOtaPendingInstall() {
    otaPendingInstallFlag = false;
    save();
}

}