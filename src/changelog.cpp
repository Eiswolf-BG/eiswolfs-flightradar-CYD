#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- Fix: the \"what's new\" screen after an update now appears only "
        "once the device is fully up and running, so tapping OK returns "
        "straight to the live radar instead of a confusing pause";

    const char* const CHANGELOG_DE =
        "- Fix: Der 'Was ist neu'-Screen nach einer Aktualisierung "
        "erscheint jetzt erst, wenn das Geraet vollstaendig hochgefahren "
        "ist - ein Tipp auf OK fuehrt direkt zum Liveradar, statt einer "
        "verwirrenden Pause";

    const char* const CHANGELOG_FR =
        "- Correction : l'écran « nouveautés » après une mise à jour "
        "n'apparaît désormais qu'une fois l'appareil complètement "
        "démarré - toucher OK ramène directement au radar en direct, "
        "sans pause déroutante";

    const char* const CHANGELOG_TR =
        "- Düzeltme: Güncelleme sonrası 'yenilikler' ekranı artık cihaz "
        "tamamen açıldıktan sonra gösteriliyor - OK'a dokunmak kafa "
        "karıştırıcı bir bekleme yerine doğrudan canlı radara dönüyor";

    const char* const CHANGELOG_ES =
        "- Corrección: la pantalla de novedades tras una actualización "
        "ahora aparece solo cuando el dispositivo ya está completamente "
        "iniciado - tocar OK vuelve directo al radar en vivo, sin una "
        "pausa confusa";

    const char* const CHANGELOG_IT =
        "- Correzione: la schermata delle novità dopo un aggiornamento "
        "ora appare solo quando il dispositivo è completamente avviato - "
        "toccando OK si torna direttamente al radar live, senza una "
        "pausa poco chiara";

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
