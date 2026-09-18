#pragma once
#include <Arduino.h>

namespace Config {
    // Wird bei jedem Versions-Release von Karl aktualisiert (siehe
    // CLAUDE.md-Workflow "Standard-Workflow: Push & Release") - erscheint
    // im Info-Screen (Menue > System > Info) und muss zum jeweiligen
    // Git-Tag passen.
    constexpr const char* APP_VERSION = "6.5.5";

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

    // Speaker-Steckverbinder ("SPK") des CYD-Boards (ESP32-2432S028) - laut
    // oeffentlicher Pinout-Dokumentation und Community-Berichten (u.a. der
    // von Alex zitierte GitHub-Kommentar) an GPIO26. Rein optional: ohne
    // angeschlossenen Lautsprecher passiert schlicht nichts, siehe
    // speaker_alert.h/.cpp. Kein eigener Hardware-Zugriff meinerseits zur
    // Verifikation moeglich (gleiche Einschraenkung wie bei LDR_PIN oben) -
    // GPIO26 ist allerdings die auf allen bekannten CYD-Klonvarianten
    // konsistent dokumentierte Zuordnung (der Pin ist fest mit der
    // Bestueckung des optionalen Verstaerker-/Speaker-Footprints
    // verdrahtet, unterscheidet sich also anders als z.B. die GPS-Pins
    // nicht zwischen Board-Revisionen) - bitte trotzdem einmal gegen das
    // tatsaechliche Board pruefen, falls sich kein Ton meldet.
    constexpr uint8_t SPK_PIN = 26;

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

    // Der eingebaute GPS-Steckverbinder dieses Boards (4-poliger JST-
    // Header, beschriftet VIN/TX/RX/GND - auf manchen Board-Revisionen
    // "P1", auf Alex' Board "P5" genannt) ist fest mit GPIO1 verdrahtet
    // (die Datenleitung vom Modul - aus Sicht des ESP32 also RX) -
    // denselben Pins wie die USB-Serial-Konsole (UART0). GPIO3 (die
    // zweite Leitung des Headers) bleibt unbenutzt, das Modul sendet nur,
    // empfaengt nichts. Per Serial-Diagnose verifiziert (echte
    // $GNGGA/$GNRMC/$GNVTG-Saetze empfangen) - die urspruengliche Annahme
    // GPIO22/27 (aus oeffentlicher Pinout-Doku) war fuer dieses Board
    // falsch, siehe Git-Historie. Da GPIO1 mit der USB-Konsole geteilt
    // wird, liest LocationManager::update() periodisch in kurzen
    // Zeitfenstern (siehe dortiger Kommentar), statt einen dauerhaft
    // eigenen UART zu belegen.
    constexpr uint8_t GPS_RX_PIN = 1;
    constexpr uint8_t GPS_TX_PIN = 3;
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
    // belastet werden. Vorher 10 Minuten - auf Alex' Wunsch auf 5 Minuten
    // verkuerzt, damit z.B. Regenbeginn (Regen-Overlay auf Radar/
    // Ruhebildschirm) nicht bis zu 10 Minuten zu spaet erkannt wird.
    constexpr uint32_t WEATHER_FETCH_INTERVAL_MS = 300000; // 5 Minuten

    // ISS-Positions-Bonusfeature (siehe iss_tracker.h) - Open-Notify liefert
    // ohnehin nur eine grob gerundete Momentaufnahme, ein kuerzeres
    // Intervall als hier haette keinen praktischen Mehrwert (die ISS
    // bewegt sich vorhersagbar, ~7,66 km/s).
    constexpr uint32_t ISS_FETCH_INTERVAL_MS = 20000; // 20 Sekunden

