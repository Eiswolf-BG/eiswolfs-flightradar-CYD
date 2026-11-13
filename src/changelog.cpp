#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: The WiFi network list during setup is now sorted by "
        "signal strength, so your network is less likely to fall off "
        "the list when many networks are nearby.\n"
        "- Fix: Fixed a rare restart loop shortly after startup, caused "
        "by the weather lookup.";

    const char* const CHANGELOG_DE =
        "- Neu: Die WLAN-Netzliste bei der Einrichtung ist jetzt nach "
        "Signalstärke sortiert, damit dein Netzwerk bei vielen "
        "Netzwerken in der Nähe seltener aus der Liste fällt.\n"
        "- Fix: Einen seltenen Neustart-Loop kurz nach dem Start "
        "behoben, der durch die Wetterabfrage ausgelöst wurde.";

    const char* const CHANGELOG_FR =
        "- Nouveau : La liste des réseaux WiFi lors de la configuration "
        "est désormais triée par force du signal, afin que votre réseau "
        "ait moins de risques de disparaître de la liste lorsqu'il y a "
        "beaucoup de réseaux à proximité.\n"
        "- Correction : Correction d'une boucle de redémarrage rare peu "
        "après le démarrage, causée par la recherche météo.";

    const char* const CHANGELOG_TR =
        "- Yeni: Kurulum sırasında WiFi ağ listesi artık sinyal gücüne "
        "göre sıralanıyor, böylece yakınında çok sayıda ağ olduğunda "
        "senin ağın listeden düşme ihtimali azalıyor.\n"
        "- Düzeltme: Başlangıçtan kısa süre sonra hava durumu "
        "sorgusunun neden olduğu nadir bir yeniden başlatma döngüsü "
        "düzeltildi.";

    const char* const CHANGELOG_ES =
        "- Novedad: La lista de redes WiFi durante la configuración "
        "ahora está ordenada por intensidad de señal, para que tu red "
        "tenga menos probabilidades de desaparecer de la lista cuando "
        "hay muchas redes cerca.\n"
        "- Corrección: Se corrigió un raro bucle de reinicio poco "
        "después del arranque, causado por la consulta del tiempo.";

    const char* const CHANGELOG_IT =
        "- Novità: L'elenco delle reti WiFi durante la configurazione è "
        "ora ordinato per intensità del segnale, così la tua rete ha "
        "meno probabilità di scomparire dall'elenco quando ci sono "
        "molte reti nelle vicinanze.\n"
        "- Correzione: Risolto un raro ciclo di riavvio poco dopo "
        "l'avvio, causato dalla richiesta meteo.";

    const char* const CHANGELOG_PT =
        "- Novo: A lista de redes WiFi durante a configuração agora é "
        "ordenada por intensidade de sinal, para que sua rede tenha "
        "menos chance de sair da lista quando há muitas redes por "
        "perto.\n"
        "- Correção: Corrigido um raro loop de reinicialização pouco "
        "depois de iniciar, causado pela consulta de meteorologia.";

    const char* const CHANGELOG_NL =
        "- Nieuw: De wifi-netwerklijst tijdens het instellen is nu "
        "gesorteerd op signaalsterkte, zodat je netwerk minder snel van "
        "de lijst valt wanneer er veel netwerken in de buurt zijn.\n"
        "- Fix: Een zeldzame herstartlus kort na het opstarten, "
        "veroorzaakt door de weersopvraging, is verholpen.";

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
