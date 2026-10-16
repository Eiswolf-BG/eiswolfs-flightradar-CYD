#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Fix: MQTT connection broke after every reconfiguration of "
        "the broker (fixed a dangling-pointer bug in the MQTT "
        "client)\n"
        "- Fix: Split the broker-address field in MQTT settings into "
        "separate host/port fields and guarded it against overflow "
        "with long values";

    const char* const CHANGELOG_DE =
        "- Fix: MQTT-Verbindung brach nach jeder Neukonfiguration "
        "des Brokers ab (Dangling-Pointer-Bug im MQTT-Client "
        "behoben)\n"
        "- Fix: Eingabefeld für die Broker-Adresse in den MQTT-"
        "Einstellungen in getrennte Host-/Port-Felder aufgeteilt und "
        "gegen Überlauf bei langen Werten abgesichert";

    const char* const CHANGELOG_FR =
        "- Correction : la connexion MQTT se rompait après chaque "
        "reconfiguration du broker (correction d'un bug de pointeur "
        "pendant dans le client MQTT)\n"
        "- Correction : le champ d'adresse du broker dans les "
        "réglages MQTT a été divisé en champs hôte/port séparés et "
        "protégé contre le débordement avec des valeurs longues";

    const char* const CHANGELOG_TR =
        "- Düzeltme: Broker her yeniden yapılandırıldığında MQTT "
        "bağlantısı kopuyordu (MQTT istemcisindeki sarkan işaretçi "
        "hatası düzeltildi)\n"
        "- Düzeltme: MQTT ayarlarındaki broker adresi alanı ayrı "
        "host/port alanlarına bölündü ve uzun değerlerde taşmaya "
        "karşı korundu";

    const char* const CHANGELOG_ES =
        "- Corrección: la conexión MQTT se interrumpía después de "
        "cada reconfiguración del broker (se corrigió un error de "
        "puntero colgante en el cliente MQTT)\n"
        "- Corrección: el campo de dirección del broker en los "
        "ajustes de MQTT se dividió en campos separados de host y "
        "puerto, protegidos contra desbordamiento con valores "
        "largos";

    const char* const CHANGELOG_IT =
        "- Correzione: la connessione MQTT si interrompeva dopo ogni "
        "riconfigurazione del broker (risolto un bug di puntatore "
        "pendente nel client MQTT)\n"
        "- Correzione: il campo dell'indirizzo del broker nelle "
        "impostazioni MQTT è stato diviso in campi host/porta "
        "separati e protetto da overflow con valori lunghi";

    const char* const CHANGELOG_PT =
        "- Correção: a conexão MQTT falhava após cada "
        "reconfiguração do broker (corrigido um bug de ponteiro "
        "pendente no cliente MQTT)\n"
        "- Correção: o campo de endereço do broker nas "
        "configurações MQTT foi dividido em campos separados de "
        "host/porta e protegido contra estouro com valores longos";

    const char* const CHANGELOG_NL =
        "- Fix: MQTT-verbinding viel uit na elke herconfiguratie van "
        "de broker (dangling-pointerbug in de MQTT-client "
        "verholpen)\n"
        "- Fix: het broker-adresveld in de MQTT-instellingen is "
        "opgesplitst in aparte host-/poortvelden en beveiligd tegen "
        "overloop bij lange waarden";

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
