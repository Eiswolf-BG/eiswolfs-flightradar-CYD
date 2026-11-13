#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Fix: Fixed a recurring restart loop. A safety time limit for "
        "the aircraft data lookup was too tight for slower WiFi "
        "connections and could trigger a restart even though everything "
        "was working normally - the limit is now more generous, plus "
        "several smaller timing improvements in the weather, location, "
        "and aircraft data lookups.";

    const char* const CHANGELOG_DE =
        "- Fix: Einen wiederkehrenden Neustart-Loop behoben. Ein "
        "Sicherheits-Zeitlimit für die Flugzeugdaten-Abfrage war bei "
        "langsameren WLAN-Verbindungen zu knapp bemessen und konnte "
        "einen Neustart auslösen, obwohl alles normal weiterlief - das "
        "Zeitlimit ist jetzt grosszügiger, dazu mehrere kleinere "
        "Verbesserungen bei der Wetter-, Standort- und "
        "Flugzeugdaten-Abfrage.";

    const char* const CHANGELOG_FR =
        "- Correction : Correction d'une boucle de redémarrage "
        "récurrente. Un délai de sécurité pour la récupération des "
        "données d'avion était trop court pour les connexions WiFi "
        "lentes et pouvait déclencher un redémarrage alors que tout "
        "fonctionnait normalement - ce délai est désormais plus "
        "généreux, avec aussi plusieurs petites améliorations pour la "
        "météo, la localisation et les données d'avion.";

    const char* const CHANGELOG_TR =
        "- Düzeltme: Tekrarlayan bir yeniden başlatma döngüsü "
        "düzeltildi. Uçak verisi sorgusu için güvenlik süre sınırı "
        "yavaş WiFi bağlantılarında çok dar kalıyordu ve her şey normal "
        "çalışırken bile yeniden başlatmayı tetikleyebiliyordu - bu "
        "süre sınırı artık daha geniş, ayrıca hava durumu, konum ve "
        "uçak verisi sorgularında birkaç küçük iyileştirme yapıldı.";

    const char* const CHANGELOG_ES =
        "- Corrección: Se corrigió un bucle de reinicio recurrente. Un "
        "límite de tiempo de seguridad para la consulta de datos de "
        "aeronaves era demasiado ajustado en conexiones WiFi lentas y "
        "podía provocar un reinicio aunque todo funcionara con "
        "normalidad - ese límite es ahora más generoso, además de "
        "varias mejoras menores en las consultas de clima, ubicación y "
        "datos de aeronaves.";

    const char* const CHANGELOG_IT =
        "- Correzione: Risolto un ciclo di riavvio ricorrente. Un "
        "limite di tempo di sicurezza per la richiesta dei dati degli "
        "aerei era troppo stretto con connessioni WiFi lente e poteva "
        "causare un riavvio anche se tutto funzionava normalmente - "
        "ora questo limite è più ampio, oltre a diversi piccoli "
        "miglioramenti nelle richieste di meteo, posizione e dati "
        "degli aerei.";

    const char* const CHANGELOG_PT =
        "- Correção: Corrigido um loop de reinicialização recorrente. "
        "Um limite de tempo de segurança para a consulta de dados de "
        "aeronaves estava apertado demais em conexões WiFi lentas e "
        "podia causar uma reinicialização mesmo com tudo funcionando "
        "normalmente - esse limite agora é mais generoso, além de "
        "várias pequenas melhorias nas consultas de clima, localização "
        "e dados de aeronaves.";

    const char* const CHANGELOG_NL =
        "- Fix: Een terugkerende herstartlus verholpen. Een "
        "veiligheidstijdslimiet voor het ophalen van vliegtuiggegevens "
        "was te krap voor tragere wifi-verbindingen en kon een herstart "
        "veroorzaken terwijl alles normaal functioneerde - deze limiet "
        "is nu ruimer, plus enkele kleinere verbeteringen bij het "
        "ophalen van weer-, locatie- en vliegtuiggegevens.";

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
