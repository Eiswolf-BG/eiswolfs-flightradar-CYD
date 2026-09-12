#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Fix: the GPS position feature introduced in the previous "
        "release didn't actually receive data on this board - the "
        "built-in GPS connector is wired to GPIO1 (shared with the "
        "USB-serial console), not the previously assumed pins. GPS is "
        "now read correctly";

    const char* const CHANGELOG_DE =
        "- Fix: Die im letzten Release eingefuehrte GPS-Positions-"
        "Anzeige empfing auf diesem Board tatsaechlich gar keine Daten "
        "- der eingebaute GPS-Steckverbinder ist mit GPIO1 verdrahtet "
        "(geteilt mit der USB-Serial-Konsole), nicht mit den vorher "
        "angenommenen Pins. GPS wird jetzt korrekt ausgelesen";

    const char* const CHANGELOG_FR =
        "- Correction : la fonction de position GPS introduite dans la "
        "version précédente ne recevait en réalité aucune donnée sur "
        "cette carte - le connecteur GPS intégré est câblé sur GPIO1 "
        "(partagé avec la console USB-série), pas sur les broches "
        "supposées auparavant. Le GPS est désormais lu correctement";

    const char* const CHANGELOG_TR =
        "- Düzeltme: Önceki sürümde eklenen GPS konum özelliği bu "
        "kartta aslında hiç veri almıyordu - yerleşik GPS konektörü "
        "GPIO1'e bağlı (USB-seri konsolla paylaşılıyor), daha önce "
        "varsayılan pinlere değil. GPS artık doğru şekilde okunuyor";

    const char* const CHANGELOG_ES =
        "- Corrección: la función de posición GPS introducida en la "
        "versión anterior en realidad no recibía datos en esta placa "
        "- el conector GPS integrado está conectado al GPIO1 "
        "(compartido con la consola USB-serie), no a los pines "
        "asumidos anteriormente. El GPS ahora se lee correctamente";

    const char* const CHANGELOG_IT =
        "- Correzione: la funzione di posizione GPS introdotta nella "
        "versione precedente in realtà non riceveva dati su questa "
        "scheda - il connettore GPS integrato è collegato al GPIO1 "
        "(condiviso con la console USB-seriale), non ai pin ipotizzati "
        "in precedenza. Il GPS ora viene letto correttamente";

    const char* const CHANGELOG_PT =
        "- Correção: o recurso de posição GPS introduzido na versão "
        "anterior na verdade não recebia dados nesta placa - o "
        "conector GPS embutido está ligado ao GPIO1 (compartilhado "
        "com o console USB-serial), não aos pinos presumidos "
        "anteriormente. O GPS agora é lido corretamente";

    const char* const CHANGELOG_NL =
        "- Fix: de in de vorige release geïntroduceerde GPS-"
        "positiefunctie ontving op dit board in werkelijkheid helemaal "
        "geen data - de ingebouwde GPS-connector is bedraad op GPIO1 "
        "(gedeeld met de USB-seriële console), niet op de eerder "
        "aangenomen pinnen. GPS wordt nu correct uitgelezen";

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
