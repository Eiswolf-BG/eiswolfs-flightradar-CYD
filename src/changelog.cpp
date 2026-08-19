#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: tap the WiFi signal bars in the header to jump straight "
        "to the WiFi settings\n"
        "- New: tap the app title in the header for a QR code linking to "
        "the project's GitHub page";

    const char* const CHANGELOG_DE =
        "- Neu: Antippen der WLAN-Balken im Header fuehrt direkt zu den "
        "WLAN-Einstellungen\n"
        "- Neu: Antippen des App-Titels im Header zeigt einen QR-Code, "
        "der zur GitHub-Seite des Projekts fuehrt";

    const char* const CHANGELOG_FR =
        "- Nouveau : toucher les barres de signal Wi-Fi dans l'en-tête "
        "ouvre directement les réglages Wi-Fi\n"
        "- Nouveau : toucher le titre de l'application dans l'en-tête "
        "affiche un code QR menant à la page GitHub du projet";

    const char* const CHANGELOG_TR =
        "- Yeni: Üst bilgideki WiFi sinyal çubuklarına dokunmak "
        "doğrudan WiFi ayarlarını açar\n"
        "- Yeni: Üst bilgideki uygulama başlığına dokunmak, projenin "
        "GitHub sayfasına götüren bir QR kodu gösterir";

    const char* const CHANGELOG_ES =
        "- Novedad: tocar las barras de señal WiFi en la cabecera abre "
        "directamente los ajustes WiFi\n"
        "- Novedad: tocar el título de la app en la cabecera muestra un "
        "código QR que lleva a la página de GitHub del proyecto";

    const char* const CHANGELOG_IT =
        "- Novità: toccare le barre del segnale WiFi nell'intestazione "
        "apre direttamente le impostazioni WiFi\n"
        "- Novità: toccare il titolo dell'app nell'intestazione mostra "
        "un codice QR che porta alla pagina GitHub del progetto";

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
