#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: optional MQTT interface (Menu > System > Tools) for "
        "smart-home systems like Home Assistant - sends aircraft count, "
        "proximity/watchlist alert status, WiFi signal strength, and "
        "firmware version\n"
        "- New: automatic Home Assistant device discovery (MQTT "
        "Discovery) - all sensors appear automatically as one grouped "
        "device on the Home Assistant dashboard, including online/"
        "offline status\n"
        "- New: legend hint in the aircraft detail panel when an "
        "aircraft is recognized as a military/government flight "
        "(best-effort)\n"
        "- Improved: model and route lookups are noticeably faster "
        "(fixed a timeout bug that made shortened timeouts ineffective, "
        "and optimized the fallback server order)\n"
        "- Fix: the color theme in the aircraft list (Menu > Flight "
        "Options > Lists) now correctly follows the selected color "
        "theme (green/amber/blue) instead of always being green";

    const char* const CHANGELOG_DE =
        "- Neu: Optionale MQTT-Schnittstelle (Menü > System > "
        "Werkzeuge) für Smart-Home-Systeme wie Home Assistant - sendet "
        "Flugzeuganzahl, Näherungs-/Watchlist-Alarmstatus, WLAN-"
        "Signalstärke und Firmware-Version\n"
        "- Neu: Automatische Home-Assistant-Geräteerkennung (MQTT "
        "Discovery) - alle Sensoren erscheinen automatisch als ein "
        "zusammengehöriges Gerät im Home-Assistant-Dashboard, inklusive "
        "Online-/Offline-Status\n"
        "- Neu: Legenden-Hinweis im Flugzeug-Detail-Panel, wenn ein "
        "Flugzeug als Militär-/Behördenflug erkannt wurde (Best-"
        "Effort)\n"
        "- Verbessert: Modell- und Routen-Abfrage deutlich beschleunigt "
        "(u.a. ein Timeout-Bug behoben, der verkürzte Timeouts "
        "unwirksam machte, sowie die Abfragereihenfolge der Fallback-"
        "Server optimiert)\n"
        "- Fix: Farbthema in der Flugzeugliste (Menü > Flugoptionen > "
        "Listen) folgt jetzt korrekt dem gewählten Farbthema (Grün/"
        "Amber/Blau) statt fest Grün zu sein";

    const char* const CHANGELOG_FR =
        "- Nouveau : interface MQTT optionnelle (Menu > Système > "
        "Outils) pour les systèmes domotiques comme Home Assistant - "
        "envoie le nombre d'avions, l'état des alertes de proximité/"
        "liste de surveillance, la force du signal WiFi et la version "
        "du firmware\n"
        "- Nouveau : découverte automatique d'appareil Home Assistant "
        "(MQTT Discovery) - tous les capteurs apparaissent "
        "automatiquement comme un seul appareil groupé dans le tableau "
        "de bord Home Assistant, y compris le statut en ligne/hors "
        "ligne\n"
        "- Nouveau : indication dans le panneau de détail de l'avion "
        "lorsqu'un avion est reconnu comme vol militaire/"
        "gouvernemental (au mieux)\n"
        "- Amélioré : la recherche du modèle et de la route est "
        "nettement plus rapide (correction d'un bug de délai "
        "d'expiration qui rendait les délais raccourcis inefficaces, "
        "et optimisation de l'ordre des serveurs de secours)\n"
        "- Correction : le thème de couleur dans la liste des avions "
        "(Menu > Options de vol > Listes) suit désormais correctement "
        "le thème de couleur sélectionné (vert/ambre/bleu) au lieu "
        "d'être toujours vert";

    const char* const CHANGELOG_TR =
        "- Yeni: Home Assistant gibi akıllı ev sistemleri için isteğe "
        "bağlı MQTT arayüzü (Menü > Sistem > Araçlar) - menzildeki "
        "uçak sayısını, yakınlık/izleme listesi alarm durumunu, WiFi "
        "sinyal gücünü ve ürün yazılımı sürümünü gönderir\n"
        "- Yeni: Otomatik Home Assistant cihaz keşfi (MQTT Discovery) "
        "- tüm sensörler Home Assistant panosunda çevrimiçi/çevrimdışı "
        "durumu dahil olmak üzere otomatik olarak tek bir gruplanmış "
        "cihaz olarak görünür\n"
        "- Yeni: bir uçak askeri/resmi uçuş olarak tanındığında uçak "
        "detay panelinde açıklama ipucu (en iyi çaba)\n"
        "- İyileştirme: Model ve rota sorguları belirgin şekilde "
        "hızlandırıldı (kısaltılmış zaman aşımlarını etkisiz kılan bir "
        "hata düzeltildi ve yedek sunucuların sorgulama sırası "
        "optimize edildi)\n"
        "- Düzeltme: Uçak listesindeki renk teması (Menü > Uçuş "
        "Seçenekleri > Listeler) artık her zaman yeşil olmak yerine "
        "seçilen renk temasını (yeşil/amber/mavi) doğru şekilde takip "
        "ediyor";

    const char* const CHANGELOG_ES =
        "- Novedad: interfaz MQTT opcional (Menú > Sistema > "
        "Herramientas) para sistemas domóticos como Home Assistant - "
        "envía el número de aviones, el estado de las alertas de "
        "proximidad/lista de vigilancia, la intensidad de la señal "
        "WiFi y la versión del firmware\n"
        "- Novedad: detección automática de dispositivo Home Assistant "
        "(MQTT Discovery) - todos los sensores aparecen automáticamente "
        "como un único dispositivo agrupado en el panel de Home "
        "Assistant, incluido el estado en línea/fuera de línea\n"
        "- Novedad: aviso en el panel de detalles del avión cuando se "
        "reconoce un vuelo militar/gubernamental (best-effort)\n"
        "- Mejorado: la búsqueda de modelo y ruta es notablemente más "
        "rápida (se corrigió un error de tiempo de espera que hacía "
        "ineficaces los tiempos de espera acortados, y se optimizó el "
        "orden de los servidores de reserva)\n"
        "- Corrección: el tema de color en la lista de aviones (Menú > "
        "Opciones de vuelo > Listas) ahora sigue correctamente el tema "
        "de color seleccionado (verde/ámbar/azul) en lugar de ser "
        "siempre verde";

    const char* const CHANGELOG_IT =
        "- Novità: interfaccia MQTT opzionale (Menu > Sistema > "
        "Strumenti) per sistemi domotici come Home Assistant - invia "
        "il numero di aerei, lo stato degli allarmi di prossimità/"
        "lista di controllo, l'intensità del segnale WiFi e la "
        "versione del firmware\n"
        "- Novità: rilevamento automatico del dispositivo Home "
        "Assistant (MQTT Discovery) - tutti i sensori appaiono "
        "automaticamente come un unico dispositivo raggruppato nella "
        "dashboard di Home Assistant, incluso lo stato online/"
        "offline\n"
        "- Novità: indicazione nel pannello dettagli dell'aereo quando "
        "un aereo viene riconosciuto come volo militare/governativo "
        "(best-effort)\n"
        "- Migliorato: la ricerca di modello e rotta è notevolmente "
        "più veloce (corretto un bug del timeout che rendeva "
        "inefficaci i timeout ridotti, e ottimizzato l'ordine dei "
        "server di fallback)\n"
        "- Correzione: il tema colore nell'elenco aerei (Menu > "
        "Opzioni di volo > Elenchi) ora segue correttamente il tema "
        "colore selezionato (verde/ambra/blu) invece di essere sempre "
        "verde";

    const char* const TABLE[CHANGELOG_LANG_COUNT] = {
        CHANGELOG_EN, CHANGELOG_DE, CHANGELOG_FR, CHANGELOG_TR, CHANGELOG_ES, CHANGELOG_IT
    };
}

const char* changelogLatest() {
    uint8_t lang = SettingsStore::language();
    if (lang >= CHANGELOG_LANG_COUNT) lang = 0;
    return TABLE[lang];
}

}
