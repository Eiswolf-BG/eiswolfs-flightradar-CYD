#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: a hint in the web interface now points out that it "
        "can be installed as its own app\n"
        "- Fix: loading the web interface could take several "
        "seconds up to a noticeable delay due to inefficient "
        "logbook file access - significantly faster now";

    const char* const CHANGELOG_DE =
        "- Neu: Ein Hinweis in der Web-Oberfläche macht jetzt "
        "darauf aufmerksam, dass sie sich als eigene App "
        "installieren lässt\n"
        "- Fix: Das Laden der Web-Oberfläche konnte durch "
        "ineffiziente Logbuch-Dateizugriffe mehrere Sekunden bis "
        "hin zu spürbaren Verzögerungen dauern - deutlich "
        "beschleunigt";

    const char* const CHANGELOG_FR =
        "- Nouveau : un indice dans l'interface web signale "
        "désormais qu'elle peut être installée comme sa propre "
        "application\n"
        "- Correction : le chargement de l'interface web pouvait "
        "prendre plusieurs secondes voire un délai perceptible à "
        "cause d'accès inefficaces aux fichiers du journal de vol - "
        "nettement plus rapide maintenant";

    const char* const CHANGELOG_TR =
        "- Yeni: web arayüzündeki bir ipucu artık kendi uygulaması "
        "olarak yüklenebileceğine dikkat çekiyor\n"
        "- Düzeltme: web arayüzünün yüklenmesi, verimsiz uçuş "
        "defteri dosya erişimleri nedeniyle birkaç saniyeden "
        "belirgin bir gecikmeye kadar sürebiliyordu - artık "
        "belirgin şekilde daha hızlı";

    const char* const CHANGELOG_ES =
        "- Novedad: un aviso en la interfaz web ahora indica que se "
        "puede instalar como su propia app\n"
        "- Corrección: la carga de la interfaz web podía tardar "
        "varios segundos, incluso con un retraso notable, debido a "
        "accesos ineficientes a los archivos del cuaderno de vuelo "
        "- ahora considerablemente más rápida";

    const char* const CHANGELOG_IT =
        "- Novità: un avviso nell'interfaccia web ora segnala che "
        "può essere installata come app a sé stante\n"
        "- Correzione: il caricamento dell'interfaccia web poteva "
        "richiedere diversi secondi, con ritardi percepibili, a "
        "causa di accessi inefficienti ai file del diario di volo - "
        "ora notevolmente più veloce";

    const char* const CHANGELOG_PT =
        "- Novo: um aviso na interface web agora indica que ela "
        "pode ser instalada como um app próprio\n"
        "- Correção: o carregamento da interface web podia levar "
        "vários segundos, com atrasos perceptíveis, devido a "
        "acessos ineficientes aos arquivos do diário de bordo - "
        "agora consideravelmente mais rápido";

    const char* const CHANGELOG_NL =
        "- Nieuw: een hint in de webinterface wijst er nu op dat "
        "deze als eigen app geïnstalleerd kan worden\n"
        "- Fix: het laden van de webinterface kon door "
        "inefficiënte logboek-bestandstoegang meerdere seconden tot "
        "een merkbare vertraging duren - nu aanzienlijk sneller";

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
