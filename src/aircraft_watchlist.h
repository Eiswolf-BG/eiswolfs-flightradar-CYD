#pragma once
#include <Arduino.h>

namespace AircraftWatchlist {
    // Erhoeht von 5 auf 12 (Alex' Auftrag, Nutzer-Vorschlag war 15-20) -
    // erst durch echtes Scrollen im zugehoerigen Screen
    // (aircraft_watchlist_screen.cpp) ueberhaupt sinnvoll moeglich, vorher
    // war 5 das Maximum, das ohne Scrollen auf den Bildschirm passte.
    // WICHTIG: 20 (wie urspruenglich angedacht) scheiterte tatsaechlich am
    // Linker - "DRAM segment data does not fit, region overflowed by 160
    // bytes" - der interne RAM (kein PSRAM vorhanden) ist bereits ohne
    // diese Erhoehung nahezu vollstaendig durch andere statische Daten
    // belegt (bereits einmal bei der SD-Sprachdatei-Untersuchung
    // aufgefallen). Der Bytebedarf PRO EINTRAG ist zwar winzig (9 Byte
    // hier), aber der insgesamt noch freie DRAM-Spielraum ist es inzwischen
    // auch - macht sich erst bemerkbar, wenn (wie hier) alle 4
    // Wachlisten gleichzeitig vergroessert werden. 12 wurde live per Build
    // gegen dieses Limit ausgetestet und passt mit spuerbarer Marge.
    constexpr uint8_t MAX_WATCHED = 12;

    void init();

    uint8_t count();
    String callsignAt(uint8_t index);

    bool addWatched(const char* callsign);
    void removeWatched(uint8_t index);

    bool isWatched(const char* callsign);
}
