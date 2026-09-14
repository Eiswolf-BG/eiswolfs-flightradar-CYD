#pragma once
#include <Arduino.h>

// "Logo"-Bild fuer den GitHub-QR-Screen (main.cpp::runGithubQrScreen(),
// erreichbar ueber Menue > System > Werkzeuge > Ueber - der fruehere Zugriff
// per Antippen von "Eiswolfs FR" im Header ist seit dem Einbau des Mode-
// Buttons dort aus Platzgruenden entfallen) - Alex' Avatar-Bild,
// aus einem hochgeladenen Foto erzeugt (Graustufen, auf 240x240px skaliert,
// RGB565) - laeuft ueber die volle Bildschirmbreite, direkt bis knapp ueber
// den Zurueck-Button (Alex' Wunsch: "Foto in voller Breite direkt auf den
// Zurueckbutton, 5px darueber").
//
// RLE-komprimiert statt als rohes 115.200-Byte-Pixel-Array eingebettet
// (Alex' Wunsch, Flash-Speicher sparen - siehe Aufraeum-Check-Bericht):
// 64,2% der Pixel sind reines Schwarz, eine einfache Lauflaengenkodierung
// greift hier gut. Format: fortlaufende 3-Byte-Eintraege
// (1 Byte Wiederholungszahl 1-255, 2 Byte RGB565-Pixelwert little-endian),
// als durchgehender Strom ueber alle 240x240 Pixel in Zeilen-Reihenfolge
// (ein einzelner Lauf KANN also ueber eine Zeilengrenze hinausreichen -
// die Entpack-Routine in main.cpp behandelt das transparent, indem sie
// den Strom einfach fortlaufend liest, ohne auf Zeilengrenzen zu achten).
// Automatisch generiert (aus dem vorherigen, unkomprimierten Array) -
// NICHT von Hand editieren, bei Bedarf per Skript aus einem neuen Foto
// neu erzeugen.
constexpr int16_t GITHUB_SCREEN_LOGO_W = 240;
constexpr int16_t GITHUB_SCREEN_LOGO_H = 240;

// Nur noch extern deklariert (Flash-Spar-Fix, siehe Aufraeum-Check-Bericht):
// main.cpp UND menu_screen.cpp binden diesen Header ein - eine Definition
// direkt hier (ohne extern) gab jeder Uebersetzungseinheit ihre eigene
// private 41.262-Byte-Kopie des Arrays (2x im fertigen Binary statt 1x).
// Die tatsaechliche Definition liegt jetzt in genau einer Datei,
// github_screen_logo_image.cpp.
extern const uint8_t GITHUB_SCREEN_LOGO_RLE[];
extern const size_t GITHUB_SCREEN_LOGO_RLE_LEN;
