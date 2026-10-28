#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Fix: Repeatedly switching between ADS-B data sources could "
        "cause the device to crash and restart in a loop - fixed by "
        "properly closing the old connection before opening a new one "
        "and reducing CPU load while parsing\n"
        "- Fix: Removed airplanes.live from the data source list - it "
        "requires manual registration and proved unreliable without it";

    const char* const CHANGELOG_DE =
        "- Fix: Mehrfaches Wechseln zwischen den ADS-B-Datenquellen "
        "konnte zu wiederholten Abstürzen/Neustarts führen - behoben, "
        "indem die alte Verbindung vor dem Aufbau einer neuen sauber "
        "geschlossen und die CPU-Last beim Verarbeiten verringert wird\n"
        "- Fix: airplanes.live aus der Quellenliste entfernt - verlangt "
        "eine manuelle Registrierung und erwies sich ohne diese als zu "
        "unzuverlässig";

    const char* const CHANGELOG_FR =
        "- Correction : passer plusieurs fois entre les sources de "
        "données ADS-B pouvait provoquer des plantages et redémarrages "
        "en boucle de l'appareil - corrigé en fermant proprement "
        "l'ancienne connexion avant d'en ouvrir une nouvelle et en "
        "réduisant la charge CPU pendant l'analyse\n"
        "- Correction : airplanes.live a été retiré de la liste des "
        "sources - nécessite une inscription manuelle et s'est révélé "
        "peu fiable sans elle";

    const char* const CHANGELOG_TR =
        "- Düzeltme: ADS-B veri kaynakları arasında tekrar tekrar geçiş "
        "yapmak, cihazın döngü halinde çökmesine ve yeniden başlamasına "
        "neden olabiliyordu - yeni bir bağlantı açmadan önce eskisinin "
        "düzgün kapatılması ve ayrıştırma sırasındaki CPU yükünün "
        "azaltılmasıyla düzeltildi\n"
        "- Düzeltme: airplanes.live kaynak listesinden kaldırıldı - "
        "manuel kayıt gerektiriyor ve bu olmadan güvenilmez olduğu "
        "görüldü";

    const char* const CHANGELOG_ES =
        "- Corrección: cambiar repetidamente entre las fuentes de datos "
        "ADS-B podía provocar que el dispositivo se bloqueara y se "
        "reiniciara en bucle - corregido cerrando correctamente la "
        "conexión anterior antes de abrir una nueva y reduciendo la "
        "carga de CPU durante el análisis\n"
        "- Corrección: se eliminó airplanes.live de la lista de fuentes "
        "- requiere un registro manual y resultó poco fiable sin él";

    const char* const CHANGELOG_IT =
        "- Correzione: cambiare ripetutamente tra le fonti dati ADS-B "
        "poteva causare blocchi e riavvii in loop del dispositivo - "
        "risolto chiudendo correttamente la vecchia connessione prima "
        "di aprirne una nuova e riducendo il carico della CPU durante "
        "l'analisi\n"
        "- Correzione: airplanes.live è stata rimossa dall'elenco delle "
        "fonti - richiede una registrazione manuale e si è rivelata "
        "inaffidabile senza di essa";

    const char* const CHANGELOG_PT =
        "- Correção: alternar repetidamente entre as fontes de dados "
        "ADS-B podia causar travamentos e reinicializações em loop do "
        "dispositivo - corrigido fechando corretamente a conexão antiga "
        "antes de abrir uma nova e reduzindo a carga de CPU durante a "
        "análise\n"
        "- Correção: airplanes.live foi removida da lista de fontes - "
        "exige registro manual e se mostrou pouco confiável sem ele";

    const char* const CHANGELOG_NL =
        "- Fix: herhaaldelijk wisselen tussen de ADS-B-gegevensbronnen "
        "kon ertoe leiden dat het apparaat herhaaldelijk crashte en "
        "opnieuw opstartte - opgelost door de oude verbinding netjes te "
        "sluiten voordat een nieuwe wordt geopend en door de "
        "CPU-belasting tijdens het verwerken te verlagen\n"
        "- Fix: airplanes.live is verwijderd uit de bronnenlijst - "
        "vereist handmatige registratie en bleek daarzonder "
        "onbetrouwbaar";

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
