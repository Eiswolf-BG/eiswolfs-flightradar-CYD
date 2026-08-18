#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- Fix: waking the sleep screen while an aircraft detail panel "
        "was still open could leave leftover clock/date pixels behind "
        "in the panel area\n"
        "- Fix: the background update check now uses much less memory, "
        "which could occasionally cause garbled digits on the sleep "
        "screen";

    const char* const CHANGELOG_DE =
        "- Fix: Beim Aufwachen aus dem Ruhebildschirm mit noch offenem "
        "Flugzeug-Detail-Panel konnten Reste von Uhrzeit/Datum im "
        "Panel-Bereich sichtbar stehen bleiben\n"
        "- Fix: Die Hintergrund-Update-Pruefung braucht jetzt deutlich "
        "weniger Speicher - das konnte gelegentlich verzerrte Ziffern "
        "auf dem Ruhebildschirm verursachen";

    const char* const CHANGELOG_FR =
        "- Correction : au réveil de l'écran de veille alors qu'un "
        "panneau de détails d'avion était encore ouvert, des restes de "
        "l'heure/de la date pouvaient rester visibles dans le panneau\n"
        "- Correction : la vérification des mises à jour en arrière-plan "
        "utilise désormais bien moins de mémoire, ce qui pouvait "
        "occasionnellement déformer les chiffres sur l'écran de veille";

    const char* const CHANGELOG_TR =
        "- Düzeltme: Uçak detay paneli açıkken bekleme ekranından "
        "uyanıldığında panel alanında saat/tarih kalıntıları görünür "
        "kalabiliyordu\n"
        "- Düzeltme: Arka plan güncelleme kontrolü artık çok daha az "
        "bellek kullanıyor - bu, bekleme ekranında zaman zaman "
        "bozuk rakamlara neden olabiliyordu";

    const char* const CHANGELOG_ES =
        "- Corrección: al despertar de la pantalla de reposo con un "
        "panel de detalles de avión aún abierto, podían quedar restos "
        "de la hora/fecha visibles en el panel\n"
        "- Corrección: la comprobación de actualizaciones en segundo "
        "plano ahora usa mucha menos memoria, lo que podía provocar "
        "ocasionalmente dígitos deformados en la pantalla de reposo";

    const char* const CHANGELOG_IT =
        "- Correzione: risvegliandosi dal salvaschermo con un pannello "
        "dei dettagli dell'aereo ancora aperto, potevano rimanere "
        "residui di ora/data visibili nel pannello\n"
        "- Correzione: il controllo aggiornamenti in background ora usa "
        "molta meno memoria, il che poteva occasionalmente causare "
        "cifre distorte sul salvaschermo";

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
