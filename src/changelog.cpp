#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: a \"Show helicopters only\" toggle in the flight "
        "options menu now lets you filter the radar, aircraft list, "
        "and web live map down to helicopters";

    const char* const CHANGELOG_DE =
        "- Neu: Ein \"Nur Helikopter anzeigen\"-Schalter im "
        "Flugoptionen-Menü filtert Radar, Flugzeugliste und "
        "Web-Livekarte jetzt auf Helikopter";

    const char* const CHANGELOG_FR =
        "- Nouveau : un interrupteur « Afficher uniquement les "
        "hélicoptères » dans le menu des options de vol permet "
        "désormais de filtrer le radar, la liste des avions et la "
        "carte web en direct sur les hélicoptères uniquement";

    const char* const CHANGELOG_TR =
        "- Yeni: Uçuş seçenekleri menüsündeki \"Sadece helikopterleri "
        "göster\" anahtarı artık radarı, uçak listesini ve web canlı "
        "haritasını yalnızca helikopterlere göre filtreleyebiliyor";

    const char* const CHANGELOG_ES =
        "- Novedad: un interruptor \"Mostrar solo helicópteros\" en "
        "el menú de opciones de vuelo ahora permite filtrar el radar, "
        "la lista de aeronaves y el mapa web en vivo solo a "
        "helicópteros";

    const char* const CHANGELOG_IT =
        "- Novità: un interruttore \"Mostra solo elicotteri\" nel "
        "menu delle opzioni di volo ora consente di filtrare radar, "
        "elenco aeromobili e mappa web live solo sugli elicotteri";

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
