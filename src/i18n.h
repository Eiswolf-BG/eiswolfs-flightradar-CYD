#pragma once
#include <Arduino.h>

enum class StringId : uint16_t {
    OK = 0,
    CANCEL,
    BACK,
    BACK_ARROW,
    LOADING,
    ADD,

    ON,
    OFF,
    NEVER,

    SPLASH_SD_OK,
    SPLASH_CONNECTING_WIFI,
    SPLASH_WIFI_OK_PREFIX,
    SPLASH_WIFI_FAILED,
    SPLASH_GETTING_LOCATION,
    SPLASH_READY,

    SD_REQUIRED_LINE1,
    SD_REQUIRED_LINE2,
    SD_REQUIRED_LINE3,
    SD_REQUIRED_HINT,

    CALIB_TITLE,
    CALIB_PROMPT,
    CALIB_TOP_LEFT,
    CALIB_TOP_RIGHT,
    CALIB_BOTTOM_RIGHT,
    CALIB_BOTTOM_LEFT,
    CALIB_SAVED,

    WIFI_SCANNING,
    WIFI_NO_NETWORKS,
    WIFI_SELECT,
    WIFI_CONNECTED_BANG,
    WIFI_CONNECTION_FAILED,
    WIFI_BACK_TO_LIST_MSG,
    WIFI_PASSWORD_LABEL,
    WIFI_SPACE,
    WIFI_CONNECT,
    WIFI_BACK_TO_LIST_BTN,
    WIFI_ALREADY_3,
    WIFI_REMOVE_ONE_FIRST,
    WIFI_CONNECTING,
    WIFI_LABEL_PREFIX,

    MENU_SETTINGS,
    MENU_SETTINGS_PAGE2,
    MENU_CALIBRATE,
    MENU_DISPLAY_INVERTED,
    MENU_DISPLAY_NORMAL,
    MENU_MANAGE_WIFI,
    MENU_LOCATION_PRESETS,
    MENU_SCREEN_TIMEOUT_PREFIX,
    MENU_NIGHT_DIMMING,
    MENU_STATISTICS,
    MENU_STATS_HISTORY,
    MENU_LOGBOOK_FILES,
    MENU_MORE_SETTINGS,
    MENU_EMERGENCY_ALERT,
    MENU_PROXIMITY_LED,
    MENU_LED_HEARTBEAT,
    MENU_FLIGHT_LOGBOOK,
    MENU_HIDE_GROUND,
    MENU_AIRLINE_FILTER,
    MENU_WATCHLIST,
    MENU_WATCHLIST_ALERT,
    MENU_BACKUP,
    MENU_RESTORE,
    MENU_BACKUP_SAVED,
    MENU_BACKUP_FAILED,
    MENU_RESTORED,
    MENU_RESTORE_FAILED,
    MENU_LANGUAGE,
    MENU_UNITS,

    AIRLINE_FILTER_TITLE,
    AIRLINE_FILTER_DESC1,
    AIRLINE_FILTER_DESC2,
    AIRLINE_FILTER_ADD,
    AIRLINE_ADD_TITLE,

    WATCHLIST_TITLE,
    WATCHLIST_DESC1,
    WATCHLIST_DESC2,
    WATCHLIST_ADD,
    WATCHLIST_ADD_TITLE,
    WATCHLIST_EMPTY,
    WATCHLIST_INFO_TITLE,
    WATCHLIST_INFO_PARA1,
    WATCHLIST_INFO_PARA2,

    LOCATION_TITLE,
    LOCATION_AUTO,
    LOCATION_PRESET,
    LOCATION_PRESET_EMPTY,
    LOCATION_ADD,
    LOCATION_LAT_PROMPT,
    LOCATION_LON_PROMPT,
    LOCATION_NEAREST_AIRPORT_PREFIX,
    LOCATION_INFO_TITLE,
    LOCATION_INFO_PARA1,
    LOCATION_INFO_PARA2,
    LOCATION_INFO_PARA3,
    LOCATION_INFO_PARA4,

