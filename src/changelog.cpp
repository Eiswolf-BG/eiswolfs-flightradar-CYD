#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the info bar now shows the live GPS position (and "
        "altitude, once available) whenever an external GPS module is "
        "enabled and has a fix";

    const char* const CHANGELOG_DE =
        "- Neu: Die Infoleiste zeigt jetzt die aktuelle GPS-Position "
        "an (sobald verfuegbar auch die Hoehe), sobald ein externes "
        "GPS-Modul aktiviert ist und einen Fix hat";

    const char* const CHANGELOG_FR =
        "- Nouveau : la barre d'info affiche désormais la position GPS "
        "en direct (et l'altitude, dès qu'elle est disponible) dès "
        "qu'un module GPS externe est activé et dispose d'une position "
        "fixe";

    const char* const CHANGELOG_TR =
        "- Yeni: Harici bir GPS modülü etkinleştirildiğinde ve konum "
        "sabitlemesi (fix) olduğunda, bilgi çubuğu artık canlı GPS "
        "konumunu (ve mevcut olur olmaz irtifayı da) gösteriyor";

    const char* const CHANGELOG_ES =
        "- Novedad: la barra de información ahora muestra la posición "
        "GPS en directo (y la altitud, en cuanto esté disponible) "
        "siempre que un módulo GPS externo esté activado y tenga una "
        "posición fija";

    const char* const CHANGELOG_IT =
        "- Novità: la barra informativa ora mostra la posizione GPS in "
        "tempo reale (e l'altitudine, non appena disponibile) quando "
        "un modulo GPS esterno è attivo e ha un fix";

    const char* const CHANGELOG_PT =
        "- Novo: a barra de informações agora mostra a posição GPS ao "
        "vivo (e a altitude, assim que disponível) sempre que um "
        "módulo GPS externo estiver ativado e tiver um fix";

    const char* const CHANGELOG_NL =
        "- Nieuw: de infobalk toont nu de actuele GPS-positie (en "
        "zodra beschikbaar ook de hoogte) zodra een externe GPS-module "
        "is ingeschakeld en een fix heeft";

    const char* const TABLE[CHANGELOG_LANG_COUNT] = {
        CHANGELOG_EN, CHANGELOG_DE, CHANGELOG_FR, CHANGELOG_TR, CHANGELOG_ES, CHANGELOG_IT, CHANGELOG_PT, CHANGELOG_NL
    };
}

const char* changelogLatest() {
    uint8_t lang = SettingsStore::language();
    if (lang >= CHANGELOG_LANG_COUNT) lang = 0;
    return TABLE[lang];
}

}
