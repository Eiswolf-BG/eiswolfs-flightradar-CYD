#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: address search for location presets now shows a "
        "list to choose from when a place name is ambiguous (e.g. "
        "\"Cambridge\" exists multiple times worldwide), instead "
        "of blindly picking the first match\n"
        "- Fix: manually entered hidden WiFi networks (hidden "
        "SSID) stopped connecting automatically after a restart - "
        "now connects directly and reliably instead of waiting "
        "for a match in the normal network scan (only affects "
        "newly entered networks from this update onward; hidden "
        "networks saved before this update need to be re-entered "
        "once via \"Other/hidden SSID\")\n"
        "- Fix: address search didn't correctly respect the "
        "language setting for Portuguese and Dutch";

    const char* const CHANGELOG_DE =
        "- Neu: Die Adresssuche für Standort-Presets zeigt jetzt "
        "eine Auswahlliste, wenn ein Ortsname mehrdeutig ist "
        "(z. B. gibt es \"Cambridge\" weltweit mehrfach), statt "
        "blind den ersten Treffer zu übernehmen\n"
        "- Fix: Manuell eingegebene versteckte WLAN-Netzwerke "
        "(Hidden SSID) verbanden sich nach einem Neustart nicht "
        "mehr automatisch - verbindet sich jetzt zuverlässig "
        "direkt, statt auf einen Treffer im normalen Netzwerk-Scan "
        "zu warten (betrifft nur neu eingegebene Netzwerke ab "
        "diesem Update; bereits vorher gespeicherte versteckte "
        "Netzwerke müssen einmalig erneut über \"Andere/versteckte "
        "SSID\" eingegeben werden)\n"
        "- Fix: Die Adresssuche berücksichtigte die "
        "Spracheinstellung bei Portugiesisch und Niederländisch "
        "nicht korrekt";

    const char* const CHANGELOG_FR =
        "- Nouveau : la recherche d'adresse pour les emplacements "
        "enregistrés affiche désormais une liste de choix lorsqu'un "
        "nom de lieu est ambigu (par ex. « Cambridge » existe "
        "plusieurs fois dans le monde), au lieu de reprendre "
        "aveuglément le premier résultat\n"
        "- Correction : les réseaux WiFi cachés (SSID masqué) "
        "saisis manuellement ne se reconnectaient plus "
        "automatiquement après un redémarrage - se connecte "
        "désormais directement et de manière fiable, au lieu "
        "d'attendre une correspondance dans le scan réseau normal "
        "(concerne uniquement les réseaux nouvellement saisis à "
        "partir de cette mise à jour ; les réseaux cachés "
        "enregistrés avant doivent être ressaisis une fois via "
        "« Autre SSID/caché »)\n"
        "- Correction : la recherche d'adresse ne respectait pas "
        "correctement le paramètre de langue pour le portugais et "
        "le néerlandais";

    const char* const CHANGELOG_TR =
        "- Yeni: konum ön ayarları için adres araması, bir yer adı "
        "belirsiz olduğunda (örn. \"Cambridge\" dünyada birden "
        "fazla kez var) artık ilk sonucu körü körüne almak yerine "
        "bir seçim listesi gösteriyor\n"
        "- Düzeltme: elle girilen gizli WiFi ağları (gizli SSID) "
        "yeniden başlatmadan sonra artık otomatik olarak "
        "bağlanmıyordu - artık normal ağ taramasında bir eşleşme "
        "beklemek yerine doğrudan ve güvenilir şekilde bağlanıyor "
        "(yalnızca bu güncellemeden itibaren yeni girilen ağları "
        "etkiler; bu güncellemeden önce kaydedilmiş gizli ağların "
        "\"Diğer/gizli SSID\" üzerinden bir kez daha girilmesi "
        "gerekir)\n"
        "- Düzeltme: adres araması Portekizce ve Felemenkçe için "
        "dil ayarını doğru şekilde dikkate almıyordu";

    const char* const CHANGELOG_ES =
        "- Novedad: la búsqueda de direcciones para ubicaciones "
        "guardadas ahora muestra una lista para elegir cuando el "
        "nombre de un lugar es ambiguo (por ejemplo, \"Cambridge\" "
        "existe varias veces en el mundo), en lugar de tomar "
        "ciegamente el primer resultado\n"
        "- Corrección: las redes WiFi ocultas (SSID oculto) "
        "introducidas manualmente dejaban de conectarse "
        "automáticamente tras un reinicio - ahora se conecta de "
        "forma directa y fiable, en lugar de esperar una "
        "coincidencia en el escaneo de red normal (afecta solo a "
        "las redes introducidas de nuevo a partir de esta "
        "actualización; las redes ocultas guardadas antes deben "
        "volver a introducirse una vez mediante \"Otro/SSID "
        "oculto\")\n"
        "- Corrección: la búsqueda de direcciones no tenía en "
        "cuenta correctamente el idioma configurado para "
        "portugués y neerlandés";

    const char* const CHANGELOG_IT =
        "- Novità: la ricerca dell'indirizzo per le posizioni "
        "salvate ora mostra un elenco tra cui scegliere quando il "
        "nome di un luogo è ambiguo (ad es. \"Cambridge\" esiste "
        "più volte nel mondo), invece di prendere ciecamente il "
        "primo risultato\n"
        "- Correzione: le reti WiFi nascoste (SSID nascosto) "
        "inserite manualmente non si connettevano più "
        "automaticamente dopo un riavvio - ora si connette "
        "direttamente e in modo affidabile, invece di attendere "
        "una corrispondenza nella scansione di rete normale "
        "(riguarda solo le reti inserite di nuovo a partire da "
        "questo aggiornamento; le reti nascoste salvate in "
        "precedenza devono essere reinserite una volta tramite "
        "\"Altro/SSID nascosto\")\n"
        "- Correzione: la ricerca dell'indirizzo non teneva conto "
        "correttamente dell'impostazione della lingua per "
        "portoghese e olandese";

    const char* const CHANGELOG_PT =
        "- Novo: a busca de endereço para locais salvos agora "
        "mostra uma lista para escolher quando um nome de lugar é "
        "ambíguo (por exemplo, \"Cambridge\" existe várias vezes "
        "no mundo), em vez de aceitar cegamente o primeiro "
        "resultado\n"
        "- Correção: redes WiFi ocultas (SSID oculto) inseridas "
        "manualmente paravam de se conectar automaticamente após "
        "uma reinicialização - agora se conecta de forma direta e "
        "confiável, em vez de esperar uma correspondência na "
        "varredura de rede normal (afeta apenas redes inseridas "
        "novamente a partir desta atualização; redes ocultas "
        "salvas antes precisam ser reinseridas uma vez através de "
        "\"Outro/SSID oculto\")\n"
        "- Correção: a busca de endereço não considerava "
        "corretamente a configuração de idioma para português e "
        "holandês";

    const char* const CHANGELOG_NL =
        "- Nieuw: adreszoeken voor locatievoorinstellingen toont "
        "nu een keuzelijst wanneer een plaatsnaam dubbelzinnig is "
        "(bijv. \"Cambridge\" bestaat wereldwijd meerdere keren), "
        "in plaats van blindelings het eerste resultaat over te "
        "nemen\n"
        "- Fix: handmatig ingevoerde verborgen WiFi-netwerken "
        "(verborgen SSID) verbonden na een herstart niet meer "
        "automatisch - verbindt nu direct en betrouwbaar, in "
        "plaats van te wachten op een match in de normale "
        "netwerkscan (geldt alleen voor netwerken die vanaf deze "
        "update opnieuw worden ingevoerd; eerder opgeslagen "
        "verborgen netwerken moeten eenmalig opnieuw worden "
        "ingevoerd via \"Ander/verborgen SSID\")\n"
        "- Fix: adreszoeken hield geen rekening met de "
        "taalinstelling voor Portugees en Nederlands";

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
