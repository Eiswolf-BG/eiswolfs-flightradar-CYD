#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Fix: the aircraft detail panel used to close "
        "automatically when the shown aircraft left radar range - "
        "it now stays open until manually closed by tapping\n"
        "- New: the GitHub info screen (with QR code) is reachable "
        "again, under Menu → System → Tools → \"About\"\n"
        "- Fix: occasional remnants of aircraft callsign labels "
        "near the top edge of the radar that stayed visible for "
        "several sweep cycles\n"
        "- Changed: IATA airport codes are now the default on a "
        "fresh install (instead of ICAO)\n"
        "- New: a small heading indicator in front of each aircraft "
        "silhouette, showing its current direction of travel";

    const char* const CHANGELOG_DE =
        "- Fix: das Flugzeug-Detail-Panel schloss sich bisher "
        "automatisch, wenn das angezeigte Flugzeug den Radarbereich "
        "verliess - bleibt jetzt offen, bis es manuell durch "
        "Antippen geschlossen wird\n"
        "- Neu: der GitHub-Info-Screen (mit QR-Code) ist wieder "
        "erreichbar, unter Menü → System → Werkzeuge → "
        "\"Über\"\n"
        "- Fix: vereinzelte Reste von Flugzeug-Rufzeichen-Labels "
        "nahe des oberen Radarrands, die mehrere Sweep-Zyklen lang "
        "sichtbar blieben\n"
        "- Geändert: IATA-Flughafencodes sind jetzt der Standard "
        "bei einer frischen Installation (statt ICAO)\n"
        "- Neu: ein kleiner Richtungsindikator vor jeder "
        "Flugzeug-Silhouette zeigt die aktuelle Flugrichtung an";

    const char* const CHANGELOG_FR =
        "- Correction : le panneau de détails de l'avion se "
        "fermait auparavant automatiquement lorsque l'avion "
        "affiché quittait la portée du radar - reste maintenant "
        "ouvert jusqu'à une fermeture manuelle par un appui\n"
        "- Nouveau : l'écran d'informations GitHub (avec code QR) "
        "est de nouveau accessible, sous Menu → Système → "
        "Outils → \"À propos\"\n"
        "- Correction : des restes isolés d'indicatifs d'avions "
        "près du bord supérieur du radar, qui restaient visibles "
        "pendant plusieurs cycles de balayage\n"
        "- Modifié : les codes d'aéroport IATA sont désormais la "
        "valeur par défaut lors d'une nouvelle installation (au "
        "lieu de l'OACI)\n"
        "- Nouveau : un petit indicateur de direction devant chaque "
        "silhouette d'avion, indiquant le cap actuel";

    const char* const CHANGELOG_TR =
        "- Düzeltme: gösterilen uçak radar menzilinden "
        "çıktığında uçak detay paneli daha önce "
        "otomatik olarak kapanıyordu - artık yalnızca dokunarak "
        "manuel olarak kapatılana kadar açık kalıyor\n"
        "- Yeni: GitHub bilgi ekranı (QR kodlu) artık Menü → "
        "Sistem → Araçlar → \"Hakkında\" altında tekrar "
        "erişilebilir\n"
        "- Düzeltme: radarın üst kenarına yakın uçak "
        "çağrı işareti etiketlerinin birkaç tarama döngüsü "
        "boyunca görünür kalan tekil kalıntıları\n"
        "- Değiştirildi: yeni bir kurulumda artık ICAO yerine "
        "IATA havalimanı kodları varsayılan oluyor\n"
        "- Yeni: her uçak silüetinin önünde, mevcut uçuş "
        "yönünü gösteren küçük bir yön göstergesi";

    const char* const CHANGELOG_ES =
        "- Corrección: el panel de detalles del avión antes se "
        "cerraba automáticamente cuando el avión mostrado salía "
        "del alcance del radar - ahora permanece abierto hasta "
        "cerrarlo manualmente tocándolo\n"
        "- Novedad: la pantalla de información de GitHub (con "
        "código QR) vuelve a estar accesible, en Menú → Sistema "
        "→ Herramientas → \"Acerca de\"\n"
        "- Corrección: restos aislados de etiquetas de indicativo "
        "de avión cerca del borde superior del radar que "
        "permanecían visibles durante varios ciclos de barrido\n"
        "- Cambiado: los códigos de aeropuerto IATA son ahora el "
        "valor predeterminado en una instalación nueva (en lugar "
        "de OACI)\n"
        "- Novedad: un pequeño indicador de dirección delante de "
        "cada silueta de avión, que muestra el rumbo actual";

    const char* const CHANGELOG_IT =
        "- Correzione: il pannello dei dettagli dell'aereo prima si "
        "chiudeva automaticamente quando l'aereo mostrato usciva "
        "dalla portata del radar - ora rimane aperto finché non "
        "viene chiuso manualmente toccandolo\n"
        "- Novità: la schermata informativa di GitHub (con codice "
        "QR) è di nuovo raggiungibile, in Menu → Sistema → "
        "Strumenti → \"Informazioni\"\n"
        "- Correzione: residui isolati di etichette del nominativo "
        "dell'aereo vicino al bordo superiore del radar, che "
        "restavano visibili per diversi cicli di scansione\n"
        "- Modificato: i codici aeroportuali IATA sono ora "
        "l'impostazione predefinita su un'installazione nuova "
        "(invece dell'ICAO)\n"
        "- Novità: un piccolo indicatore di direzione davanti a "
        "ogni sagoma di aereo, che mostra la rotta attuale";

    const char* const CHANGELOG_PT =
        "- Correção: o painel de detalhes da aeronave antes se "
        "fechava automaticamente quando a aeronave exibida saía "
        "do alcance do radar - agora permanece aberto até ser "
        "fechado manualmente ao tocar\n"
        "- Novo: a tela de informações do GitHub (com código QR) "
        "está acessível novamente, em Menu → Sistema → "
        "Ferramentas → \"Sobre\"\n"
        "- Correção: restos isolados de rótulos de indicativo de "
        "chamada de aeronaves perto da borda superior do radar, "
        "que permaneciam visíveis por vários ciclos de "
        "varredura\n"
        "- Alterado: os códigos de aeroporto IATA agora são o "
        "padrão em uma instalação nova (em vez de ICAO)\n"
        "- Novo: um pequeno indicador de direção na frente de "
        "cada silhueta de aeronave, mostrando o rumo atual";

    const char* const CHANGELOG_NL =
        "- Fix: het vliegtuig-detailpaneel sloot voorheen "
        "automatisch wanneer het getoonde vliegtuig het "
        "radarbereik verliet - blijft nu open totdat het "
        "handmatig wordt gesloten door erop te tikken\n"
        "- Nieuw: het GitHub-infoscherm (met QR-code) is weer "
        "bereikbaar, onder Menu → Systeem → Hulpmiddelen → "
        "\"Over\"\n"
        "- Fix: incidentele restanten van vliegtuig-roepnaamlabels "
        "dicht bij de bovenrand van de radar, die meerdere "
        "sweep-cycli zichtbaar bleven\n"
        "- Gewijzigd: IATA-luchthavencodes zijn nu de standaard bij "
        "een verse installatie (in plaats van ICAO)\n"
        "- Nieuw: een kleine richtingsindicator vóór elke "
        "vliegtuigsilhouet, die de huidige vliegrichting toont";

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