    // Eigener, grosszuegigerer Timeout nur fuer die ISS-Abfrage, getrennt
    // von HTTP_TIMEOUT_MS (siehe Root-Cause-Diagnose im Chat, eigener
    // serieller Mitschnitt mit DNS-/TCP-Connect-Trennung): DNS loeste bei
    // JEDEM gemessenen Versuch sofort auf (~0ms), der TCP-Connect zum
    // Open-Notify-Server (kleines Hobby-Projekt auf einer einzelnen VM,
    // keine CDN-Absicherung) schwankte dagegen stark zwischen ~200ms und
    // ueber 5,9s - ALLE gemessenen Fehlschlaege trafen exakt die alte
    // 6000ms-Grenze (HTTP_TIMEOUT_MS), waren also echte Server-Timeouts
    // bei einem gelegentlich ueberlasteten Server, keine sofortigen
    // Verbindungsablehnungen und kein Netzwerk-/DNS-Problem auf Alex'
    // Seite. 12s gibt dem Server ausreichend Spielraum, ohne den NetTask-
    // Loop bei einem echten Totalausfall unnoetig lange zu blockieren -
    // das ISS-Feature ist rein dekorativ, ein paar Sekunden mehr Wartezeit
    // bei einem einzelnen langsamen Zyklus faellt nicht negativ auf.
    constexpr uint32_t ISS_HTTP_TIMEOUT_MS = 12000;

    // Aeltere Position ausblenden statt eingefroren weiter anzuzeigen
    // (Alex' Meldung: bei wiederholt fehlschlagenden Abrufen - siehe
    // iss_tracker.cpp - blieb der Marker unbegrenzt lange an der letzten
    // erfolgreich abgerufenen Stelle stehen). 2 Minuten = 6x das normale
    // 20s-Abrufintervall, toleriert also ein paar aufeinanderfolgende
    // Fehlschlaege ohne staendiges Ein-/Ausblenden, faellt bei laenger
    // anhaltenden Problemen aber zuverlaessig weg - bei ~7,66 km/s legt die
    // ISS in dieser Zeit ohnehin schon ueber 900km zurueck, die Position
    // waere laengst nicht mehr aussagekraeftig.
    constexpr uint32_t ISS_POSITION_STALE_MS = 120000; // 2 Minuten


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
    //
    // Per Schieberegler einstellbar (Menue > System > Anzeige > Menue-
    // Timeout, siehe menu_timeout_screen.cpp), gleiches Muster wie
    // SCREEN_TIMEOUT_MIN/MAX_MINUTES oben - in 30-Sekunden-Schritten, danach
    // folgt "Nie" (kein automatischer Ruecksprung) als eigene Endposition.
    // MENU_IDLE_TIMEOUT_MS bleibt als Default-Wert erhalten (siehe
    // SettingsStore::menuIdleTimeoutSeconds(), Default 120s = exakt dieser
    // bisherige feste Wert) - damit aendert sich fuer niemanden ungefragt
    // etwas, bis der neue Regler aktiv genutzt wird.
    constexpr uint16_t MENU_IDLE_TIMEOUT_MIN_SECONDS = 30;
    constexpr uint16_t MENU_IDLE_TIMEOUT_MAX_SECONDS = 300; // 5 Minuten
    constexpr uint16_t MENU_IDLE_TIMEOUT_STEP_SECONDS = 30;
    constexpr uint32_t MENU_IDLE_TIMEOUT_MS = 2UL * 60UL * 1000UL; // 2 Minuten (Default)

    constexpr float DEFAULT_PROXIMITY_ALERT_KM = 8.0f;

    constexpr float LED_ALERT_RADIUS_KM = 3.0f;
    constexpr uint32_t ALERT_RETRIGGER_COOLDOWN_MS = 30000;

    // "Intelligenter" Naeherungsalarm (SettingsStore::
    // proximityAlertSmartMode(), siehe radar_screen.cpp::
    // updateProximityAlert()) - alternative Zonen-basierte Auswertung
    // statt des einfachen LED_ALERT_RADIUS_KM-Schwellenwerts oben. Drei
    // gestaffelte Zonen, jeweils die Nachfolgestufe der vorherigen (Rot
    // liegt also automatisch auch innerhalb Orange und Gelb).
    constexpr float SMART_PROXIMITY_YELLOW_KM = 20.0f;
    constexpr float SMART_PROXIMITY_ORANGE_KM = 10.0f;
    constexpr float SMART_PROXIMITY_RED_KM    = 5.0f;

