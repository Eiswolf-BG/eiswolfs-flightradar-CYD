#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New sleep screen: big logo, clock, localized date, and version "
        "number, with deeper dimming\n"
        "- New: aircraft photo via QR code (planespotters.net) in the "
        "flight detail panel\n"
        "- Fix: flight logbook now reliably turns off after 24h, even "
        "during ADS-B outages\n"
        "- Fix: header sometimes kept showing stale text after leaving "
        "the QR code screen";

    const char* const CHANGELOG_DE =
        "- Neuer Ruhebildschirm: großes Logo, Uhrzeit, Datum (im "
        "jeweiligen Sprachformat) und Versionsnummer, mit tieferer "
        "Dimmung\n"
        "- Neu: Flugzeug-Foto per QR-Code (planespotters.net) im "
        "Flugzeug-Detail-Panel\n"
        "- Fix: Flugbuch schaltet sich jetzt zuverlässig nach 24h "
        "automatisch ab, auch bei ADS-B-Ausfällen\n"
        "- Fix: Kopfzeile blieb nach dem QR-Code-Screen manchmal "
        "fehlerhaft stehen";

    const char* const CHANGELOG_FR =
        "- Nouvel écran de veille : grand logo, heure, date (au format "
        "local) et numéro de version, avec un assombrissement plus "
        "prononcé\n"
        "- Nouveau : photo de l'avion par code QR (planespotters.net) "
        "dans le panneau de détails du vol\n"
        "- Correction : le carnet de vol se désactive maintenant de "
        "façon fiable après 24h, même en cas de panne ADS-B\n"
        "- Correction : l'en-tête affichait parfois un texte obsolète "
        "après l'écran du code QR";

    const char* const CHANGELOG_TR =
        "- Yeni bekleme ekranı: büyük logo, saat, yerel biçimde tarih ve "
        "sürüm numarası, daha derin karartma ile\n"
        "- Yeni: Uçak detay panelinde QR kod ile uçak fotoğrafı "
        "(planespotters.net)\n"
        "- Düzeltme: Uçuş defteri artık ADS-B kesintilerinde bile 24 "
        "saat sonra güvenilir şekilde kapanıyor\n"
        "- Düzeltme: QR kod ekranından dönüldüğünde üst bilgi bazen "
        "eski metni göstermeye devam ediyordu";

    const char* const CHANGELOG_ES =
        "- Nueva pantalla de reposo: logo grande, hora, fecha (en "
        "formato local) y número de versión, con un atenuado más "
        "profundo\n"
        "- Nuevo: foto del avión mediante código QR (planespotters.net) "
        "en el panel de detalles del vuelo\n"
        "- Corrección: el diario de vuelo ahora se desactiva de forma "
        "fiable tras 24h, incluso durante cortes de ADS-B\n"
        "- Corrección: el encabezado a veces seguía mostrando texto "
        "obsoleto tras salir de la pantalla del código QR";

    const char* const CHANGELOG_IT =
        "- Nuova schermata di riposo: logo grande, ora, data (nel "
        "formato locale) e numero di versione, con un'oscurità più "
        "profonda\n"
        "- Novità: foto dell'aereo tramite codice QR (planespotters.net) "
        "nel pannello dei dettagli del volo\n"
        "- Correzione: il diario di volo ora si disattiva in modo "
        "affidabile dopo 24h, anche durante le interruzioni ADS-B\n"
        "- Correzione: l'intestazione a volte continuava a mostrare "
        "testo obsoleto dopo la schermata del codice QR";

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
