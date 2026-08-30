#pragma once
#include <Arduino.h>

namespace Config {
    // Wird bei jedem Versions-Release von Karl aktualisiert (siehe
    // CLAUDE.md-Workflow "Standard-Workflow: Push & Release") - erscheint
    // im Info-Screen (Menue > System > Info) und muss zum jeweiligen
    // Git-Tag passen.
    constexpr const char* APP_VERSION = "4.7.0";

    // Display-Helligkeit (Menue > System > Helligkeit), in Prozent.
    // MIN bewusst nicht 0 - ein komplett dunkles Display koennte sonst wie
    // ein Defekt wirken statt wie eine Einstellung.
    constexpr uint8_t BRIGHTNESS_MIN_PERCENT = 10;
    constexpr uint8_t BRIGHTNESS_MAX_PERCENT = 100;
    constexpr uint8_t BRIGHTNESS_STEP_PERCENT = 10;

    // Auto-Helligkeit (Menue > System > Anzeige > Helligkeit, siehe
    // auto_brightness.h/.cpp, SettingsStore::autoBrightnessEnabled(), AUS
    // per Default) - der eingebaute Lichtsensor (LDR) des CYD-Boards
    // (ESP32-2432S028) sitzt laut oeffentlicher Pinout-Dokumentation
    // (Mischianti, RandomNerdTutorials) an GPIO34 (ADC1_CH6, eingangs-
    // only, unabhaengig von WiFi nutzbar - ADC2 waere das nicht). Kein
    // eigener Hardware-Zugriff meinerseits zur Verifikation moeglich.
    constexpr uint8_t LDR_PIN = 34;

    // ADC-Rohwertbereich (12-Bit, 0-4095), der auf BRIGHTNESS_MIN_PERCENT..
    // BRIGHTNESS_MAX_PERCENT abgebildet wird - AUSDRUECKLICH Schaetzwerte
    // ohne Kalibrierung am echten Geraet (siehe LDR_PIN-Kommentar oben).
    // Laut denselben Quellen streut der verbaute LDR-Widerstand je nach
    // Fertigungslos spuerbar - diese Werte muessen bei Bedarf am echten
    // Geraet nachjustiert werden (z.B. Serial-Logging des Rohwerts bei
    // "ganz dunkel" und "hell beleuchtet", dann hier eintragen).
    constexpr uint16_t AUTO_BRIGHTNESS_ADC_MIN = 200;
    constexpr uint16_t AUTO_BRIGHTNESS_ADC_MAX = 3200;

    // Glaettungsfaktor fuer den exponentiellen gleitenden Mittelwert des
    // LDR-Rohwerts (0.0 = friert ein, 1.0 = keine Glaettung/jeder Messwert
    // wirkt sofort voll) - klein gewaehlt, damit kurzes Abdecken/Vorbei-
    // laufen vor dem Sensor oder einzelne verrauschte Messwerte nicht
    // sofort ein sichtbares Helligkeits-Flackern ausloesen.
    constexpr float AUTO_BRIGHTNESS_SMOOTHING = 0.15f;

    // Bildschirm-Timeout (Menue > System > Bildschirm-Timeout), in Minuten,
    // per Schieberegler einstellbar (siehe timeout_screen.cpp) - danach
    // folgt "Nie" (kein Timeout) als eigene Endposition. Vorher nur per
    // wiederholtem Antippen 0-10 durchklickbar (0 = Nie), was bei z.B. 10
    // Minuten zehn einzelne Tipps brauchte.
    constexpr uint8_t SCREEN_TIMEOUT_MIN_MINUTES = 1;
    constexpr uint8_t SCREEN_TIMEOUT_MAX_MINUTES = 15;

    // Nachtmodus (22-6 Uhr) dimmt relativ zur jeweils eingestellten normalen
    // Helligkeit, nicht auf einen festen Absolutwert - sonst waere der
    // Dimm-Effekt bei niedrig eingestellter Normalhelligkeit wirkungslos
    // oder wuerde das Display nachts sogar heller machen als tagsueber.
    constexpr uint8_t NIGHT_DIM_REDUCTION_PERCENT = 40;

