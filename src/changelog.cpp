#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the airport reference database (used for the "
        "\"nearest airport\" display and approach detection) has "
        "been expanded from 34 hand-picked international hubs to "
        "a comprehensive, worldwide database of over 5,000 large "
        "and medium-sized airports - noticeably more reliable "
        "detection even at smaller regional airports";

    const char* const CHANGELOG_DE =
        "- Neu: Die Flughafen-Referenzliste (für die \"Nächster "
        "Flughafen\"-Anzeige und die Anflug-Erkennung) wurde von "
        "34 handverlesenen internationalen Hubs auf eine "
        "umfassende, weltweite Datenbank mit über 5.000 großen "
        "und mittelgroßen Flughäfen erweitert - deutlich "
        "zuverlässigere Erkennung auch bei kleineren "
        "Regionalflughäfen";

    const char* const CHANGELOG_FR =
        "- Nouveau : la base de données de référence des "
        "aéroports (utilisée pour l'affichage \"aéroport le plus "
        "proche\" et la détection d'approche) a été étendue de 34 "
        "grands hubs internationaux sélectionnés à une base de "
        "données mondiale complète de plus de 5 000 aéroports "
        "grands et moyens - détection nettement plus fiable, même "
        "pour les petits aéroports régionaux";

    const char* const CHANGELOG_TR =
        "- Yeni: havalimanı referans veritabanı (\"en yakın "
        "havalimanı\" gösterimi ve yaklaşma tespiti için "
        "kullanılır) elle seçilmiş 34 uluslararası merkezden, "
        "5.000'den fazla büyük ve orta ölçekli havalimanını "
        "kapsayan dünya çapında kapsamlı bir veritabanına "
        "genişletildi - küçük bölgesel havalimanlarında bile "
        "belirgin şekilde daha güvenilir tespit";

    const char* const CHANGELOG_ES =
        "- Novedad: la base de datos de referencia de aeropuertos "
        "(usada para la indicación \"aeropuerto más cercano\" y la "
        "detección de aproximación) se ha ampliado de 34 grandes "
        "aeropuertos internacionales seleccionados a una base de "
        "datos mundial completa con más de 5.000 aeropuertos "
        "grandes y medianos - detección notablemente más fiable "
        "incluso en aeropuertos regionales más pequeños";

    const char* const CHANGELOG_IT =
        "- Novità: il database di riferimento degli aeroporti "
        "(usato per l'indicazione \"aeroporto più vicino\" e il "
        "rilevamento dell'avvicinamento) è stato ampliato da 34 "
        "grandi hub internazionali selezionati a un database "
        "mondiale completo con oltre 5.000 aeroporti grandi e "
        "medi - rilevamento notevolmente più affidabile anche "
        "negli aeroporti regionali più piccoli";

    const char* const CHANGELOG_PT =
        "- Novo: o banco de dados de referência de aeroportos "
        "(usado para a exibição \"aeroporto mais próximo\" e a "
        "detecção de aproximação) foi ampliado de 34 grandes "
        "hubs internacionais selecionados para um banco de dados "
        "mundial completo com mais de 5.000 aeroportos grandes e "
        "médios - detecção visivelmente mais confiável mesmo em "
        "aeroportos regionais menores";

    const char* const CHANGELOG_NL =
        "- Nieuw: de referentiedatabase met luchthavens (gebruikt "
        "voor de weergave \"dichtstbijzijnde luchthaven\" en de "
        "naderingsdetectie) is uitgebreid van 34 handmatig "
        "geselecteerde internationale hubs naar een uitgebreide, "
        "wereldwijde database met meer dan 5.000 grote en "
        "middelgrote luchthavens - merkbaar betrouwbaardere "
        "detectie, ook bij kleinere regionale luchthavens";

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
