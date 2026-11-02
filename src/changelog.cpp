#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: Optional toggle to hide aircraft on the ground "
        "(altitude 0) or at 0 knots ground speed - disabled by default, "
        "especially useful near airports against cluttered displays. "
        "Find it under Menu -> Flight Options -> Display Filters.";

    const char* const CHANGELOG_DE =
        "- Neu: Optionaler Schalter zum Ausblenden von Flugzeugen am "
        "Boden (Höhe 0) oder mit 0 Knoten Geschwindigkeit - "
        "standardmäßig deaktiviert, hilfreich vor allem in "
        "Flughafennähe gegen überfüllte Anzeigen. Zu finden unter "
        "Menü -> Flugoptionen -> Anzeigefilter.";

    const char* const CHANGELOG_FR =
        "- Nouveau : Interrupteur optionnel pour masquer les avions au "
        "sol (altitude 0) ou à une vitesse de 0 nœud - désactivé par "
        "défaut, particulièrement utile à proximité des aéroports "
        "contre un affichage surchargé. À trouver dans Menu -> Options "
        "de vol -> Filtres d'affichage.";

    const char* const CHANGELOG_TR =
        "- Yeni: Yerdeki (irtifa 0) veya 0 knot hızdaki uçakları "
        "gizlemek için isteğe bağlı bir anahtar - varsayılan olarak "
        "kapalı, özellikle havalimanı yakınında dolup taşan ekranlara "
        "karşı faydalı. Menü -> Uçuş Seçenekleri -> Görüntü "
        "Filtreleri altında bulunur.";

    const char* const CHANGELOG_ES =
        "- Novedad: Interruptor opcional para ocultar aeronaves en "
        "tierra (altitud 0) o a 0 nudos de velocidad - desactivado de "
        "forma predeterminada, especialmente útil cerca de aeropuertos "
        "contra pantallas sobrecargadas. Se encuentra en Menú -> "
        "Opciones de vuelo -> Filtros de visualización.";

    const char* const CHANGELOG_IT =
        "- Novità: Interruttore opzionale per nascondere gli aerei a "
        "terra (altitudine 0) o a velocità 0 nodi - disattivato per "
        "impostazione predefinita, utile soprattutto vicino agli "
        "aeroporti contro schermate affollate. Si trova in Menu -> "
        "Opzioni di volo -> Filtri di visualizzazione.";

    const char* const CHANGELOG_PT =
        "- Novo: Interruptor opcional para ocultar aeronaves no solo "
        "(altitude 0) ou a 0 nós de velocidade - desativado por padrão, "
        "útil principalmente perto de aeroportos contra telas "
        "sobrecarregadas. Encontra-se em Menu -> Opções de voo -> "
        "Filtros de exibição.";

    const char* const CHANGELOG_NL =
        "- Nieuw: Optionele schakelaar om vliegtuigen aan de grond "
        "(hoogte 0) of met 0 knopen snelheid te verbergen - standaard "
        "uitgeschakeld, vooral handig in de buurt van luchthavens tegen "
        "overvolle weergaves. Te vinden onder Menu -> Vluchtopties -> "
        "Weergavefilters.";

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