    STATS_TITLE,
    STATS_TODAY,
    STATS_ALLTIME,
    STATS_DAYS,
    STATS_AVG,
    STATS_RESET_BTN,
    STATS_RESET_CONFIRM,
    STATS_RESET_DONE,
    STATS_UPTIME_PREFIX,
    STATS_TOP_ALTITUDE_PREFIX,
    STATS_HISTORY_TITLE,
    STATS_HISTORY_EMPTY,
    STATS_HISTORY_INFO_TITLE,
    STATS_HISTORY_INFO_PARA1,

    LOGFILES_TITLE,
    LOGFILES_EMPTY,
    LOGFILES_SHOWING_PREFIX,
    LOGFILES_OF,
    LOGFILES_DAYS_SUFFIX,
    LOGFILES_AIRCRAFT_SUFFIX,
    LOGFILES_INFO_TITLE,
    LOGFILES_INFO_PARA1,
    LOGFILES_INFO_PARA2,

    RADAR_TAP_FOR_DETAILS,
    RADAR_EMPTY_SKY_PREFIX,

    // Anzahl aktuell sichtbarer Flugzeuge, der Hinweiszeile RADAR_TAP_FOR_DETAILS
    // vorangestellt (radar_screen.cpp, "kind == TapForDetails"-Zweig) - z.B.
    // "5 Flugzeuge - Für mehr Details ein Flugzeug antippen". SINGULAR ist der
    // komplette Text fuer genau 1 Flugzeug, PLURAL_SUFFIX wird an die Zahl
    // angehaengt (String(count) + PLURAL_SUFFIX) fuer 0 oder mehr als 1.
    RADAR_AIRCRAFT_COUNT_SINGULAR,
    RADAR_AIRCRAFT_COUNT_PLURAL_SUFFIX,

    DETAIL_MODEL,
    DETAIL_TYPE,
    DETAIL_LOADING_DOTS,
    DETAIL_UNKNOWN,
    DETAIL_ALT,
    DETAIL_SPEED,
    DETAIL_DIST,
    DETAIL_HDG,
    DETAIL_SEATS_EST,
    DETAIL_SEATS_UNKNOWN,
    DETAIL_TAP_CLOSE,
    DETAIL_CLIMB,
    DETAIL_DESCENT,
    DETAIL_LEVEL,
    DETAIL_SQUAWK,

    UNITS_TITLE,
    UNITS_AUTO,
    UNITS_METRIC,
    UNITS_IMPERIAL,

    LANGUAGE_TITLE,

    MENU_CATEGORY_REGION,
    MENU_CATEGORY_WIFI,
    MENU_CATEGORY_SYSTEM,
    MENU_CATEGORY_FLIGHT,

    WIFI_NETWORKS_TITLE,
    WIFI_ADD_NETWORK,
    WIFI_EMPTY_SLOT,
    WIFI_INFO_TITLE,
    WIFI_INFO_PARA1,
    WIFI_INFO_PARA2,
    WIFI_INFO_PARA3,

    MENU_AIRCRAFT_LIST,
    AIRCRAFT_LIST_TITLE,
    AIRCRAFT_LIST_EMPTY,
    AIRCRAFT_LIST_SORT_PREFIX,
    AIRCRAFT_LIST_SORT_DISTANCE,
    AIRCRAFT_LIST_SORT_ALTITUDE,
    AIRCRAFT_LIST_SORT_CALLSIGN,

    // Eigener "Info"-Screen (about_screen.cpp) entfernt - der einzige
    // wirklich relevante Inhalt dort war die Versionsnummer. Die steht
    // jetzt als erste von zwei Zeilen direkt auf dem vergroesserten "Nach
    // Update suchen"-Button im System-Menue (siehe menu_screen.cpp,
    // Page::System), der den freigewordenen Platz des alten "Info"-Buttons
    // mit uebernommen hat - Zeile 2 ist der bereits vorhandene
    // MENU_CHECK_UPDATE-Text, eine eigene neue "Antippen, um..."-Zeile war
    // nicht noetig.
    CHECK_UPDATE_VERSION_PREFIX,

    MENU_BRIGHTNESS_PREFIX,
    BRIGHTNESS_TITLE,

