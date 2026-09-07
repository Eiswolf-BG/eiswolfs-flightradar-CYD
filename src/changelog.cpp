#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the radar range can now also be set from the web "
        "interface - changes there and on the device stay in sync "
        "automatically in both directions\n"
        "- Fix: a large query range could fail to fetch data due to "
        "high memory usage and falsely show \"No connection\" - "
        "fundamentally fixed, even with very high aircraft counts\n"
        "- Fix: when more aircraft were in range than could be shown, "
        "the nearest ones weren't reliably prioritized - fixed\n"
        "- Fix: the rain effect on the idle screen could partially "
        "cover the time and date";

    const char* const CHANGELOG_DE =
        "- Neu: Die Radar-Reichweite lässt sich jetzt auch über die "
        "Web-Oberfläche einstellen - Änderungen dort und am Gerät "
        "selbst gleichen sich in beide Richtungen automatisch ab\n"
        "- Fix: Bei größerer Reichweite konnte der Datenabruf durch "
        "hohen Speicherbedarf fehlschlagen und das Gerät fälschlich "
        "\"Keine Verbindung\" anzeigen lassen - grundlegend behoben, "
        "auch bei sehr hoher Flugzeuganzahl\n"
        "- Fix: Bei mehr Flugzeugen in Reichweite als angezeigt werden "
        "können, wurden nicht zuverlässig die nächstgelegenen "
        "bevorzugt - behoben\n"
        "- Fix: Der Regen-Effekt auf dem Ruhebildschirm konnte Uhrzeit "
        "und Datum teilweise verdecken";

    const char* const CHANGELOG_FR =
        "- Nouveau : la portée du radar peut désormais aussi être "
        "réglée depuis l'interface web - les modifications faites là "
        "ou sur l'appareil se synchronisent automatiquement dans les "
        "deux sens\n"
        "- Correction : à grande portée, la récupération des données "
        "pouvait échouer en raison d'une consommation mémoire élevée "
        "et faire afficher à tort \"Pas de connexion\" - corrigé en "
        "profondeur, même avec un très grand nombre d'avions\n"
        "- Correction : lorsqu'il y avait plus d'avions à portée "
        "qu'il n'était possible d'en afficher, les plus proches "
        "n'étaient pas systématiquement privilégiés - corrigé\n"
        "- Correction : l'effet de pluie sur l'écran de veille "
        "pouvait partiellement masquer l'heure et la date";

    const char* const CHANGELOG_TR =
        "- Yeni: Radar menzili artık web arayüzünden de "
        "ayarlanabiliyor - orada ve cihazın kendisinde yapılan "
        "değişiklikler otomatik olarak her iki yönde de senkronize "
        "oluyor\n"
        "- Düzeltme: Daha geniş menzilde, yüksek bellek kullanımı "
        "nedeniyle veri alma başarısız olabiliyor ve cihaz yanlışlıkla "
        "\"Bağlantı yok\" gösterebiliyordu - çok yüksek uçak "
        "sayılarında bile kalıcı olarak düzeltildi\n"
        "- Düzeltme: Menzildeki uçak sayısı gösterilebilecek sayıdan "
        "fazla olduğunda en yakın uçaklar güvenilir şekilde öncelik "
        "kazanmıyordu - düzeltildi\n"
        "- Düzeltme: Bekleme ekranındaki yağmur efekti saati ve "
        "tarihi kısmen kapatabiliyordu";

    const char* const CHANGELOG_ES =
        "- Novedad: el alcance del radar ahora también se puede "
        "ajustar desde la interfaz web - los cambios allí y en el "
        "propio dispositivo se sincronizan automáticamente en ambas "
        "direcciones\n"
        "- Corrección: con un alcance mayor, la obtención de datos "
        "podía fallar por el alto uso de memoria y hacer que el "
        "dispositivo mostrara erróneamente \"Sin conexión\" - "
        "corregido de raíz, incluso con un número muy alto de "
        "aviones\n"
        "- Corrección: cuando había más aviones en alcance de los que "
        "se podían mostrar, no siempre se priorizaban los más "
        "cercanos - corregido\n"
        "- Corrección: el efecto de lluvia en la pantalla de reposo "
        "podía cubrir parcialmente la hora y la fecha";

    const char* const CHANGELOG_IT =
        "- Novità: ora è possibile impostare il raggio del radar "
        "anche dall'interfaccia web - le modifiche fatte lì e sul "
        "dispositivo stesso si sincronizzano automaticamente in "
        "entrambe le direzioni\n"
        "- Correzione: con un raggio maggiore, il recupero dei dati "
        "poteva fallire per l'elevato utilizzo di memoria e far "
        "mostrare erroneamente al dispositivo \"Nessuna connessione\" "
        "- risolto alla radice, anche con un numero molto elevato di "
        "aerei\n"
        "- Correzione: quando c'erano più aerei nel raggio di quanti "
        "se ne potessero mostrare, i più vicini non venivano sempre "
        "privilegiati - risolto\n"
        "- Correzione: l'effetto pioggia nella schermata di riposo "
        "poteva coprire parzialmente l'ora e la data";

    const char* const CHANGELOG_PT =
        "- Novo: o alcance do radar agora também pode ser ajustado "
        "pela interface web - as alterações feitas lá e no próprio "
        "aparelho são sincronizadas automaticamente em ambas as "
        "direções\n"
        "- Correção: com um alcance maior, a busca de dados podia "
        "falhar devido ao alto uso de memória e fazer o aparelho "
        "mostrar erroneamente \"Sem conexão\" - corrigido pela raiz, "
        "mesmo com um número muito alto de aeronaves\n"
        "- Correção: quando havia mais aeronaves no alcance do que "
        "podiam ser exibidas, as mais próximas nem sempre eram "
        "priorizadas - corrigido\n"
        "- Correção: o efeito de chuva na tela de descanso podia "
        "cobrir parcialmente a hora e a data";

    const char* const CHANGELOG_NL =
        "- Nieuw: het radarbereik is nu ook instelbaar via de "
        "webinterface - wijzigingen daar en op het apparaat zelf "
        "worden automatisch in beide richtingen gesynchroniseerd\n"
        "- Fix: bij een groter bereik kon het ophalen van gegevens "
        "mislukken door hoog geheugengebruik, waardoor het apparaat "
        "ten onrechte \"Geen verbinding\" toonde - fundamenteel "
        "verholpen, ook bij een zeer groot aantal vliegtuigen\n"
        "- Fix: als er meer vliegtuigen binnen bereik waren dan "
        "getoond konden worden, kregen de dichtstbijzijnde niet "
        "altijd voorrang - verholpen\n"
        "- Fix: het regeneffect op het rustscherm kon de tijd en "
        "datum gedeeltelijk aan het zicht onttrekken";

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
