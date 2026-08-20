#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: the Radar Display menu now offers a CRT-Phosphor glow "
        "effect and a Radar Pulse animation as independent options, "
        "combinable with any color scheme\n"
        "- New: aircraft can now be added to the watchlist directly "
        "from the detail panel with a single tap";

    const char* const CHANGELOG_DE =
        "- Neu: Das Menü \"Radar-Darstellung\" bietet jetzt einen "
        "CRT-Phosphor-Leuchteffekt und eine Radar-Puls-Animation als "
        "unabhängige Optionen, kombinierbar mit jedem Farbschema\n"
        "- Neu: Flugzeuge lassen sich jetzt mit einem Tipp direkt aus "
        "dem Detail-Panel zur Beobachtungsliste hinzufügen";

    const char* const CHANGELOG_FR =
        "- Nouveau : le menu « Affichage radar » propose désormais un "
        "effet de rémanence CRT-Phosphore et une animation d'impulsion "
        "radar en options indépendantes, combinables avec n'importe "
        "quel thème de couleur\n"
        "- Nouveau : possibilité d'ajouter un avion à la liste de "
        "surveillance d'un simple tap directement depuis le panneau de "
        "détails";

    const char* const CHANGELOG_TR =
        "- Yeni: \"Radar Görünümü\" menüsü artık her renk temasıyla "
        "birleştirilebilen, birbirinden bağımsız bir CRT Fosfor "
        "parlama efekti ve bir Radar Nabzı animasyonu sunuyor\n"
        "- Yeni: Uçaklar artık detay panelinden tek dokunuşla doğrudan "
        "takip listesine eklenebiliyor";

    const char* const CHANGELOG_ES =
        "- Novedad: el menú \"Aspecto del radar\" ofrece ahora un "
        "efecto de resplandor Fósforo CRT y una animación de Pulso de "
        "radar como opciones independientes, combinables con cualquier "
        "esquema de color\n"
        "- Novedad: ahora se puede añadir un avión a la lista de "
        "seguimiento con un solo toque directamente desde el panel de "
        "detalles";

    const char* const CHANGELOG_IT =
        "- Novità: il menu \"Aspetto del radar\" offre ora un effetto "
        "di luminescenza Fosforo CRT e un'animazione Impulso radar "
        "come opzioni indipendenti, combinabili con qualsiasi schema "
        "di colori\n"
        "- Novità: ora è possibile aggiungere un aereo alla lista di "
        "controllo con un solo tocco direttamente dal pannello dei "
        "dettagli";

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
