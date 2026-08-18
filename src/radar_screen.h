#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace RadarScreen {
    void render(TFT_eSPI& tft, int16_t top);
    void tick(TFT_eSPI& tft, int16_t top, uint32_t deltaMs);
    bool handleTap(TFT_eSPI& tft, int16_t x, int16_t y, int16_t top);
    void updateProximityAlert(uint32_t nowMs);

    // Aktuelle Radar-Grundfarbe (Menue > System > Radar-Farbschema) - fuer
    // main.cpp, damit der persistente "Menu"-Header-Button dem gewaehlten
    // Farbschema folgt statt immer fest gruen zu bleiben. Bewusst nur fuer
    // diesen einen Button freigegeben, alle anderen Screens bleiben gruen.
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
}