    // Der Ruhebildschirm (Sternenhimmel + Uhrzeit, siehe main.cpp) dimmt
    // deutlich staerker als die normale Nachtabsenkung oben - er ersetzt ja
    // den kompletten Bildschirminhalt und laeuft oft ueber laengere Zeit
    // (z.B. nachts als Deko), eine bloss leicht abgesenkte Helligkeit wirkte
    // dafuer zu hell. Ebenfalls relativ zur eingestellten Normalhelligkeit,
    // gleiches Prinzip wie NIGHT_DIM_REDUCTION_PERCENT.
    constexpr uint8_t SCREENSAVER_DIM_REDUCTION_PERCENT = 75;

    constexpr const char* IP_GEO_HOST = "ip-api.com";
    constexpr const char* IP_GEO_PATH = "/json/?fields=status,lat,lon,offset,countryCode";

    // Adresssuche (AddressSearchScreen) - kostenloser, anmeldefreier
    // Geokodierungs-Dienst (OpenStreetMap Nominatim). Deren Nutzungsregeln
    // verlangen einen aussagekraeftigen User-Agent statt des HTTPClient-
    // Standardwerts, siehe https://operations.osmfoundation.org/policies/nominatim/.
    constexpr const char* NOMINATIM_HOST = "nominatim.openstreetmap.org";
    constexpr const char* NOMINATIM_USER_AGENT = "EiswolfsFlightradarCYD (github.com/Eiswolf-BG/eiswolfs-flightradar-CYD)";

    struct GpsPinPair { uint8_t rx; uint8_t tx; const char* label; };
    constexpr GpsPinPair GPS_PIN_CANDIDATES[] = {
        {22, 27, "G22/G27"}
    };
    constexpr uint8_t GPS_PIN_CANDIDATE_COUNT = 1;
    constexpr uint32_t GPS_BAUD = 9600;

    constexpr float RANGE_STEPS_KM[] = {10.0f, 25.0f, 50.0f, 100.0f};
    constexpr uint8_t RANGE_STEP_COUNT = 4;
    constexpr uint8_t DEFAULT_RANGE_INDEX = 1;

    // adsb.lol statt adsb.fi (seit v4.0.2-Nachfolger) - adsb.fi lieferte an
    // das Geraet trotz gueltigem HTTP 200 und validem JSON konstant leere
    // Flugzeuglisten, vermutlich Cloudflare-Bot-Management/TLS-Fingerprinting
    // gegen den ESP32-Client (per curl vom selben Netzwerk kamen jederzeit
    // volle Daten). adsb.lol hat identisches URL-/JSON-Schema (nur "/v2/..."
    // statt "/api/v3/...", Feldnamen unveraendert), daher direkt austauschbar.
    constexpr const char* ADSB_API_HOST = "api.adsb.lol";
    constexpr uint16_t ADSB_API_PORT = 443;
    // TESTWEISE von 8000 auf 10000 erhoeht (siehe Absprache mit Karl) - nach
    // vereinzelten HTTP 429 von adsb.lol etwas serverfreundlicher, aber
    // bewusst nicht weiter als 10s, damit der Radar noch reaktionsschnell
    // bleibt (Trade-off dokumentiert, siehe Bericht an Alex).
    constexpr uint32_t FETCH_INTERVAL_MS = 10000;
    // TESTWEISE - Obergrenze fuers exponentielle Backoff nach HTTP 429
    // (siehe net_task.cpp), damit sich das Intervall nicht unbegrenzt
    // aufschaukelt.
    constexpr uint32_t FETCH_BACKOFF_MAX_MS = 3UL * 60UL * 1000UL;
    // TESTWEISE - moderate feste Wartezeit nach EINEM fehlgeschlagenen
    // Request (Timeout/SSL-Fehler, kein 429) vor dem naechsten Versuch, statt
    // sofort wieder im FETCH_INTERVAL_MS-Takt weiterzumachen (siehe
    // net_task.cpp) - verhindert eine Anfragen-Flut bei kurzen WLAN-
    // Aussetzern.
    constexpr uint32_t FETCH_RETRY_DELAY_MS = 18000;
    constexpr uint32_t HTTP_TIMEOUT_MS = 6000;
    // Eigener, grosszuegigerer Timeout nur fuer die ADS-B-Abfrage, getrennt
    // von HTTP_TIMEOUT_MS (das weiterhin fuer hexdb.io/Wetter/etc. gilt) -
    // die Antwort bei 100km Radius kann ueber 100KB gross werden.
    constexpr uint32_t ADSB_HTTP_TIMEOUT_MS = 15000;