    // Grosse Warn-Ueberlage beim Einschalten des Flugbuchs (Menue >
    // Flugoptionen > Flugbuch), erklaert die 24h-Sicherheitsabschaltung.
    MENU_LOGBOOK_WARNING_TITLE,
    MENU_LOGBOOK_WARNING_BODY,

    // Preset-Name beim Anlegen (optional), "Presets voll"-Hinweis beim
    // Antippen der Naechster-Flughafen-Laufschrift, sowie ein zusaetzlicher
    // Info-Absatz dazu (Menue > Flugoptionen > Standort-Presets > "?").
    LOCATION_NAME_PROMPT,
    LOCATION_NAME_SKIP,
    LOCATION_PRESETS_FULL,
    LOCATION_INFO_PARA5,

    // Neuer, direkt im System-Menue erreichbarer Punkt "Logbuch/WebUI"
    // (statt nur ueber das versteckte "?" im Logbuch-Dateien-Screen) -
    // zeigt IP + Erklaerung der Weboberflaeche (Flugbuch
    // ansehen/herunterladen/loeschen). Die urspruenglich hier mit
    // enthaltene Screenshot-Verwaltung wurde wieder entfernt, siehe
    // Entfernung von SCREENSHOT_SAVED_PREFIX/SCREENSHOT_FAILED oben - das
    // Bildschirm-Auslesen (SPI-Readback) funktioniert auf diesem
    // CYD-Board hardwareseitig nicht (TFT_MISO nicht angebunden).
    MENU_LOGBOOK_WEBUI,
    WEBUI_TITLE,
    WEBUI_INFO_PARA1,
    WEBUI_INFO_PARA2,

    // Kleines Info-Fenster, das beim Antippen des Wetter-Icons im Header
    // erscheint (siehe main.cpp/showWeatherInfo()) - erklaert, dass das
    // Wetter zum aktuell aktiven Standort (bzw. aktivem Standort-Preset)
    // gehoert.
    WEATHER_INFO_TITLE,
    WEATHER_INFO_BODY,

    // Adresssuche (AddressSearchScreen) - Standort per Adresseingabe statt
    // manueller Koordinaten, per kostenlosem Nominatim-Geokodierungsdienst.
    // Erreichbar sowohl beim Ersteinrichten (nach der Sprachauswahl) als
    // auch jederzeit ueber Standort-Presets > "+".
    LOCATION_ADD_CHOICE_TITLE,
    LOCATION_ADD_BY_COORDS,
    LOCATION_ADD_BY_ADDRESS,
    ADDRESS_SEARCH_TITLE,
    ADDRESS_SEARCH_SEARCHING,
    ADDRESS_SEARCH_NO_RESULTS,
    ADDRESS_SEARCH_ERROR,
    ADDRESS_SEARCH_CONFIRM_TITLE,
    ADDRESS_SEARCH_USE_THIS,
    ADDRESS_SEARCH_TRY_AGAIN,
    ADDRESS_SEARCH_CANCEL,

    // Grosser Hinweis-Screen beim allerersten Start (nur einmalig, nach der
    // Sprachauswahl) - erklaert den Genauigkeitsvorteil eines per Adresse
    // gesetzten Standorts gegenueber der automatischen IP-Standort-
    // bestimmung, mit direktem Einstieg in die Adresssuche. Ueberspringbar.
    FIRST_RUN_LOCATION_TITLE,
    FIRST_RUN_LOCATION_BODY,
    FIRST_RUN_LOCATION_SET_BTN,
    FIRST_RUN_LOCATION_SKIP_BTN,

    // Zusaetzlicher Info-Absatz im Standort-Presets-Screen (Menue >
    // Flugoptionen > Standort-Presets > "?") - weist auf den "Per Adresse
    // suchen"-Weg beim "+"-Button hin (deutlich genauer als die
    // automatische IP-Standortbestimmung). Ergaenzt die knappe Erwaehnung
    // in LOCATION_INFO_PARA3 um denselben anschaulichen Vergleich wie im
    // Ersteinrichtungs-Screen (FIRST_RUN_LOCATION_BODY) und im README.
    LOCATION_INFO_PARA6,

