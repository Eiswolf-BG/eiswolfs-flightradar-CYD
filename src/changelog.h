#pragma once

namespace Config {
    // Kurze Aenderungsliste des AKTUELLEN Releases - wird auf dem "Update
    // installiert"-Screen nach einem erfolgreichen OTA-Update angezeigt
    // (siehe menu_screen.cpp, infoScreen()-Aufruf fuer OTA_UPDATE_SUCCESS).
    //
    // WICHTIG: Muss bei JEDEM Release (siehe CLAUDE.md "Standard-Workflow:
    // Push & Release") zusammen mit APP_VERSION in config.h aktualisiert
    // werden - sonst zeigt das Geraet nach dem naechsten Update den
    // Changelog des VORHERIGEN Releases an.
    //
    // Bewusst nur EINSPRACHIG Englisch (wie README/Release-Notes), NICHT
    // ueber das i18n-System mit allen 6 Sprachen - der Text aendert sich bei
    // jedem einzelnen Release komplett neu, sechsfache Uebersetzungspflege
    // pro Release waere unverhaeltnismaessig viel Aufwand fuer einen
    // kurzlebigen Hinweistext. Die feste Bildschirm-Beschriftung darueber
    // ("Neu in dieser Version:" / "What's new in this version:" usw.) bleibt
    // regulaer mehrsprachig ueber StringId::OTA_CHANGELOG_LABEL, da sich NUR
    // dieser Label-Text nie aendert.
    //
    // Format: Zeilenumbrueche ("\n") erzeugen echte Zeilenumbrueche auf dem
    // Display (siehe layoutWrapped()-Fix in menu_screen.cpp). Kurz halten -
    // der Screen ist zwar scrollbar, aber ein Bildschirm mit 240x320px.
    constexpr const char* CHANGELOG_LATEST =
        "- Removed the separate Info screen - the version number now shows "
        "directly on the bigger 'Check for updates' button in System settings";
}