    // Naeherungs-/Entfernungs-Trend im Detail-Panel (aircraft.h::
    // DistanceTrend, aircraft_table.cpp::postFetchUpdate()) - Mindest-
    // Distanzaenderung pro Abrufzyklus (siehe FETCH_INTERVAL_MS oben,
    // aktuell 10s), unterhalb derer die Distanzaenderung als Rauschen/
    // Positionsungenauigkeit gilt statt als echte Annaeherung/Entfernung
    // ("Fliegt vorbei"). 0,2km/10s entspricht einer radialen Geschwindig-
    // keitskomponente von 72 km/h - ein rein tangential (seitlich)
    // vorbeifliegendes Flugzeug hat dort naeherungsweise 0 radiale
    // Geschwindigkeit, waehrend selbst ein langsames, sich tatsaechlich
    // annaeherndes/entfernendes Flugzeug (typische Geschwindigkeiten
    // liegen deutlich darueber) diesen Schwellenwert klar ueberschreitet.
    constexpr float DISTANCE_TREND_THRESHOLD_KM = 0.2f;

    // "Ueberflug"-CPA-Anzeige im Detail-Panel (aircraft.h::cpaRelevant/
    // cpaEtaMin, Berechnung in aircraft_table.cpp::postFetchUpdate(),
    // Standard-Navigationsformel: Zeit bis zum naechsten Punkt auf der
    // aktuellen Flugbahn = -(r*v)/(v*v), r=Position des Flugzeugs relativ
    // zum eigenen Standort, v=Geschwindigkeitsvektor aus Kurs+Groundspeed).
    // CPA_MAX_DISTANCE_KM bewusst identisch zu SMART_PROXIMITY_YELLOW_KM
    // gewaehlt (nicht direkt wiederverwendet, damit beide Werte unabhaengig
    // voneinander weiter abgestimmt werden koennen) - die Anzeige soll
    // genau dann erscheinen, wenn das Flugzeug mindestens so nah vorbei-
    // kommen wird wie die aeusserste Naeherungsalarm-Zone, sonst waere sie
    // fuer Flugbahnen, die ohnehin nie in Alarmnaehe kommen, nur Rauschen.
    // CPA_MAX_TIME_MIN begrenzt die Anzeige auf einen praktisch relevanten
    // Zeithorizont (in 30 Minuten kann sich ein Kurs laengst geaendert
    // haben - eine "ETA" darueber hinaus waere kaum noch aussagekraeftig).
    // CPA_MIN_SPEED_KT verhindert eine numerisch instabile/unsinnig hohe
    // ETA bei einem praktisch stehenden "Flugzeug" (Geschwindigkeitsvektor
    // nahe Null macht die Formel instabil, siehe (v*v)-Division) - 20kt
    // liegt deutlich unter jeder realistischen Reisegeschwindigkeit, aber
    // ueber typischem Boden-/Schwebeflug-Rauschen.
    constexpr float CPA_MAX_DISTANCE_KM = 20.0f;
    constexpr float CPA_MAX_TIME_MIN    = 30.0f;
    constexpr float CPA_MIN_SPEED_KT    = 20.0f;

    // Circle-Crossing-Puls (radar_screen.cpp, siehe Aircraft::
    // ringCrossedAtMs) - wie lange der visuelle Puls-Ring nach einem
    // tatsaechlichen Ring-Durchgang sichtbar bleibt.
    constexpr uint32_t RING_CROSS_PULSE_MS = 1500;

    // Best-Effort-Anflug-Erkennung auf den naechstgelegenen Flughafen
    // (aircraft_table.cpp::postFetchUpdate(), Anzeige in radar_screen.cpp::
    // drawDetailPanel()) - rein geometrisch aus Live-Daten abgeleitet, KEIN
    // Routen-Lookup. Alle vier Kriterien (sinkende Distanz zum Flughafen
    // ueber die letzten Zyklen, Distanz unter APPROACH_MAX_DISTANCE_KM,
    // Sinkflug, Geschwindigkeit unter APPROACH_MAX_SPEED_KT, Hoehe unter
    // APPROACH_MAX_ALT_FT) muessen gleichzeitig erfuellt sein.
    constexpr float APPROACH_MAX_DISTANCE_KM = 50.0f;
    constexpr float APPROACH_MAX_SPEED_KT    = 250.0f;
    constexpr int32_t APPROACH_MAX_ALT_FT    = 10000;
    // ETA (Distanz/Geschwindigkeit) wird nur angezeigt, wenn sie in diesem
    // Bereich liegt - bei sehr niedriger Geschwindigkeit waere die
    // rechnerische ETA unsinnig hoch, bei extrem kurzer Distanz/hoher
    // Geschwindigkeit unsinnig niedrig (z.B. 0min).
    constexpr uint16_t APPROACH_ETA_MIN_PLAUSIBLE_MIN = 1;
    constexpr uint16_t APPROACH_ETA_MAX_PLAUSIBLE_MIN = 60;