    // Format-Hinweis unter dem Titel der Adresssuche (AddressSearchScreen)
    // - ein Beispiel im lokalen Format (Strasse Hausnr., PLZ Ort), da das
    // einzelne Freitextfeld sonst ohne jeden Hinweis war und Nutzer nicht
    // wussten, was/in welcher Reihenfolge sie eingeben sollen.
    ADDRESS_SEARCH_HINT,

    // Letzter Screen der Ersteinrichtung (nach der Standort-Adresssuche,
    // main.cpp) - bestaetigt den Abschluss der Einrichtung, nennt den
    // SD-Karten-Ordner, in dem alle Daten liegen, und weist darauf hin,
    // dass sich das WLAN ab dem naechsten Start automatisch verbindet.
    FIRST_RUN_COMPLETE_TITLE,
    FIRST_RUN_COMPLETE_BODY1,
    FIRST_RUN_COMPLETE_BODY2,

    // Hinweis mit Beispielnamen unter dem Titel der Namens-Tastatur
    // (location_presets_screen.cpp::runPresetNameKeypad UND
    // address_search_screen.cpp::runNameKeypad, beide spiegeln sich) -
    // ohne jeden Hinweis wussten Einsteiger nicht, was fuer ein Name hier
    // gemeint ist (Preset-Name wie "Zuhause", nicht z.B. ein Ort/Adresse).
    LOCATION_NAME_HINT,

    // Legenden-Eintrag auf dem Radar-Screen (radar_screen.cpp::drawLegend)
    // fuer die blauen Quadrat-Marker der Bodenfahrzeuge - nur sichtbar,
    // wenn "Bodenfahrzeuge ausblenden" AUS ist (Flugoptionen), da die
    // Fahrzeuge dann als eigene Legenden-Zeile unter den drei
    // Hoehen-Farbstufen erscheinen.
    LEGEND_GROUND_VEHICLE,

    // Neue "Sicherung & Reset"-Unterseite im System-Menue (fasst die
    // bisher einzeln im System-Menue stehenden Backup/Restore-Buttons
    // zusammen und ergaenzt einen dritten Punkt "Einstellungen
    // zuruecksetzen" - ein kompletter Werksreset fuer Entwickler/Tester,
    // der den gesamten Flightradar-Ordner von der SD-Karte loescht und
    // neu startet, siehe settings_backup.cpp::factoryReset()). Titel und
    // Button-Label teilen sich MENU_BACKUP_RESET, genau wie
    // MENU_CATEGORY_SYSTEM sowohl fuer den Menue-Button als auch den
    // Bildschirmtitel der System-Seite verwendet wird.
    MENU_BACKUP_RESET,
    MENU_FACTORY_RESET,
    MENU_FACTORY_RESET_WARNING_BODY,
    MENU_FACTORY_RESET_DELETING,
    MENU_FACTORY_RESET_FAILED,

    // Small, centered hint below the countdown button on the "Setup
    // complete" screen (first_run_complete_screen.cpp) - makes it visible
    // that tapping the button skips the 7-second countdown immediately
    // instead of waiting it out.
    FIRST_RUN_COMPLETE_TAP_TO_SKIP,

    // Zeilenpraefix im Radar-Detailpanel (radar_screen.cpp::drawDetailPanel)
    // fuer die Flugroute (Start- -> Zielflughafen, ICAO-Code), abgefragt
    // ueber AircraftDetails per Rufzeichen. Faellt wie MODEL/TYPE/SQUAWK auf
    // DETAIL_UNKNOWN zurueck, wenn kein Rufzeichen bekannt ist oder keine
    // Route gefunden wurde.
    DETAIL_ROUTE,

    // Hinweiszeile ueber dem QR-Code auf dem neuen QR-Unterscreen der
    // Logbuch/WebUI-Seite (webui_screen.cpp::runQrScreen) - der WEBUI_TITLE-
    // String wird fuer die Kopfzeile wiederverwendet, kein eigener Titel
    // noetig.
    WEBUI_QR_HINT,

