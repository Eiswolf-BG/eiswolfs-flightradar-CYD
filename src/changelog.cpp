#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- Fix: button text no longer overflows when a translation (or a "
        "dynamic status text) is longer than expected - it now wraps to a "
        "second line automatically";

    const char* const CHANGELOG_DE =
        "- Fix: Button-Text laeuft nicht mehr ueber, wenn eine "
        "Uebersetzung (oder ein dynamischer Statustext) laenger als "
        "erwartet ist - er bricht jetzt automatisch in eine zweite Zeile "
        "um";

    const char* const CHANGELOG_FR =
        "- Correction : le texte des boutons ne déborde plus lorsqu'une "
        "traduction (ou un texte d'état dynamique) est plus long que "
        "prévu - il passe désormais automatiquement à la ligne suivante";

    const char* const CHANGELOG_TR =
        "- Düzeltme: Bir çeviri (veya dinamik bir durum metni) "
        "beklenenden uzun olduğunda düğme metni artık taşmıyor - metin "
        "artık otomatik olarak ikinci satıra geçiyor";

    const char* const CHANGELOG_ES =
        "- Corrección: el texto de los botones ya no se desborda cuando "
        "una traducción (o un texto de estado dinámico) es más largo de "
        "lo esperado - ahora pasa automáticamente a una segunda línea";

    const char* const CHANGELOG_IT =
        "- Correzione: il testo dei pulsanti non trabocca più quando una "
        "traduzione (o un testo di stato dinamico) è più lungo del "
        "previsto - ora va automaticamente a capo su una seconda riga";

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
