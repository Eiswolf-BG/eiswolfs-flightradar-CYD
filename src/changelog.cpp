#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the entire live radar website (aircraft info popup, "
        "weather popup, connection status, PWA install hint, logbook "
        "page, airline filter/watchlist management) is now fully bound "
        "to the device's language setting\n"
        "- New: altitude/speed/climb-descent rate in the website's "
        "aircraft popup now switch between metric and imperial, matching "
        "the device setting\n"
        "- New: the live radar website now has its own control area - "
        "color scheme, \"Show Weather\", and military/government "
        "detection can be switched from the website, synced with the "
        "device in both directions\n"
        "- New: the live radar website now shows a color legend for the "
        "aircraft markers, matching the markers' altitude colors\n"
        "- Fix: a color scheme changed via the new web remote control "
        "wasn't picked up immediately by the Mode/Menu header buttons or "
        "the screensaver clock/date - now redraws right away\n"
        "- Fix: the tap hint on the website's FlightAware link was stuck "
        "in German regardless of device language - now translated too";

    const char* const CHANGELOG_DE =
        "- Neu: Die komplette Live-Radar-Webseite (Flugzeug-Info-Popup, "
        "Wetter-Popup, Verbindungsstatus, PWA-Installationshinweis, "
        "Logbuch-Seite, Airline-Filter-/Beobachtungslisten-Verwaltung) "
        "ist jetzt vollstaendig an die Geraete-Spracheinstellung gebunden\n"
        "- Neu: Altitude/Geschwindigkeit/Steig-Sinkrate im Flugzeug-Popup "
        "der Webseite schalten jetzt zwischen metrisch und imperial um, "
        "passend zur Geraete-Einstellung\n"
        "- Neu: Die Live-Radar-Webseite hat jetzt einen eigenen "
        "Kontrollbereich - Farbschema, \"Wetter anzeigen\" und Militaer-/"
        "Behoerdenflug-Erkennung lassen sich von der Webseite aus "
        "umschalten, synchron mit dem Geraet in beide Richtungen\n"
        "- Neu: Die Live-Radar-Webseite zeigt jetzt eine Farb-Legende fuer "
        "die Flugzeugmarker, passend zu deren Hoehenfarben\n"
        "- Fix: Ein ueber die neue Web-Fernsteuerung geaendertes "
        "Farbschema wurde von den Mode-/Menu-Kopfzeilen-Buttons und der "
        "Ruhebildschirm-Uhr/dem -Datum nicht sofort uebernommen - wird "
        "jetzt sofort neu gezeichnet\n"
        "- Fix: Der Tipp-Hinweis am FlightAware-Link der Webseite war "
        "unabhaengig von der Geraete-Sprache fest auf Deutsch - ist jetzt "
        "ebenfalls uebersetzt";

    const char* const CHANGELOG_FR =
        "- Nouveau : l'ensemble du site du radar en direct (fenêtre "
        "d'info avion, fenêtre météo, état de connexion, conseil "
        "d'installation PWA, page du carnet de vol, gestion du filtre "
        "compagnies/liste de surveillance) suit désormais entièrement la "
        "langue réglée sur l'appareil\n"
        "- Nouveau : l'altitude/la vitesse/le taux de montée-descente "
        "dans la fenêtre d'info avion du site basculent désormais entre "
        "métrique et impérial, comme sur l'appareil\n"
        "- Nouveau : le site du radar en direct dispose désormais de son "
        "propre panneau de contrôle - le thème de couleur, « Afficher la "
        "météo » et la détection militaire/gouvernementale peuvent être "
        "changés depuis le site, synchronisés avec l'appareil dans les "
        "deux sens\n"
        "- Nouveau : le site du radar en direct affiche désormais une "
        "légende de couleurs pour les avions, correspondant aux couleurs "
        "d'altitude des marqueurs\n"
        "- Correction : un thème de couleur changé via la nouvelle "
        "télécommande web n'était pas repris immédiatement par les "
        "boutons d'en-tête Mode/Menu ni par l'horloge/la date de l'écran "
        "de veille - ils se redessinent maintenant aussitôt\n"
        "- Correction : le message affiché en tapant sur le lien "
        "FlightAware du site restait bloqué en allemand, quelle que soit "
        "la langue de l'appareil - il est désormais traduit aussi";

    const char* const CHANGELOG_TR =
        "- Yeni: Tüm canlı radar web sitesi (uçak bilgi penceresi, hava "
        "durumu penceresi, bağlantı durumu, PWA kurulum ipucu, uçuş "
        "günlüğü sayfası, havayolu filtresi/takip listesi yönetimi) artık "
        "tamamen cihazın dil ayarına bağlı\n"
        "- Yeni: Web sitesindeki uçak bilgi penceresinde irtifa/hız/"
        "tırmanma-alçalma oranı artık cihaz ayarına uygun şekilde metrik "
        "ve imperial arasında geçiş yapıyor\n"
        "- Yeni: Canlı radar web sitesinin artık kendi kontrol alanı var "
        "- renk teması, \"Hava Durumunu Göster\" ve askeri/resmi tespiti "
        "web sitesinden değiştirilebiliyor, cihazla her iki yönde de "
        "senkronize\n"
        "- Yeni: Canlı radar web sitesi artık uçak işaretleri için, "
        "işaretlerin irtifa renklerine uygun bir renk lejantı gösteriyor\n"
        "- Düzeltme: Yeni web uzaktan kumandası üzerinden değiştirilen "
        "bir renk teması, Mode/Menu üst bilgi düğmeleri veya bekleme "
        "ekranı saati/tarihi tarafından hemen uygulanmıyordu - artık "
        "hemen yeniden çiziliyor\n"
        "- Düzeltme: Web sitesindeki FlightAware bağlantısındaki dokunma "
        "ipucu, cihaz dilinden bağımsız olarak Almanca'da sabit "
        "kalıyordu - artık o da çevrildi";

    const char* const CHANGELOG_ES =
        "- Novedad: todo el sitio web de radar en vivo (ventana de "
        "información del avión, ventana del tiempo, estado de conexión, "
        "aviso de instalación PWA, página del diario de vuelo, gestión "
        "del filtro de aerolíneas/lista de vigilancia) ahora sigue "
        "totalmente el idioma configurado en el dispositivo\n"
        "- Novedad: la altitud/velocidad/régimen de ascenso-descenso en "
        "la ventana de información del avión del sitio ahora cambian "
        "entre métrico e imperial, según el ajuste del dispositivo\n"
        "- Novedad: el sitio web de radar en vivo ahora tiene su propia "
        "zona de control - el esquema de color, \"Mostrar clima\" y la "
        "detección militar/gubernamental se pueden cambiar desde el "
        "sitio, sincronizados con el dispositivo en ambas direcciones\n"
        "- Novedad: el sitio web de radar en vivo ahora muestra una "
        "leyenda de colores para los aviones, a juego con los colores de "
        "altitud de los marcadores\n"
        "- Corrección: un esquema de color cambiado mediante el nuevo "
        "control remoto web no se aplicaba de inmediato en los botones "
        "de cabecera Mode/Menu ni en el reloj/fecha del salvapantallas - "
        "ahora se redibujan al instante\n"
        "- Corrección: el aviso al tocar el enlace de FlightAware del "
        "sitio se quedaba fijo en alemán sin importar el idioma del "
        "dispositivo - ahora también está traducido";

    const char* const CHANGELOG_IT =
        "- Novità: l'intero sito del radar live (popup info aereo, popup "
        "meteo, stato della connessione, suggerimento di installazione "
        "PWA, pagina del diario di volo, gestione filtro compagnie/lista "
        "di controllo) ora segue completamente la lingua impostata sul "
        "dispositivo\n"
        "- Novità: altitudine/velocità/rateo di salita-discesa nel popup "
        "aereo del sito ora passano tra metrico e imperiale, in base "
        "all'impostazione del dispositivo\n"
        "- Novità: il sito del radar live ha ora una propria area di "
        "controllo - schema colori, \"Mostra meteo\" e rilevamento "
        "militare/governativo si possono cambiare dal sito, sincronizzati "
        "col dispositivo in entrambe le direzioni\n"
        "- Novità: il sito del radar live ora mostra una legenda colori "
        "per gli aerei, corrispondente ai colori di altitudine dei "
        "marcatori\n"
        "- Correzione: uno schema colori cambiato tramite il nuovo "
        "telecomando web non veniva ripreso subito dai pulsanti "
        "Mode/Menu dell'intestazione né dall'orologio/data dello "
        "screensaver - ora vengono ridisegnati subito\n"
        "- Correzione: il suggerimento al tocco sul link FlightAware del "
        "sito restava fisso in tedesco indipendentemente dalla lingua del "
        "dispositivo - ora è tradotto anche lui";

    const char* const CHANGELOG_PT =
        "- Novo: todo o site de radar ao vivo (pop-up de informações da "
        "aeronave, pop-up do clima, status da conexão, dica de instalação "
        "PWA, página do diário de bordo, gerenciamento de filtro de "
        "companhias/lista de observação) agora segue totalmente o idioma "
        "configurado no dispositivo\n"
        "- Novo: altitude/velocidade/taxa de subida-descida no pop-up da "
        "aeronave do site agora alternam entre métrico e imperial, de "
        "acordo com a configuração do dispositivo\n"
        "- Novo: o site de radar ao vivo agora tem sua própria área de "
        "controle - esquema de cores, \"Mostrar clima\" e detecção "
        "militar/governamental podem ser alterados pelo site, "
        "sincronizados com o dispositivo nos dois sentidos\n"
        "- Novo: o site de radar ao vivo agora mostra uma legenda de "
        "cores para as aeronaves, correspondendo às cores de altitude dos "
        "marcadores\n"
        "- Correção: um esquema de cores alterado pelo novo controle "
        "remoto web não era aplicado de imediato pelos botões de "
        "cabeçalho Mode/Menu nem pelo relógio/data da proteção de tela - "
        "agora são redesenhados na hora\n"
        "- Correção: a dica ao tocar no link do FlightAware no site "
        "ficava presa em alemão, independentemente do idioma do "
        "dispositivo - agora também está traduzida";

    const char* const CHANGELOG_NL =
        "- Nieuw: de hele live-radarwebsite (vliegtuig-infopopup, "
        "weerpopup, verbindingsstatus, PWA-installatietip, "
        "logboekpagina, beheer van airlinefilter/volglijst) volgt nu "
        "volledig de taalinstelling van het apparaat\n"
        "- Nieuw: hoogte/snelheid/klim-daalsnelheid in de "
        "vliegtuig-popup van de website schakelen nu tussen metrisch en "
        "imperiaal, passend bij de apparaatinstelling\n"
        "- Nieuw: de live-radarwebsite heeft nu een eigen "
        "bedieningsgebied - kleurthema, \"Weer tonen\" en militaire/"
        "overheidsdetectie kunnen vanaf de website worden omgeschakeld, "
        "in beide richtingen gesynchroniseerd met het apparaat\n"
        "- Nieuw: de live-radarwebsite toont nu een kleurenlegenda voor "
        "de vliegtuigmarkers, passend bij de hoogtekleuren van de "
        "markers\n"
        "- Fix: een via de nieuwe web-afstandsbediening gewijzigd "
        "kleurthema werd niet meteen overgenomen door de Mode-/Menu-"
        "kopregelknoppen of de klok/datum van de schermbeveiliging - "
        "wordt nu meteen opnieuw getekend\n"
        "- Fix: de tik-hint bij de FlightAware-link op de website stond "
        "vast in het Duits, ongeacht de apparaattaal - is nu ook vertaald";

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
