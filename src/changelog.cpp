#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- Removed the separate Info screen - the version number now shows "
        "directly on the (now larger) 'Check for updates' button in System settings";

    const char* const CHANGELOG_DE =
        "- Der separate Info-Bildschirm wurde entfernt - die Versionsnummer "
        "steht jetzt direkt auf dem (jetzt größeren) 'Nach Update "
        "suchen'-Button in den Systemeinstellungen";

    const char* const CHANGELOG_FR =
        "- L'écran Info séparé a été supprimé - le numéro de version "
        "s'affiche maintenant directement sur le bouton (plus grand) "
        "« Rechercher des mises à jour » dans les réglages système";

    const char* const CHANGELOG_TR =
        "- Ayrı Bilgi ekranı kaldırıldı - sürüm numarası artık Sistem "
        "ayarlarındaki (artık daha büyük) 'Güncelleme ara' düğmesinde "
        "doğrudan gösteriliyor";

    const char* const CHANGELOG_ES =
        "- Se eliminó la pantalla de información independiente: el número "
        "de versión ahora aparece directamente en el botón (más grande) "
        "«Buscar actualizaciones» en Ajustes del sistema";

    const char* const CHANGELOG_IT =
        "- La schermata Info separata è stata rimossa: il numero di "
        "versione ora appare direttamente sul pulsante (più grande) "
        "\"Cerca aggiornamenti\" nelle impostazioni di sistema";

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