    // Wetter-Icon im Header (siehe weather.cpp) - deutlich seltener
    // abgefragt als die ADS-B-Daten, das Wetter aendert sich nicht
    // minuetlich und die kostenlose Open-Meteo-API soll nicht unnoetig oft
    // belastet werden.
    constexpr uint32_t WEATHER_FETCH_INTERVAL_MS = 600000; // 10 Minuten

    // ISS-Positions-Bonusfeature (siehe iss_tracker.h) - Open-Notify liefert
    // ohnehin nur eine grob gerundete Momentaufnahme, ein kuerzeres
    // Intervall als hier haette keinen praktischen Mehrwert (die ISS
    // bewegt sich vorhersagbar, ~7,66 km/s).
    constexpr uint32_t ISS_FETCH_INTERVAL_MS = 20000; // 20 Sekunden

    // Intervall fuer die automatische Hintergrund-Pruefung auf neue
    // Firmware-Updates (siehe OtaUpdate::pollBackground(), aufgerufen aus
    // net_task.cpp) - ein neues Firmware-Release erscheint zwar hoechstens
    // alle paar Wochen, 3 Minuten sind aber bewusst trotzdem gewaehlt: so
    // zeigt sich ein frisch veroeffentlichtes Update schnell als Badge, statt
    // erst nach einer viertel Stunde. 3 Minuten = maximal 20 Anfragen/Stunde
    // an die GitHub-API, immer noch unter deren anonymem Limit von 60
    // Anfragen/Stunde - bei mehreren Geraeten an derselben Heim-IP
    // entsprechend vervielfacht, im Blick behalten, falls das Limit je
    // erreicht wird.
    constexpr uint32_t OTA_BACKGROUND_CHECK_INTERVAL_MS = 3UL * 60UL * 1000UL; // 3 Minuten

    // Inaktivitaets-Timeout INNERHALB von Vollbild-Menues/Einstellungs-
    // Screens (Menue, WLAN-Verwaltung, GitHub-QR-Screen, etc.) - jeder dieser
    // Screens haengt in seiner eigenen blockierenden Touch-Schleife und
    // haelt dadurch den normalen Bildschirm-Timeout (SCREEN_TIMEOUT_MIN/MAX_
    // MINUTES oben, main.cpp::loop()) komplett an, solange er offen bleibt -
    // das Geraet blieb sonst z.B. auf dem Tisch liegend mit voller
    // Beleuchtung im Menue haengen, ohne dass der eingestellte Timeout je
    // greift (Alex' Bugmeldung). Nach dieser Zeit ohne Tap springt der
    // jeweilige Screen automatisch zum Radarscreen zurueck, danach greift
    // der normale Timeout wieder ganz regulaer.
    constexpr uint32_t MENU_IDLE_TIMEOUT_MS = 2UL * 60UL * 1000UL; // 2 Minuten

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

    // MQTT-Schnittstelle (optional, AUS per Default, siehe mqtt_client.h/
    // mqtt_screen.cpp) - fuer Nutzer, die den Radar an ein eigenes Smart-
    // Home-System (z.B. Home Assistant) anbinden wollen. Topic-Praefix
    // bewusst als eigener Namensraum, damit auf einem geteilten Broker
    // (z.B. ein bereits fuer andere Geraete genutzter Home-Assistant-
    // Broker) keine Kollision mit anderen Themen entsteht.
    constexpr const char* MQTT_TOPIC_PREFIX = "eiswolfs-flightradar";
    constexpr uint16_t MQTT_DEFAULT_PORT = 1883;
}