    // Ruhebildschirm bei Inaktivitaets-Timeout (Menue > System >
    // "Ruhebildschirm", siehe SettingsStore::screensaverEnabled() und
    // main.cpp).
    MENU_SCREENSAVER,

    // OTA-Firmware-Update ueber WLAN (Menue > System > "Nach Update
    // suchen", siehe menu_screen.cpp::runOtaUpdateScreen() und
    // ota_update.h/.cpp).
    MENU_CHECK_UPDATE,
    OTA_CHECKING,
    OTA_CHECK_FAILED,
    OTA_UP_TO_DATE_PREFIX,
    OTA_UPDATE_AVAILABLE_PREFIX,
    OTA_CONFIRM_BODY,
    OTA_INSTALLING_PREFIX,
    OTA_UPDATE_FAILED,
    OTA_UPDATE_SUCCESS,

    // Erfolgs-/Fehler-Ergebnis nach Download+Flash wird jetzt als
    // dauerhafter Info-Screen mit explizitem Button angezeigt (statt
    // automatischem Neustart bzw. kurzer, automatisch verschwindender
    // Meldung) - der Nutzer muss bei einer sicherheitsrelevanten Aktion wie
    // einem Firmware-Update IMMER aktiv informiert werden und selbst
    // bestaetigen, siehe menu_screen.cpp::infoScreen().
    OTA_SUCCESS_BODY,
    OTA_RESTART_BUTTON,
    // Feste Beschriftung ueber dem Changelog-Text auf dem Erfolgs-Screen
    // (Config::CHANGELOG_LATEST, siehe changelog.h) - bewusst regulaer
    // mehrsprachig, da sich NUR dieses Label nie aendert, anders als der
    // Changelog-Inhalt selbst (siehe Kommentar in changelog.h).
    OTA_CHANGELOG_LABEL,
    OTA_FAILED_BODY,

    // Eigener Bildschirm-Timeout-Screen (Menue > System > Bildschirm-
    // Timeout, siehe timeout_screen.cpp) mit Schieberegler statt des
    // vorherigen Durchklickens per wiederholtem Antippen - der
    // Ruhebildschirm-Umschalter (vorher eigene Zeile im System-Menue,
    // MENU_SCREENSAVER) zieht mit auf diesen Screen um, da er inhaltlich
    // eng mit dem Timeout zusammenhaengt und hier genug Platz fuer eine
    // kurze Erklaerung ist.
    TIMEOUT_SCREEN_TITLE,
    TIMEOUT_SCREENSAVER_DESC,

    MENU_RADAR_THEME,
    RADAR_THEME_TITLE,
    RADAR_THEME_GREEN,
    RADAR_THEME_AMBER,
    RADAR_THEME_BLUE,

    STATS_TOP_AIRCRAFT_BTN,
    TOP_AIRCRAFT_TITLE,
    TOP_AIRCRAFT_EMPTY,
    TOP_AIRCRAFT_SIGHTINGS_SUFFIX,

    WEATHER_METAR_PREFIX,

    DETAIL_QR_HINT,

    // Sonnenauf-/untergang fuer den aktuell aktiven Standort, im Wetter-
    // Info-Fenster (main.cpp::showWeatherInfo()) unterhalb von METAR
    // angezeigt - nutzt die bereits vorhandene SunTimes::compute()-Logik
    // (bisher nur intern fuer die Nachtdimmung verwendet, siehe
    // isNightDimHours()).
    WEATHER_SUNRISE_PREFIX,
    WEATHER_SUNSET_PREFIX,
    WEATHER_POLAR_DAY,
    WEATHER_POLAR_NIGHT,

    // Kurzfristige Wettervorhersage (main.cpp::showWeatherInfo(), unterhalb
    // von METAR/Sonnenauf-untergang) - Open-Meteo liefert dafuer einen
    // einzelnen stuendlichen Datenpunkt fuer "jetzt + 3 Stunden" (siehe
    // Weather::currentForecast()). WEATHER_FORECAST_PREFIX hat die "3
    // Stunden" bereits fest eingebaut (kein Platzhalter-Mechanismus fuer
    // Zahlen in I18n vorhanden, gleiche Konvention wie bei den uebrigen
    // *_PREFIX-Strings).
    WEATHER_FORECAST_PREFIX,

