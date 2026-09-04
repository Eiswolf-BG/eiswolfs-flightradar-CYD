#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Fix: an aircraft directly above your own location (near "
        "the radar center) could briefly disappear/flicker instead "
        "of staying visible continuously";

    const char* const CHANGELOG_DE =
        "- Fix: Ein Flugzeug direkt über dem eigenen Standort (nahe "
        "der Radar-Mitte) konnte kurzzeitig verschwinden/blinken, "
        "statt durchgehend sichtbar zu bleiben";

    const char* const CHANGELOG_FR =
        "- Correction : un avion directement au-dessus de votre "
        "propre position (près du centre du radar) pouvait "
        "brièvement disparaître/clignoter au lieu de rester "
        "visible en continu";

    const char* const CHANGELOG_TR =
        "- Düzeltme: kendi konumunuzun tam üzerindeki bir uçak "
        "(radar merkezine yakın) sürekli görünür kalmak yerine "
        "kısa süreliğine kaybolabiliyor/yanıp sönebiliyordu";

    const char* const CHANGELOG_ES =
        "- Corrección: un avión directamente sobre tu propia "
        "ubicación (cerca del centro del radar) podía desaparecer/"
        "parpadear brevemente en lugar de permanecer visible de "
        "forma continua";

    const char* const CHANGELOG_IT =
        "- Correzione: un aereo direttamente sopra la propria "
        "posizione (vicino al centro del radar) poteva "
        "sparire/lampeggiare brevemente invece di rimanere visibile "
        "in modo continuo";

    const char* const CHANGELOG_PT =
        "- Correção: uma aeronave diretamente sobre sua própria "
        "localização (perto do centro do radar) podia desaparecer/"
        "piscar brevemente em vez de permanecer visível "
        "continuamente";

    const char* const CHANGELOG_NL =
        "- Fix: een vliegtuig direct boven je eigen locatie (dicht "
        "bij het radarcentrum) kon kort verdwijnen/knipperen in "
        "plaats van continu zichtbaar te blijven";

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
