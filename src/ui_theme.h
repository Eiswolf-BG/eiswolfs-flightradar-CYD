#pragma once
#include <TFT_eSPI.h>

// Zentrale, projektweite UI-Akzentfarbe (Menue > System > Radar-
// Darstellung, SettingsStore::radarThemeIndex() - Gruen/Amber/Blau).
// Urspruenglich NUR fuer den Radar-Screen selbst gedacht (Sweep-Linie,
// Panel-Rahmen, niedrigste Hoehenstufe), jetzt auf das GESAMTE Projekt
// ausgeweitet (Alex' ausdruecklicher Wunsch) - alle Menues/Buttons/Rahmen/
// Schriften folgen jetzt demselben Farbschema statt fest verdrahtetem
// TFT_GREEN.
//
// AUSDRUECKLICH AUSGENOMMEN bleiben semantische Farben, die eine eigene,
// vom UI-Thema unabhaengige Bedeutung tragen und NICHT umgefaerbt werden:
//   - Flugzeug-Hoehenfarben (Gruen <3000m/Gelb 3000-9100m/Rot >9100m,
//     siehe radar_screen.cpp::colorForAltitude()) - die niedrigste Stufe
//     ist jetzt bewusst IMMER TFT_GREEN, unabhaengig vom gewaehlten Thema
//     (vorher folgte sie dem Thema, siehe Git-Historie - auf Alex'
//     ausdruecklichen Wunsch hin zurueckgestellt, damit die drei Hoehen-
//     Farben als Gruppe immer eindeutig erkennbar bleiben).
//   - Notfall-Rot (TFT_RED), Beobachtungslisten-Cyan (TFT_CYAN), Militaer-/
//     Behoerden-Orange (TFT_ORANGE) und aehnliche Alarm-/Status-Ringe.
namespace UiTheme {
    uint16_t accentColor(TFT_eSPI& gfx);

    // Gedaempfte Variante von accentColor() (0.0 = schwarz, 1.0 =
    // unveraendert) - fuer dezente Hintergrund-/Deko-Elemente, die nicht in
    // voller Akzentfarbe leuchten sollen (z.B. das Radar-Logo, siehe
    // radar_logo.cpp).
    uint16_t accentColorDimmed(TFT_eSPI& gfx, float fraction);
}
