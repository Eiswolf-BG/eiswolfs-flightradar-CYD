#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- Fix: the update-available red dot on the radar screen now "
        "appears reliably within minutes of a new release, instead of "
        "only showing once the screensaver kicked in";

    const char* const CHANGELOG_DE =
        "- Fix: Der rote Update-verfügbar-Punkt auf dem Radarscreen "
        "erscheint jetzt zuverlässig innerhalb weniger Minuten nach "
        "einem neuen Release, statt erst wenn der Ruhebildschirm "
        "ansprang";

    const char* const CHANGELOG_FR =
        "- Correction : le point rouge « mise à jour disponible » sur "
        "l'écran radar apparaît désormais de façon fiable quelques "
        "minutes après une nouvelle version, au lieu de n'apparaître "
        "qu'au démarrage de l'économiseur d'écran";

    const char* const CHANGELOG_TR =
        "- Düzeltme: Radar ekranındaki kırmızı güncelleme noktası "
        "artık yeni bir sürümden sonra dakikalar içinde güvenilir "
        "şekilde görünüyor, öncesinde yalnızca ekran koruyucu devreye "
        "girdiğinde görünüyordu";

    const char* const CHANGELOG_ES =
        "- Corrección: el punto rojo de actualización disponible en "
        "la pantalla del radar ahora aparece de forma fiable a los "
        "pocos minutos de una nueva versión, en lugar de aparecer "
        "solo cuando se activaba el salvapantallas";

    const char* const CHANGELOG_IT =
        "- Correzione: il puntino rosso di aggiornamento disponibile "
        "sulla schermata radar ora appare in modo affidabile entro "
        "pochi minuti da una nuova versione, invece di apparire solo "
        "all'avvio del salvaschermo";

    const char* const TABLE[CHANGELOG_LANG_COUNT] = {
        CHANGELOG_EN, CHANGELOG_DE, CHANGELOG_FR, CHANGELOG_TR, CHANGELOG_ES, CHANGELOG_IT
    };
}

const char* changelogLatest() {
    uint8_t lang = SettingsStore::language();
    if (lang >= CHANGELOG_LANG_COUNT) lang = 0;
    return TABLE[lang];
}

}
