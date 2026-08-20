#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: the clock in the radar screen header is now tappable "
        "and jumps straight to the screen-timeout settings - the "
        "entire header is now interactive";

    const char* const CHANGELOG_DE =
        "- Neu: Die Uhr in der Kopfzeile des Radarscreens ist jetzt "
        "antippbar und springt direkt zu den Bildschirm-Timeout-"
        "Einstellungen - damit ist die komplette Kopfzeile jetzt "
        "interaktiv";

    const char* const CHANGELOG_FR =
        "- Nouveau : l'horloge dans l'en-tête de l'écran radar est "
        "désormais tactile et ouvre directement les réglages du délai "
        "d'extinction de l'écran - tout l'en-tête est maintenant "
        "interactif";

    const char* const CHANGELOG_TR =
        "- Yeni: Radar ekranı başlığındaki saat artık dokunulabilir "
        "ve doğrudan ekran zaman aşımı ayarlarını açıyor - böylece "
        "tüm başlık artık etkileşimli";

    const char* const CHANGELOG_ES =
        "- Novedad: el reloj en la cabecera de la pantalla del radar "
        "ahora se puede tocar y abre directamente los ajustes de "
        "tiempo de espera de la pantalla - toda la cabecera es ahora "
        "interactiva";

    const char* const CHANGELOG_IT =
        "- Novità: l'orologio nell'intestazione della schermata radar "
        "ora è toccabile e apre direttamente le impostazioni del "
        "timeout dello schermo - l'intera intestazione è ora "
        "interattiva";

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