    // Textuelle Namen der von Weather::Condition zugeordneten Wetterlagen -
    // bisher nur als Icon gezeichnet (main.cpp::drawWeatherIcon()), jetzt
    // zusaetzlich als Text fuer die Kurzvorhersage gebraucht.
    WEATHER_CONDITION_CLEAR,
    WEATHER_CONDITION_PARTLY_CLOUDY,
    WEATHER_CONDITION_CLOUDY,
    WEATHER_CONDITION_RAIN,
    WEATHER_CONDITION_SNOW,
    WEATHER_CONDITION_THUNDERSTORM,

    // Peilung (Kompassrichtung vom eigenen Standort zum ausgewaehlten
    // Flugzeug, NICHT zu verwechseln mit dessen eigenem Kurs/HDG) als Text
    // in der Distanz/Kurs-Zeile des Detail-Panels (radar_screen.cpp::
    // drawDetailPanel()) - ergaenzt die bereits vorhandene grafische
    // Peilungsanzeige auf dem Radar selbst (drawBearingIndicator()) um eine
    // leicht lesbare Himmelsrichtung, bevor man ueberhaupt nach draussen
    // schaut. DETAIL_BEARING_PREFIX steht vor der Gradzahl, die acht
    // COMPASS_*-Werte sind die uebliche 8-Punkte-Kompass-Abkuerzung (in
    // jeder Sprache anders, z.B. Ost="O" auf Deutsch/Franzoesisch/
    // Spanisch/Italienisch, aber "E" auf Englisch, "D" auf Tuerkisch).
    DETAIL_BEARING_PREFIX,
    COMPASS_N,
    COMPASS_NE,
    COMPASS_E,
    COMPASS_SE,
    COMPASS_S,
    COMPASS_SW,
    COMPASS_W,
    COMPASS_NW,

    // Ersetzt kurzzeitig den Text des Neustart-Buttons auf dem OTA-Erfolgs-
    // Screen (menu_screen.cpp::runOtaUpdateScreen()), SOBALD er angetippt
    // wurde - ESP.restart() braucht spuerbar einen Moment, bis das Geraet
    // tatsaechlich neu startet, und ohne diese Rueckmeldung wurde in dieser
    // Zeit oft mehrfach frustriert auf den (schon "erledigten") Button
    // getippt (Alex' Testbericht). Siehe infoScreen()'s tappedLabel-Parameter
    // in menu_screen.cpp.
    OTA_RESTARTING,

    // Ankreuzbares Kaestchen im Radar-Darstellung-Menue (radar_theme_screen.cpp,
    // SettingsStore::crtPhosphorEnabled()) - unabhaengig von den drei
    // Farbschema-Buttons, faedet niedrig fliegende Flugzeuge wie ein alter
    // Roehrenradarschirm aus, in der jeweils gewaehlten Theme-Farbe.
    RADAR_THEME_CRT,

    // Zweites ankreuzbares Kaestchen im selben Menue
    // (SettingsStore::radarPulseEnabled()) - ein auslaufender Ring vom
    // Radar-Zentrum aus bei jedem frischen ADS-B-Datenabruf.
    RADAR_PULSE_TOGGLE,

    // Schalter im Flugoptionen-Menue (Menue > Flugoptionen), neben "Boden-
    // fahrzeuge ausblenden" - filtert Radar/Flugzeugliste/Web-Livekarte auf
    // Helikopter (ADS-B-Emitter-Kategorie "A7").
    MENU_ONLY_HELICOPTERS,

    // Neue Kategorie-Untermenues, entstanden beim Aufteilen der Flugoptionen-/
    // System-Seiten in grosse Kategorie-Buttons statt langer Einzelzeilen-
    // Listen (siehe menu_screen.cpp, Page::SystemDisplay/SystemTools/
    // FlightStatsLogbook/FlightLed/FlightTools).
    MENU_CATEGORY_DISPLAY,
    MENU_CATEGORY_SYSTEM_TOOLS,
    MENU_CATEGORY_STATS_LOGBOOK,
    MENU_CATEGORY_LED,
    MENU_CATEGORY_TOOLS,

