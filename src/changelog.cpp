#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the aircraft detail panel now also shows whether an "
        "aircraft is approaching, departing, or just passing by\n"
        "- New: a subtle \"nearest aircraft\" display (icon + "
        "distance) on the idle screensaver and as a fixed line on "
        "the radar screen itself - regardless of whether overlay "
        "displays are enabled\n"
        "- New: the smart proximity alert now reacts regardless of "
        "the aircraft's altitude\n"
        "- Fix: the bearing/distance line in the detail panel could "
        "never be read in full while auto-scrolling, since a data "
        "update kept resetting the scroll progress";

    const char* const CHANGELOG_DE =
        "- Neu: Das Flugzeug-Detail-Panel zeigt jetzt zusätzlich an, "
        "ob sich ein Flugzeug annähert, entfernt oder vorbeifliegt\n"
        "- Neu: Dezente \"Nächstes Flugzeug\"-Anzeige (Symbol + "
        "Distanz) auf dem Ruhebildschirm sowie fest im Radarschirm "
        "selbst - unabhängig davon, ob Overlay-Anzeigen aktiviert "
        "sind\n"
        "- Neu: Der intelligente Proximity-Alarm reagiert jetzt "
        "unabhängig von der Flughöhe des Flugzeugs\n"
        "- Fix: Die Peilung/Distanz-Zeile im Detail-Panel konnte "
        "beim automatischen Scrollen nie vollständig gelesen "
        "werden, da ein Datenupdate den Scroll-Fortschritt immer "
        "wieder zurücksetzte";

    const char* const CHANGELOG_FR =
        "- Nouveau : le panneau de détails de l'avion indique "
        "désormais aussi si un avion s'approche, s'éloigne ou "
        "passe simplement à proximité\n"
        "- Nouveau : un affichage discret \"avion le plus proche\" "
        "(icône + distance) sur l'écran de veille ainsi qu'en ligne "
        "fixe sur l'écran radar lui-même - que les affichages "
        "superposés soient activés ou non\n"
        "- Nouveau : l'alarme de proximité intelligente réagit "
        "désormais indépendamment de l'altitude de l'avion\n"
        "- Correction : la ligne de relèvement/distance du panneau "
        "de détails ne pouvait jamais être lue en entier pendant le "
        "défilement automatique, car une mise à jour des données "
        "réinitialisait sans cesse la progression du défilement";

    const char* const CHANGELOG_TR =
        "- Yeni: uçak detay paneli artık bir uçağın yaklaşıp "
        "yaklaşmadığını, uzaklaşıp uzaklaşmadığını veya sadece "
        "yanından geçip geçmediğini de gösteriyor\n"
        "- Yeni: bekleme ekranında ve radar ekranının kendisinde "
        "sabit bir satır olarak - kaplama görüntüleri etkin olsun "
        "ya da olmasın - sade bir \"en yakın uçak\" göstergesi "
        "(simge + mesafe)\n"
        "- Yeni: akıllı yakınlık alarmı artık uçağın irtifasından "
        "bağımsız olarak tepki veriyor\n"
        "- Düzeltme: detay panelindeki yön/mesafe satırı otomatik "
        "kaydırma sırasında hiçbir zaman tam olarak okunamıyordu, "
        "çünkü bir veri güncellemesi kaydırma ilerlemesini sürekli "
        "sıfırlıyordu";

    const char* const CHANGELOG_ES =
        "- Novedad: el panel de detalles del avión ahora también "
        "muestra si un avión se está acercando, alejando o "
        "simplemente pasando cerca\n"
        "- Novedad: una discreta indicación de \"avión más cercano\" "
        "(icono + distancia) en el salvapantallas y como línea fija "
        "en la propia pantalla del radar - independientemente de si "
        "las superposiciones están activadas\n"
        "- Novedad: la alarma de proximidad inteligente ahora "
        "reacciona independientemente de la altitud del avión\n"
        "- Corrección: la línea de rumbo/distancia del panel de "
        "detalles nunca podía leerse por completo durante el "
        "desplazamiento automático, ya que una actualización de "
        "datos reiniciaba constantemente el progreso del "
        "desplazamiento";

    const char* const CHANGELOG_IT =
        "- Novità: il pannello dei dettagli dell'aereo ora mostra "
        "anche se un aereo si sta avvicinando, allontanando o sta "
        "semplicemente transitando nelle vicinanze\n"
        "- Novità: un discreto indicatore \"aereo più vicino\" "
        "(icona + distanza) sullo screensaver e come riga fissa "
        "sulla schermata radar stessa - indipendentemente dal fatto "
        "che le sovrapposizioni siano attive\n"
        "- Novità: l'allarme di prossimità intelligente ora reagisce "
        "indipendentemente dall'altitudine dell'aereo\n"
        "- Correzione: la riga di rilevamento/distanza nel pannello "
        "dei dettagli non poteva mai essere letta per intero "
        "durante lo scorrimento automatico, poiché un aggiornamento "
        "dei dati azzerava continuamente l'avanzamento dello "
        "scorrimento";

    const char* const CHANGELOG_PT =
        "- Novo: o painel de detalhes da aeronave agora também "
        "mostra se uma aeronave está se aproximando, se afastando "
        "ou apenas passando por perto\n"
        "- Novo: uma discreta indicação \"aeronave mais próxima\" "
        "(ícone + distância) na proteção de tela e como linha fixa "
        "na própria tela do radar - independentemente de os "
        "overlays estarem ativados\n"
        "- Novo: o alarme de proximidade inteligente agora reage "
        "independentemente da altitude da aeronave\n"
        "- Correção: a linha de direção/distância no painel de "
        "detalhes nunca podia ser lida por completo durante a "
        "rolagem automática, pois uma atualização de dados sempre "
        "reiniciava o progresso da rolagem";

    const char* const CHANGELOG_NL =
        "- Nieuw: het vliegtuig-detailpaneel toont nu ook of een "
        "vliegtuig nadert, zich verwijdert of gewoon voorbijvliegt\n"
        "- Nieuw: een subtiele \"dichtstbijzijnde vliegtuig\"-"
        "weergave (icoon + afstand) op de schermbeveiliging en als "
        "vaste regel op het radarscherm zelf - ongeacht of "
        "overlay-weergaven zijn ingeschakeld\n"
        "- Nieuw: het slimme nabijheidsalarm reageert nu "
        "onafhankelijk van de hoogte van het vliegtuig\n"
        "- Fix: de peiling/afstandsregel in het detailpaneel kon "
        "tijdens het automatisch scrollen nooit volledig worden "
        "gelezen, omdat een gegevensupdate de scrollvoortgang "
        "steeds weer resette";

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
