#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Improvement: The data source picker on the Connection status "
        "screen (System > Tools) is now a proper list of all three "
        "sources (adsb.lol, airplanes.live, adsb.fi) with a dot showing "
        "which one is active, instead of a button that jumped to a "
        "random next source on each tap\n"
        "- Fix: The Connection status screen used to redraw the whole "
        "screen roughly every half second even when nothing on it had "
        "actually changed, causing a visible flicker - it now only "
        "redraws when the displayed content really changed";

    const char* const CHANGELOG_DE =
        "- Verbesserung: Die Datenquellen-Auswahl auf der "
        "Verbindungsstatus-Seite (System > Werkzeuge) ist jetzt eine "
        "richtige Liste aller drei Quellen (adsb.lol, airplanes.live, "
        "adsb.fi) mit einer Punkt-Anzeige, welche davon aktiv ist - "
        "statt eines Knopfs, der bei jedem Antippen zufällig zur "
        "nächsten Quelle sprang\n"
        "- Fix: Die Verbindungsstatus-Seite hat bisher etwa alle halbe "
        "Sekunde den kompletten Bildschirm neu gezeichnet, auch wenn "
        "sich gar nichts geändert hatte - das führte zu sichtbarem "
        "Flackern. Jetzt wird nur noch neu gezeichnet, wenn sich der "
        "angezeigte Inhalt tatsächlich geändert hat";

    const char* const CHANGELOG_FR =
        "- Amélioration : le sélecteur de source de données sur l'écran "
        "État de la connexion (Système > Outils) est désormais une "
        "vraie liste des trois sources (adsb.lol, airplanes.live, "
        "adsb.fi) avec un point indiquant celle qui est active, au lieu "
        "d'un bouton qui passait de façon aléatoire à la source "
        "suivante à chaque tapotement\n"
        "- Correction : l'écran État de la connexion redessinait "
        "entièrement l'écran environ toutes les demi-secondes, même "
        "sans aucun changement réel, ce qui provoquait un scintillement "
        "visible - il ne redessine désormais que lorsque le contenu "
        "affiché a réellement changé";

    const char* const CHANGELOG_TR =
        "- İyileştirme: Bağlantı durumu ekranındaki (Sistem > Araçlar) "
        "veri kaynağı seçimi artık her üç kaynağı (adsb.lol, "
        "airplanes.live, adsb.fi) gösteren gerçek bir liste - hangisinin "
        "etkin olduğunu bir nokta gösteriyor - her dokunuşta rastgele "
        "bir sonraki kaynağa atlayan bir düğme yerine\n"
        "- Düzeltme: Bağlantı durumu ekranı, hiçbir şey değişmese bile "
        "yaklaşık her yarım saniyede bir tüm ekranı yeniden çiziyordu - "
        "bu da gözle görülür bir titremeye yol açıyordu. Artık yalnızca "
        "gösterilen içerik gerçekten değiştiğinde yeniden çiziliyor";

    const char* const CHANGELOG_ES =
        "- Mejora: el selector de fuente de datos en la pantalla Estado "
        "de la conexión (Sistema > Herramientas) es ahora una lista "
        "real de las tres fuentes (adsb.lol, airplanes.live, adsb.fi) "
        "con un punto que muestra cuál está activa, en lugar de un "
        "botón que saltaba aleatoriamente a la siguiente fuente con "
        "cada toque\n"
        "- Corrección: la pantalla Estado de la conexión redibujaba "
        "toda la pantalla aproximadamente cada medio segundo aunque no "
        "hubiera cambiado nada, lo que provocaba un parpadeo visible - "
        "ahora solo se redibuja cuando el contenido mostrado realmente "
        "cambia";

    const char* const CHANGELOG_IT =
        "- Miglioramento: il selettore della fonte dati nella schermata "
        "Stato della connessione (Sistema > Strumenti) è ora un vero "
        "elenco di tutte e tre le fonti (adsb.lol, airplanes.live, "
        "adsb.fi) con un pallino che indica quale sia attiva, al posto "
        "di un pulsante che saltava in modo casuale alla fonte "
        "successiva a ogni tocco\n"
        "- Correzione: la schermata Stato della connessione ridisegnava "
        "l'intero schermo circa ogni mezzo secondo anche senza alcuna "
        "modifica reale, causando uno sfarfallio visibile - ora si "
        "ridisegna solo quando il contenuto mostrato cambia davvero";

    const char* const CHANGELOG_PT =
        "- Melhoria: o seletor de fonte de dados na tela Estado da "
        "conexão (Sistema > Ferramentas) agora é uma lista completa das "
        "três fontes (adsb.lol, airplanes.live, adsb.fi) com um ponto "
        "indicando qual está ativa, em vez de um botão que pulava "
        "aleatoriamente para a próxima fonte a cada toque\n"
        "- Correção: a tela Estado da conexão redesenhava a tela "
        "inteira a cada meio segundo aproximadamente, mesmo sem "
        "nenhuma mudança real, causando uma cintilação visível - agora "
        "só redesenha quando o conteúdo exibido realmente muda";

    const char* const CHANGELOG_NL =
        "- Verbetering: de keuzelijst voor de gegevensbron op het scherm "
        "Verbindingsstatus (Systeem > Hulpmiddelen) is nu een echte "
        "lijst met alle drie de bronnen (adsb.lol, airplanes.live, "
        "adsb.fi) met een stip die laat zien welke actief is - in plaats "
        "van een knop die bij elke tik willekeurig naar de volgende bron "
        "sprong\n"
        "- Fix: het scherm Verbindingsstatus tekende ongeveer elke "
        "halve seconde het hele scherm opnieuw, zelfs als er niets "
        "veranderd was - dat zorgde voor zichtbaar geflikker. Nu wordt "
        "alleen opnieuw getekend als de getoonde inhoud echt is "
        "veranderd";

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
