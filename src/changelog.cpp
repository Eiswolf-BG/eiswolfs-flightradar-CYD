#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Fix: with the web interface open, the automatic increase "
        "of the query range could in rare cases cause a data fetch "
        "error that made the device falsely show \"No connection\"\n"
        "- Fix: a flicker on confirmation/info screens (e.g. after a "
        "firmware update) has been fixed";

    const char* const CHANGELOG_DE =
        "- Fix: Bei geöffneter Web-Oberfläche konnte die automatische "
        "Erhöhung der Abfrage-Reichweite in seltenen Fällen zu einem "
        "Datenabruf-Fehler führen, der das Gerät fälschlich \"Keine "
        "Verbindung\" anzeigen ließ\n"
        "- Fix: Ein Flackern auf Bestätigungs-/Info-Bildschirmen (z.B. "
        "nach einem Firmware-Update) wurde behoben";

    const char* const CHANGELOG_FR =
        "- Correction : lorsque l'interface web était ouverte, "
        "l'augmentation automatique du rayon de requête pouvait "
        "dans de rares cas provoquer une erreur de récupération de "
        "données faisant afficher à tort \"Pas de connexion\"\n"
        "- Correction : un scintillement sur les écrans de "
        "confirmation/info (par ex. après une mise à jour du "
        "firmware) a été corrigé";

    const char* const CHANGELOG_TR =
        "- Düzeltme: web arayüzü açıkken, sorgu menzilinin otomatik "
        "artırılması nadiren bir veri alma hatasına yol açarak "
        "cihazın yanlışlıkla \"Bağlantı yok\" göstermesine neden "
        "olabiliyordu\n"
        "- Düzeltme: onay/bilgi ekranlarındaki (örn. bir bellenim "
        "güncellemesinden sonra) bir titreme sorunu giderildi";

    const char* const CHANGELOG_ES =
        "- Corrección: con la interfaz web abierta, el aumento "
        "automático del alcance de consulta podía, en casos raros, "
        "causar un error de obtención de datos que hacía que el "
        "dispositivo mostrara erróneamente \"Sin conexión\"\n"
        "- Corrección: se corrigió un parpadeo en las pantallas de "
        "confirmación/información (por ejemplo, tras una "
        "actualización de firmware)";

    const char* const CHANGELOG_IT =
        "- Correzione: con l'interfaccia web aperta, l'aumento "
        "automatico del raggio di interrogazione poteva, in rari "
        "casi, causare un errore di recupero dati che faceva "
        "mostrare erroneamente al dispositivo \"Nessuna "
        "connessione\"\n"
        "- Correzione: risolto uno sfarfallio nelle schermate di "
        "conferma/informazione (ad es. dopo un aggiornamento del "
        "firmware)";

    const char* const CHANGELOG_PT =
        "- Correção: com a interface web aberta, o aumento "
        "automático do alcance de consulta podia, em casos raros, "
        "causar um erro de busca de dados que fazia o aparelho "
        "mostrar erroneamente \"Sem conexão\"\n"
        "- Correção: corrigido um tremular nas telas de confirmação/"
        "informação (por exemplo, após uma atualização de firmware)";

    const char* const CHANGELOG_NL =
        "- Fix: bij een geopende webinterface kon de automatische "
        "verhoging van het zoekbereik in zeldzame gevallen een "
        "gegevensophaalfout veroorzaken, waardoor het apparaat ten "
        "onrechte \"Geen verbinding\" toonde\n"
        "- Fix: een knipperprobleem op bevestigings-/infoschermen "
        "(bijv. na een firmware-update) is verholpen";

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