    // Siebter Info-Absatz im Standort-Presets-Screen (Menue > Flugoptionen >
    // Tools > Standort-Presets > "?"), erklaert den neuen "GPS"-Knopf neben
    // "Automatisch" sowie die Verkabelung eines physischen GPS-Moduls -
    // siehe location_presets_screen.cpp und LocationManager::setGpsEnabled().
    // Ans Ende angehaengt, gleicher Grund wie oben.
    LOCATION_INFO_PARA7,

    // Schalter fuer die 180-Grad-Bildschirmdrehung (Menue > System >
    // Anzeige, siehe menu_screen.cpp::Page::SystemDisplay und
    // SettingsStore::displayRotated180()) - hilft bei Tischmontage, wo das
    // TFT-Panel von oben betrachtet sonst ausgewaschen wirkt (GitHub-
    // Meldung eines Nutzers). Text wird wie bei MENU_HIDE_GROUND mit
    // onOff() kombiniert, daher ohne AN/AUS im String selbst. Ans Ende
    // angehaengt, gleicher Grund wie oben.
    MENU_DISPLAY_ROTATE,

    // Zeile im Einheiten-Screen (Menue > Land/Region > Einheiten), schaltet
    // die Flughafencodes in der Routenanzeige des Detail-Panels zwischen
    // ICAO und IATA um - siehe SettingsStore::useIataAirportCodes() und
    // radar_screen.cpp. "ICAO"/"IATA" selbst sind international einheitliche
    // Abkuerzungen und werden NICHT uebersetzt, nur dieser Praefix.
    UNITS_AIRPORT_CODE_FORMAT,

    // Zeile in Menue > Flugoptionen > Tools (gleiche Stelle wie
    // MENU_ONLY_HELICOPTERS), schaltet den Filter "nur niedrig fliegende
    // Flugzeuge" (< Config::COLOR_LOW_ALT_THRESHOLD_FT, dieselbe Schwelle
    // wie die gruene Hoehen-Einfaerbung) um - siehe
    // SettingsStore::onlyLowAltitude() und radar_screen.cpp. Text wird wie
    // bei MENU_ONLY_HELICOPTERS mit onOff() kombiniert, daher ohne AN/AUS
    // im String selbst.
    MENU_ONLY_LOW_ALTITUDE,

    // Filter-Hinweis in der unteren Info-Zeile des Radarscreens: erscheint
    // IMMER (unabhaengig von aircraftCount/"Leerer Himmel", haengt sich an
    // die jeweils aktive Info-Zeile an - "Leerer Himmel" ODER "X Flugzeuge
    // - ..."), sobald mindestens ein Filter (Nur Helikopter/Nur
    // Niedrigflieger/Airline-Filter) aktiv ist - macht sichtbar, dass
    // Flugzeuge unsichtbar bleiben KOENNTEN, auch wenn gerade welche
    // sichtbar sind (Alex' Meldung: vorher verschwand der Hinweis, sobald
    // trotz aktivem Filter zufaellig ein Flugzeug sichtbar war). Das Wort
    // RADAR_FILTER_HINT_WORD ("Hinweis") wird ROT hervorgehoben, der Rest
    // (RADAR_FILTER_ACTIVE_PREFIX + Filternamen) in normaler Farbe -
    // radar_screen.cpp::drawInfoMarquee() zeichnet dafuer die Zeile in
    // farbigen Abschnitten statt als ein einzelner tft.print()-Aufruf.
    RADAR_FILTER_HINT_WORD,
    RADAR_FILTER_ACTIVE_PREFIX,
    RADAR_FILTER_NAME_HELICOPTERS,
    RADAR_FILTER_NAME_LOW_ALTITUDE,
    RADAR_FILTER_NAME_GROUND_HIDDEN,
    RADAR_FILTER_NAME_AIRLINE,

