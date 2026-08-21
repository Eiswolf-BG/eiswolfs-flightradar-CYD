#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: a \"GPS\" button next to \"Auto\" in Location Presets turns "
        "reading from a connected GPS module on/off - when on, your "
        "location follows your movement live instead of only being "
        "estimated via IP (wiring: GPS TX to GPIO22, GPS RX to GPIO27, "
        "9600 baud NMEA - see the \"?\" info screen for details)";

    const char* const CHANGELOG_DE =
        "- Neu: Ein \"GPS\"-Knopf neben \"Automatisch\" in den Standort-"
        "Presets schaltet die Auswertung eines angeschlossenen GPS-Moduls "
        "ein/aus - eingeschaltet folgt dein Standort live deiner Bewegung "
        "statt nur ungefähr per IP geschätzt zu werden (Verkabelung: "
        "GPS-TX an GPIO22, GPS-RX an GPIO27, 9600 Baud NMEA - Details im "
        "\"?\"-Info-Screen)";

    const char* const CHANGELOG_FR =
        "- Nouveau : un bouton \"GPS\" à côté de \"Auto\" dans les "
        "emplacements de position active/désactive la lecture d'un module "
        "GPS connecté - activé, ta position suit tes déplacements en "
        "direct au lieu d'être seulement estimée par IP (câblage : TX GPS "
        "sur GPIO22, RX GPS sur GPIO27, 9600 bauds NMEA - détails dans "
        "l'écran d'info \"?\")";

    const char* const CHANGELOG_TR =
        "- Yeni: Konum Ön Ayarları'nda \"Otomatik\" yanındaki \"GPS\" "
        "düğmesi, bağlı bir GPS modülünün okunmasını açar/kapatır - "
        "açıkken konumun hareketini canlı takip eder, sadece IP üzerinden "
        "tahmini olarak belirlenmez (bağlantı: GPS TX GPIO22'ye, GPS RX "
        "GPIO27'ye, 9600 baud NMEA - detaylar \"?\" bilgi ekranında)";

    const char* const CHANGELOG_ES =
        "- Novedad: un botón \"GPS\" junto a \"Automático\" en Ubicaciones "
        "predefinidas activa/desactiva la lectura de un módulo GPS "
        "conectado - activado, tu ubicación sigue tu movimiento en vivo "
        "en lugar de estimarse solo por IP (cableado: TX GPS a GPIO22, RX "
        "GPS a GPIO27, 9600 baudios NMEA - detalles en la pantalla de "
        "info \"?\")";

    const char* const CHANGELOG_IT =
        "- Novità: un pulsante \"GPS\" accanto ad \"Automatico\" nelle "
        "posizioni preimpostate attiva/disattiva la lettura di un modulo "
        "GPS collegato - attivo, la tua posizione segue il tuo movimento "
        "in tempo reale invece di essere stimata solo via IP "
        "(collegamento: TX GPS su GPIO22, RX GPS su GPIO27, 9600 baud "
        "NMEA - dettagli nella schermata info \"?\")";

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
