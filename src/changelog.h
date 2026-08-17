#pragma once

namespace Config {
    // Kurze Aenderungsliste des AKTUELLEN Releases - wird auf dem "Update
    // installiert"-Screen nach einem erfolgreichen OTA-Update angezeigt
    // (siehe menu_screen.cpp, infoScreen()-Aufruf fuer OTA_UPDATE_SUCCESS).
    //
    // JETZT MEHRSPRACHIG (alle 6 Sprachen, wie der Rest der Geraete-UI) -
    // eine rein englische Changelog-Zeile zwischen sonst komplett
    // uebersetzten Texten sah im Test wie ein Sprachmix aus (Alex' direktes
    // Feedback zu einem Foto vom Geraet). Implementierung in changelog.cpp,
    // gleiches Auswahlprinzip wie I18n::t() (siehe i18n.cpp): Sprache kommt
    // aus SettingsStore::language(), Reihenfolge EN/DE/FR/TR/ES/IT.
    //
    // WICHTIG: Muss bei JEDEM Release (siehe CLAUDE.md "Standard-Workflow:
    // Push & Release") in ALLEN 6 SPRACHEN zusammen mit APP_VERSION in
    // config.h aktualisiert werden - sonst zeigt das Geraet nach dem
    // naechsten Update den Changelog des VORHERIGEN Releases an.
    const char* changelogLatest();
}
