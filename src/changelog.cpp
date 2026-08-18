#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: the Live Radar (web page) range selector now actually "
        "works - a wider range there shows aircraft beyond the device's "
        "own current range setting\n"
        "- New: tap an aircraft on the Live Radar web page for a tracking "
        "link (route, details, photo) via FlightAware\n"
        "- New: the Live Radar web page now follows the device's Metric/"
        "Imperial units setting, plus a starry background like the web "
        "installer\n"
        "- Improved: reliability of the Live Radar web page - no longer "
        "affects the device's own aircraft tracking when left open\n"
        "- Fix: after installing an update, the restart button now "
        "confirms immediately when tapped, instead of a confusing pause";

    const char* const CHANGELOG_DE =
        "- Neu: Der Reichweiten-Waehler im Liveradar (WebUI) funktioniert "
        "jetzt richtig - eine groessere Reichweite zeigt dort auch "
        "Flugzeuge jenseits der aktuellen Geraete-Reichweite\n"
        "- Neu: Ein angetipptes Flugzeug im Liveradar (WebUI) zeigt jetzt "
        "einen Tracking-Link (Route, Details, Foto) ueber FlightAware\n"
        "- Neu: Das Liveradar (WebUI) folgt jetzt der Metrisch/Imperial-"
        "Einstellung des Geraets, dazu ein sternenklarer Hintergrund wie "
        "beim Web-Installer\n"
        "- Verbessert: Zuverlaessigkeit des Liveradars (WebUI) - "
        "beeinflusst die eigene Flugverfolgung des Geraets nicht mehr, "
        "wenn die Seite laenger geoeffnet bleibt\n"
        "- Fix: Nach einer Aktualisierung bestaetigt der Neustart-Button "
        "jetzt sofort beim Antippen, statt einer verwirrenden Pause";

    const char* const CHANGELOG_FR =
        "- Nouveau : le sélecteur de portée du radar en direct (page web) "
        "fonctionne désormais correctement - une portée plus large y "
        "affiche aussi les avions au-delà de la portée réglée sur "
        "l'appareil\n"
        "- Nouveau : toucher un avion sur la page web du radar en direct "
        "affiche un lien de suivi (itinéraire, détails, photo) via "
        "FlightAware\n"
        "- Nouveau : la page web du radar en direct suit désormais le "
        "réglage Métrique/Impérial de l'appareil, avec un fond étoilé "
        "comme celui de l'installateur web\n"
        "- Amélioré : fiabilité de la page web du radar en direct - "
        "n'affecte plus le suivi des avions de l'appareil lorsqu'elle "
        "reste ouverte\n"
        "- Correction : après l'installation d'une mise à jour, le "
        "bouton de redémarrage confirme désormais immédiatement au "
        "toucher, au lieu d'une pause déroutante";

    const char* const CHANGELOG_TR =
        "- Yeni: Canlı Radar (web sayfası) menzil seçici artık doğru "
        "çalışıyor - daha geniş bir menzil, cihazın kendi menzil "
        "ayarının ötesindeki uçakları da gösteriyor\n"
        "- Yeni: Canlı Radar web sayfasında bir uçağa dokunmak artık "
        "FlightAware üzerinden bir takip bağlantısı (rota, detaylar, "
        "fotoğraf) gösteriyor\n"
        "- Yeni: Canlı Radar web sayfası artık cihazın Metrik/İngiliz "
        "birimi ayarını takip ediyor, ayrıca web yükleyicisindeki gibi "
        "yıldızlı bir arka plan var\n"
        "- İyileştirme: Canlı Radar web sayfasının güvenilirliği - sayfa "
        "uzun süre açık kalsa bile cihazın kendi uçak takibini artık "
        "etkilemiyor\n"
        "- Düzeltme: Bir güncelleme yüklendikten sonra, yeniden başlatma "
        "düğmesi artık dokunulduğunda kafa karıştırıcı bir bekleme "
        "yerine hemen onay veriyor";

    const char* const CHANGELOG_ES =
        "- Novedad: el selector de alcance del radar en vivo (página "
        "web) ahora funciona correctamente - un alcance mayor muestra "
        "allí también aviones más allá del alcance configurado "
        "actualmente en el dispositivo\n"
        "- Novedad: tocar un avión en la página web del radar en vivo "
        "muestra ahora un enlace de seguimiento (ruta, detalles, foto) "
        "a través de FlightAware\n"
        "- Novedad: la página web del radar en vivo ahora sigue el "
        "ajuste Métrico/Imperial del dispositivo, además de un fondo "
        "estrellado como el del instalador web\n"
        "- Mejora: fiabilidad de la página web del radar en vivo - ya no "
        "afecta al seguimiento de aviones propio del dispositivo si se "
        "deja abierta un buen rato\n"
        "- Corrección: tras instalar una actualización, el botón de "
        "reinicio confirma ahora de inmediato al tocarlo, en lugar de "
        "una pausa confusa";

    const char* const CHANGELOG_IT =
        "- Novità: il selettore del raggio del radar live (pagina web) "
        "ora funziona correttamente - un raggio più ampio mostra anche "
        "gli aerei oltre il raggio attualmente impostato sul "
        "dispositivo\n"
        "- Novità: toccando un aereo nella pagina web del radar live "
        "appare ora un link di tracciamento (rotta, dettagli, foto) "
        "tramite FlightAware\n"
        "- Novità: la pagina web del radar live ora segue "
        "l'impostazione Metrico/Imperiale del dispositivo, con in più "
        "uno sfondo stellato come quello dell'installer web\n"
        "- Miglioramento: affidabilità della pagina web del radar live "
        "- non influisce più sul tracciamento aerei del dispositivo "
        "stesso se rimane aperta a lungo\n"
        "- Correzione: dopo l'installazione di un aggiornamento, il "
        "pulsante di riavvio conferma ora immediatamente al tocco, "
        "invece di una pausa poco chiara";

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