    // Menue-Umbau (Flugoptionen-Kategorie-Seite, siehe menu_screen.cpp):
    // "Werkzeuge" (MENU_CATEGORY_TOOLS) enthaelt jetzt nur noch Standort-
    // Presets und Beobachtungsalarm - Flugzeugliste/Beobachtungsliste
    // wanderten in ein neues "Listen"-Untermenue, die reinen
    // Sichtbarkeitsfilter (Airline/Bodenfahrzeuge/Helikopter/Niedrigflieger)
    // in ein neues "Anzeigefilter"-Untermenue. Beide neuen Kategorie-Titel:
    MENU_CATEGORY_LISTS,
    MENU_CATEGORY_FILTERS,

    // Zeigt sich im OTA-Screen (menu_screen.cpp::runOtaUpdateScreen()),
    // wenn NetTask::pause() innerhalb ihres Timeouts nicht in einen sicher
    // pausierbaren Zustand kommt (siehe net_task.h/.cpp) - verhindert, dass
    // der OTA-Vorgang trotzdem startet und bei 0% haengen bleibt, weil eine
    // ADS-B-Anfrage noch aktiv war/ist.
    OTA_NETWORK_BUSY,

    // Squawk-Wachliste (siehe squawk_watchlist.h/.cpp,
    // squawk_watchlist_screen.cpp) - wie die Rufzeichen-Beobachtungsliste
    // (AircraftWatchlist), aber fuer benutzerdefinierte Squawk-Codes statt
    // Rufzeichen. Loest denselben Mode::WatchlistBlue-Alarm aus wie ein
    // Beobachtungslisten-Treffer (siehe radar_screen.cpp::
    // updateProximityAlert()) - kein eigener Alarm-Modus, da inhaltlich
    // dieselbe Bedeutung ("ein Flugzeug, das mich interessiert, ist da").
    // Notfall-Squawks (7500/7600/7700) bleiben unveraendert dem separaten,
    // fest codierten EmergencyRed-Mechanismus vorbehalten (siehe
    // isEmergencySquawk() in radar_screen.cpp) - diese Liste ist ein
    // zusaetzlicher, rein benutzerdefinierter Mechanismus, kein Ersatz.
    MENU_SQUAWK_WATCHLIST,
    SQUAWK_WATCH_TITLE,
    SQUAWK_WATCH_DESC1,
    SQUAWK_WATCH_DESC2,
    SQUAWK_WATCH_ADD,
    SQUAWK_WATCH_ADD_TITLE,
    SQUAWK_WATCH_EMPTY,
    SQUAWK_WATCH_INFO_TITLE,
    SQUAWK_WATCH_INFO_PARA1,
    SQUAWK_WATCH_INFO_PARA2,

    // ISS-Marker-Bonusfeature (siehe iss_tracker.h, SettingsStore::
    // issMarkerEnabled()) - Schalter lebt in Menue > Flugoptionen >
    // Anzeigefilter (Page::FlightFilters in menu_screen.cpp), zusammen mit
    // einem neuen "?"-Info-Button auf derselben Seite (die vorher keinen
    // hatte), der genau diesen kurzen Erklaertext zeigt.
    MENU_ISS_MARKER,
    ISS_MARKER_INFO_TITLE,
    ISS_MARKER_INFO_BODY,

    // Terminal-Stil-Boot-Sequenz, immer aktiv (kein Schalter), spielt vor
    // dem eigentlichen Splash-Screen ab (siehe SplashScreen::
    // playBootSequence() in splash_screen.cpp) - rein kosmetischer
    // Flavour-Text im Stil eines alten Radarsystems, keine echten
    // Statusmeldungen (die bestehenden SPLASH_*-Strings oben bleiben
    // unveraendert die tatsaechlichen Boot-Status-Anzeigen).
    BOOT_SEQ_1,
    BOOT_SEQ_2,
    BOOT_SEQ_3,
    BOOT_SEQ_4,
    BOOT_SEQ_5,
    BOOT_SEQ_6,

    COUNT
};

namespace I18n {
    constexpr uint8_t LANG_COUNT = 6;
    const char* t(StringId id);
    const char* languageName(uint8_t index);
}