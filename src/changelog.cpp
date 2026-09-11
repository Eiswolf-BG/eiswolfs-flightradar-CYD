#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the live radar website can now play an alert sound in "
        "the browser on watchlist matches or emergency squawks, with its "
        "own mute/volume icon\n"
        "- New: an optional alarm tone can now also sound through the "
        "device's SPK speaker connector on emergency squawks, "
        "independent of the LED alarm\n"
        "- New: wind direction and speed (from METAR) are now shown in "
        "the weather info popup, both on the device and the website\n"
        "- Fix: recurring 600-1600ms stalls every ~16KB while "
        "downloading the firmware during an OTA update, caused by the "
        "ESP32's WiFi modem-sleep power saving - performUpdate() now "
        "disables modem-sleep for the duration of the download and "
        "restores the previous state afterward, regardless of outcome\n"
        "- Fix: the live radar website's logbook entries could go "
        "missing, or the whole page could fail to load, once enough "
        "days were logged - the page response is now streamed in pieces "
        "instead of built as one large buffer, avoiding a hard ~43KB "
        "heap allocation ceiling on this hardware";

    const char* const CHANGELOG_DE =
        "- Neu: Die Live-Radar-Webseite kann jetzt bei Watchlist-"
        "Treffern oder Notfall-Squawks einen Alarmton im Browser "
        "abspielen, mit eigenem Stummschalt-/Lautstaerke-Icon\n"
        "- Neu: Bei Notfall-Squawks kann jetzt optional zusaetzlich ein "
        "Alarmton ueber den SPK-Lautsprecheranschluss des Geraets "
        "ausgegeben werden, unabhaengig vom LED-Alarm\n"
        "- Neu: Windrichtung und -geschwindigkeit (aus METAR) werden "
        "jetzt im Wetter-Infofenster angezeigt, sowohl auf dem Geraet "
        "als auch auf der Webseite\n"
        "- Fix: Behebt wiederkehrende Aussetzer (ca. 600-1600ms alle "
        "~16KB) beim Herunterladen der Firmware waehrend eines OTA-"
        "Updates, verursacht durch den WLAN-Modem-Sleep-Stromsparmodus "
        "des ESP32 - performUpdate() in src/ota_update.cpp schaltet den "
        "Modem-Sleep jetzt fuer die Dauer des Downloads ab und stellt "
        "danach den vorherigen Zustand wieder her, unabhaengig vom "
        "Ergebnis\n"
        "- Fix: Auf der Live-Radar-Webseite konnten Logbuch-Eintraege "
        "fehlen oder die gesamte Seite konnte gar nicht mehr laden, "
        "sobald genug Tage geloggt waren - die Seite wird jetzt in "
        "Stuecken gestreamt statt als ein grosser Puffer aufgebaut, "
        "wodurch eine feste ~43KB-Speichergrenze dieser Hardware "
        "umgangen wird";

    const char* const CHANGELOG_FR =
        "- Nouveau : le site du radar en direct peut désormais jouer un "
        "son d'alerte dans le navigateur lors d'une correspondance de "
        "liste de surveillance ou d'un code squawk d'urgence, avec sa "
        "propre icône de sourdine/volume\n"
        "- Nouveau : une tonalité d'alarme optionnelle peut désormais "
        "aussi retentir via le connecteur haut-parleur SPK de "
        "l'appareil lors d'un code squawk d'urgence, indépendamment de "
        "l'alarme LED\n"
        "- Nouveau : la direction et la vitesse du vent (issues du "
        "METAR) sont désormais affichées dans la fenêtre météo, aussi "
        "bien sur l'appareil que sur le site\n"
        "- Correction : corrige des interruptions récurrentes (environ "
        "600-1600 ms toutes les ~16 Ko) lors du téléchargement du "
        "firmware pendant une mise à jour OTA, causées par le mode "
        "d'économie d'énergie « modem-sleep » Wi-Fi de l'ESP32 - "
        "performUpdate() dans src/ota_update.cpp désactive désormais le "
        "modem-sleep pendant toute la durée du téléchargement et "
        "restaure l'état précédent ensuite, quel que soit le résultat\n"
        "- Correction : sur le site du radar en direct, des entrées du "
        "carnet de vol pouvaient manquer, voire la page entière pouvait "
        "ne plus se charger du tout, une fois suffisamment de jours "
        "enregistrés - la page est désormais diffusée par morceaux au "
        "lieu d'être construite comme un seul grand tampon, ce qui "
        "contourne une limite fixe d'allocation mémoire d'environ "
        "43 Ko sur ce matériel";

    const char* const CHANGELOG_TR =
        "- Yeni: Canlı radar web sitesi artık takip listesi "
        "eşleşmelerinde veya acil durum squawk kodlarında tarayıcıda "
        "bir alarm sesi çalabiliyor, kendi sessize alma/ses simgesiyle\n"
        "- Yeni: Acil durum squawk kodlarında artık isteğe bağlı olarak "
        "cihazın SPK hoparlör bağlantısı üzerinden de bir alarm tonu "
        "çalınabiliyor, LED alarmından bağımsız olarak\n"
        "- Yeni: Rüzgar yönü ve hızı (METAR'dan) artık hem cihazda hem "
        "de web sitesindeki hava durumu bilgi penceresinde gösteriliyor\n"
        "- Düzeltme: OTA güncellemesi sırasında bellenim indirilirken "
        "ESP32'nin Wi-Fi modem uyku güç tasarrufu modundan kaynaklanan, "
        "her ~16KB'de bir tekrarlanan 600-1600ms'lik kesintileri "
        "giderir - src/ota_update.cpp içindeki performUpdate() artık "
        "indirme süresince modem uykusunu kapatıyor ve sonrasında "
        "sonuçtan bağımsız olarak önceki durumu geri yüklüyor\n"
        "- Düzeltme: Canlı radar web sitesinde, yeterince gün "
        "kaydedildiğinde uçuş günlüğü kayıtları eksik olabiliyor hatta "
        "sayfanın tamamı yüklenemeyebiliyordu - sayfa artık tek büyük "
        "bir arabellek olarak oluşturulmak yerine parçalar halinde "
        "akıtılıyor, bu da bu donanımdaki sabit ~43KB bellek ayırma "
        "sınırını aşıyor";

    const char* const CHANGELOG_ES =
        "- Novedad: el sitio web de radar en vivo ahora puede "
        "reproducir un sonido de alerta en el navegador ante "
        "coincidencias de la lista de vigilancia o códigos squawk de "
        "emergencia, con su propio icono de silencio/volumen\n"
        "- Novedad: ante códigos squawk de emergencia, ahora también "
        "puede sonar opcionalmente un tono de alarma a través del "
        "conector de altavoz SPK del dispositivo, independiente de la "
        "alarma LED\n"
        "- Novedad: la dirección y velocidad del viento (del METAR) "
        "ahora se muestran en la ventana de información del tiempo, "
        "tanto en el dispositivo como en el sitio web\n"
        "- Corrección: soluciona interrupciones recurrentes (aprox. "
        "600-1600 ms cada ~16 KB) al descargar el firmware durante una "
        "actualización OTA, causadas por el modo de ahorro de energía "
        "\"modem-sleep\" del Wi-Fi del ESP32 - performUpdate() en "
        "src/ota_update.cpp ahora desactiva el modem-sleep durante toda "
        "la descarga y restaura el estado anterior después, "
        "independientemente del resultado\n"
        "- Corrección: en el sitio web de radar en vivo, podían faltar "
        "entradas del diario de vuelo, o incluso la página entera podía "
        "dejar de cargar, una vez registrados suficientes días - la "
        "página ahora se transmite en fragmentos en lugar de "
        "construirse como un único búfer grande, evitando así un "
        "límite fijo de asignación de memoria de ~43 KB en este "
        "hardware";

    const char* const CHANGELOG_IT =
        "- Novità: il sito del radar live ora può riprodurre un suono "
        "di allerta nel browser in caso di corrispondenza nella lista "
        "di controllo o di squawk di emergenza, con una propria icona "
        "di silenziamento/volume\n"
        "- Novità: in caso di squawk di emergenza, ora può suonare "
        "anche opzionalmente un tono di allarme tramite il connettore "
        "altoparlante SPK del dispositivo, indipendentemente "
        "dall'allarme LED\n"
        "- Novità: la direzione e la velocità del vento (dal METAR) "
        "sono ora mostrate nel popup meteo, sia sul dispositivo che sul "
        "sito web\n"
        "- Correzione: risolve interruzioni ricorrenti (circa "
        "600-1600 ms ogni ~16 KB) durante il download del firmware in "
        "un aggiornamento OTA, causate dalla modalità di risparmio "
        "energetico \"modem-sleep\" del Wi-Fi dell'ESP32 - "
        "performUpdate() in src/ota_update.cpp ora disattiva il "
        "modem-sleep per tutta la durata del download e ripristina lo "
        "stato precedente al termine, indipendentemente dall'esito\n"
        "- Correzione: sul sito del radar live, alcune voci del diario "
        "di volo potevano mancare, o l'intera pagina poteva smettere di "
        "caricarsi, una volta registrati abbastanza giorni - la pagina "
        "ora viene trasmessa in blocchi invece di essere costruita come "
        "un unico grande buffer, aggirando un limite fisso di "
        "allocazione di memoria di circa 43 KB su questo hardware";

    const char* const CHANGELOG_PT =
        "- Novo: o site de radar ao vivo agora pode tocar um som de "
        "alerta no navegador em caso de correspondência na lista de "
        "observação ou squawk de emergência, com seu próprio ícone de "
        "mudo/volume\n"
        "- Novo: em squawks de emergência, agora também pode soar "
        "opcionalmente um tom de alarme pelo conector do alto-falante "
        "SPK do dispositivo, independente do alarme por LED\n"
        "- Novo: a direção e velocidade do vento (do METAR) agora são "
        "exibidas no pop-up de informações do clima, tanto no "
        "dispositivo quanto no site\n"
        "- Correção: corrige interrupções recorrentes (cerca de "
        "600-1600 ms a cada ~16 KB) ao baixar o firmware durante uma "
        "atualização OTA, causadas pelo modo de economia de energia "
        "\"modem-sleep\" do Wi-Fi do ESP32 - performUpdate() em "
        "src/ota_update.cpp agora desativa o modem-sleep durante todo o "
        "download e restaura o estado anterior depois, independentemente "
        "do resultado\n"
        "- Correção: no site de radar ao vivo, entradas do diário de "
        "bordo podiam faltar, ou a página inteira podia parar de "
        "carregar, assim que dias suficientes fossem registrados - a "
        "página agora é transmitida em partes em vez de construída "
        "como um único buffer grande, contornando um limite fixo de "
        "alocação de memória de ~43 KB neste hardware";

    const char* const CHANGELOG_NL =
        "- Nieuw: de live-radarwebsite kan nu een waarschuwingsgeluid "
        "afspelen in de browser bij een volglijsttreffer of "
        "noodsquawk, met een eigen dempings-/volume-icoon\n"
        "- Nieuw: bij noodsquawks kan nu optioneel ook een alarmtoon "
        "klinken via de SPK-luidsprekeraansluiting van het apparaat, "
        "onafhankelijk van het LED-alarm\n"
        "- Nieuw: windrichting en -snelheid (uit METAR) worden nu "
        "getoond in de weer-infopopup, zowel op het apparaat als op de "
        "website\n"
        "- Fix: verhelpt terugkerende onderbrekingen (ca. 600-1600 ms "
        "elke ~16 KB) bij het downloaden van de firmware tijdens een "
        "OTA-update, veroorzaakt door de wifi-modem-sleep-"
        "energiebesparingsmodus van de ESP32 - performUpdate() in "
        "src/ota_update.cpp schakelt de modem-sleep nu uit voor de "
        "duur van de download en herstelt daarna de vorige status, "
        "ongeacht het resultaat\n"
        "- Fix: op de live-radarwebsite konden logboekvermeldingen "
        "ontbreken, of kon de hele pagina helemaal niet meer laden, "
        "zodra er genoeg dagen waren gelogd - de pagina wordt nu in "
        "stukken gestreamd in plaats van als één grote buffer "
        "opgebouwd, waardoor een vaste geheugengrens van ~43 KB op "
        "deze hardware wordt omzeild";

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
