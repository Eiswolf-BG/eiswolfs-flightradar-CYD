#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "aircraft.h"

namespace RadarScreen {
    void render(TFT_eSPI& tft, int16_t top);
    void tick(TFT_eSPI& tft, int16_t top, uint32_t deltaMs);
    bool handleTap(TFT_eSPI& tft, int16_t x, int16_t y, int16_t top);
    void updateProximityAlert(uint32_t nowMs);

    // Aktuelle Radar-Grundfarbe (Menue > System > Radar-Farbschema) - fuer
    // main.cpp, damit der persistente "Menu"-Header-Button dem gewaehlten
    // Farbschema folgt. Reiner Wrapper um UiTheme::accentColor() (siehe
    // ui_theme.h) - dort liegt mittlerweile die eigentliche, projektweit
    // genutzte Zuordnung, dieser hier bleibt fuer main.cpp als bestehender
    // Aufrufer erhalten.
    uint16_t themeColor(TFT_eSPI& gfx);

    // Erzwingt beim naechsten render()-Aufruf einen kompletten Neuaufbau des
    // Detail-Panels (voller Hintergrund + alle Zeilen), statt nur die
    // Zeilen mit geaendertem Text neu zu zeichnen - main.cpp ruft das nach
    // jedem Screen auf, der zwischenzeitlich den kompletten Bildschirm
    // ueberschrieben hat (Menue, Wetter-Info), waehrend noch ein Flugzeug
    // ausgewaehlt war. Sonst blieben Reste des anderen Screens (z.B.
    // Menuepunkte) sichtbar stehen, weil render() faelschlich annahm, das
    // Panel sei unveraendert noch da.
    void invalidatePanel();

    // True (und setzt sich dabei zurueck), wenn seit dem letzten Abfragen
    // ein Vollbild-Overlay INNERHALB von handleTap() (aktuell nur der
    // Flug-QR-Code-Screen, siehe qrButtonRect()/runFlightQrScreen() in
    // radar_screen.cpp) den kompletten Bildschirm inkl. Kopfzeile
    // ueberschrieben hat. Menue und Wetter-Info werden direkt von main.cpp
    // aus aufgerufen und kuemmern sich dort selbst um drawHeader()/
    // updateStatusLine() danach - der QR-Screen haengt dagegen tief in
    // handleTap() drin (der Button liegt im Detail-Panel, nicht in der
    // Kopfzeile), deshalb dieser Rueckkanal: main.cpp::loop() fragt das nach
    // jedem handleTap()-Aufruf ab und holt die Kopfzeile bei Bedarf nach.
    bool consumeHeaderRedrawFlag();

    // Waehlt ein Flugzeug programmgesteuert aus (z.B. von der Flugzeugliste
    // aus, nicht per Antippen auf dem Radar) - damit beim naechsten render()
    // sofort das Detail-Panel fuer dieses Flugzeug erscheint, so als haette
    // man es direkt im Radar angetippt.
    void selectAircraft(const char* hex, const char* callsign);

    struct EmergencyInfo {
        bool active = false;
        char callsign[9] = {0};
        char squawk[5] = {0};
    };

    EmergencyInfo checkEmergency();

    // Oeffentliche Huellen um die intern (anonymer Namespace in
    // radar_screen.cpp) bereits bestehende Typ-/Kategorie-Klassifikation -
    // fuer das Live Traffic Dashboard (live_traffic_screen.cpp), damit
    // dort NICHT dieselbe Praefix-Tabelle/Logik dupliziert werden muss.
    // Gleiche 4 Kategorien wie die interne TypeSilhouette, nur unter
    // eigenem, oeffentlichem Namen.
    enum class AircraftCategory : uint8_t { Unknown, Airliner, PrivateJet, Turboprop };
    AircraftCategory classifyAircraftType(const char* typeCode);
    bool isHeavyAircraftCategory(const char* category);
    bool isRotorcraftCategory(const char* category);

    // Weitere oeffentliche Huellen, gleiches Prinzip wie die drei oben -
    // fuer Feature 14 (MQTT/Home Assistant erweitern, siehe net_task.cpp),
    // damit die dortige Aggregation Militaer-/Notfall-Erkennung nutzen
    // kann, OHNE die Squawk-Bereichspruefung/-Liste ein zweites Mal zu
    // pflegen.
    bool isEmergencySquawkCode(const char* squawk);
    bool isMilitaryGovSquawkCode(const char* squawk);

    // Oeffentliche Huelle um isAircraftVisibleOnRadar()+AirlineFilter::
    // isHidden() (beide intern im anonymen Namespace) - fuer
    // route_watchlist.cpp::pollBackground() (Core 0/NetTask), damit der
    // Route-Watchlist-Hintergrund-Lookup NUR fuer Flugzeuge ausgeloest wird,
    // die auch tatsaechlich auf dem Radar sichtbar waeren (aktuelle
    // Reichweite + Anzeigefilter) - exakt dieselbe Definition wie beim
    // Flight-Stories-Feature (siehe dortiger Kommentar in radar_screen.cpp),
    // statt die Sichtbarkeits-Logik ein zweites Mal nachzubauen.
    bool isAircraftCurrentlyVisible(const Aircraft& a);

    // Wie isAircraftCurrentlyVisible() oben, aber mit EXPLIZIT uebergebener
    // Reichweite statt der aktuellen Geraete-Einstellung (SettingsStore::
    // rangeIndex()) - fuer web_export_server.cpp gebraucht: die Web-Live-
    // karte erlaubt jedem Besucher einen EIGENEN, vom Geraet unabhaengigen
    // Anzeige-Radius (Query-Parameter "range_km"), waehrend
    // isAircraftCurrentlyVisible() immer die Geraete-Reichweite annimmt -
    // fuer Radar-Screen/Flugzeugliste/Live-Traffic (dort IMMER die Geraete-
    // Reichweite gemeint) bleibt die einfachere Variante oben richtig.
    bool isAircraftVisibleAtRange(const Aircraft& a, float rangeKm);
}