    // Flugphasen-Erkennung fuers Detail-Panel (radar_screen.cpp::
    // computeFlightPhase(), Alex' Wunsch) - rein aus bereits vorhandenen
    // Werten (Hoehe, Vertikalrate, Anflug-Erkennung, Erstsichtungszeit)
    // abgeleitet. PHASE_LOW_ALT_FT ist deutlich grosszuegiger als
    // APPROACH_MAX_ALT_FT oben (10000ft, fuer die Anflug-ERKENNUNG selbst
    // gedacht) - hier geht es um "wirklich bodennah" fuer
    // Start/Landung/Tiefflug, ohne Bezug zur tatsaechlichen Flugplatzhoehe
    // (nur barometrische Hoehe ueber Meeresspiegel verfuegbar), daher
    // bewusst grob gewaehlt statt praezise.
    constexpr int32_t PHASE_LOW_ALT_FT = 3000;
    // "Kuerzlich aufgetaucht" fuer die TAKEOFF-Erkennung (a.firstSeenMs,
    // session-lokal) - ein frisch am Boden gestartetes Flugzeug sendet
    // praktisch sofort ADS-B-Daten, 2 Minuten seit Erstsichtung sind daher
    // grosszuegig genug fuer den Start selbst plus die ersten Sekunden im
    // initialen Steigflug.
    constexpr uint32_t PHASE_TAKEOFF_RECENT_MS = 120000;

    // Offline-/Stale-Data-Modus (radar_screen.cpp) - wenn der ADS-B-Abruf
    // laenger als dieser Schwellenwert nicht mehr erfolgreich war
    // (AircraftTable::msSinceLastSuccessfulFetch()), gilt das Geraet als
    // "offline": die zuletzt bekannten Flugzeuge werden ausgegraut mit
    // "zuletzt gesehen vor Xs" weitergezeigt statt sofort zu verschwinden,
    // und der Status-Hinweis im Radar-Infobereich wechselt entsprechend.
    // Bewusst deutlich ueber FETCH_RETRY_DELAY_MS (18s) - ein einzelner
    // fehlgeschlagener Abrufversuch loest noch KEINEN Offline-Hinweis aus
    // (der naechste planmaessige Versuch koennte ja schon wieder klappen),
    // erst wenn auch dieser nicht durchkommt, ist die Verbindung
    // erkennbar laenger gestoert.
    constexpr uint32_t STALE_DATA_OFFLINE_THRESHOLD_MS = 30000;
    // Danach wird ein einzelnes Flugzeug endgueltig aus der Radaranzeige
    // entfernt (Aircraft::lastSeenMs, derselbe "letzten bekannten Wert
    // merken"-Zeitstempel, den auch die Anflug-Erkennung/der intelligente
    // Naeherungsalarm bereits nutzen) - auch wenn die Verbindung bis dahin
    // noch nicht wiederhergestellt ist. Alex' Vorschlag war 60-90s, hier
    // die Mitte gewaehlt; deutlich ueber dem theoretischen Maximalalter
    // eines Flugzeugs waehrend GANZ NORMALEN Betriebs (bis knapp unter
    // 2x STALE_TIMEOUT_MS in aircraft_table.cpp, also ~40s, bevor die
    // bestehende Eviction dort ohnehin greift) - dieser Schwellenwert
    // kommt in der Praxis also nur bei einem echten laengeren
    // Verbindungsausfall ueberhaupt zum Tragen.
    constexpr uint32_t STALE_DATA_REMOVAL_MS = 75000;
    // Dauer, die eine ausgeloeste Zonen-Eskalation auf der LED sichtbar
    // bleibt, bevor sie von selbst wieder erlischt (sofern keine neue,
    // mindestens gleich schwere Eskalation nachkommt) - deutlich laenger
    // als ein einzelner ADS-B-Abrufzyklus (8s), damit der Alarm sicher
    // wahrgenommen wird, aber kein dauerhafter Zustand wie beim einfachen
    // Alarm.
    constexpr uint32_t SMART_PROXIMITY_BURST_MS = 6000;

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