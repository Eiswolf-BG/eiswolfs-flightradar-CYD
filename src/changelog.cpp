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
        "- Fix: the clock on the sleep screen could overlap the date "
        "below it, making the date appear corrupted after the display "
        "refreshed";

    const char* const CHANGELOG_DE =
        "- Fix: Beim Aufwachen aus dem Ruhebildschirm mit noch offenem "
        "Flugzeug-Detail-Panel konnten Reste von Uhrzeit/Datum im "
        "Panel-Bereich sichtbar stehen bleiben\n"
        "- Fix: Die Uhrzeit im Ruhebildschirm konnte das darunterliegende "
        "Datum überlappen, wodurch das Datum nach dem Aktualisieren "
        "verzerrt/zusammengeschoben aussah";

    const char* const CHANGELOG_FR =
        "- Correction : au réveil de l'écran de veille alors qu'un "
        "panneau de détails d'avion était encore ouvert, des restes de "
        "l'heure/de la date pouvaient rester visibles dans le panneau\n"
        "- Correction : l'heure affichée sur l'écran de veille pouvait "
        "chevaucher la date en dessous, ce qui déformait l'affichage de "
        "la date après une mise à jour";

    const char* const CHANGELOG_TR =
        "- Düzeltme: Uçak detay paneli açıkken bekleme ekranından "
        "uyanıldığında panel alanında saat/tarih kalıntıları görünür "
        "kalabiliyordu\n"
        "- Düzeltme: Bekleme ekranındaki saat, altındaki tarihle "
        "çakışabiliyordu, bu da ekran güncellendiğinde tarihin bozuk "
        "görünmesine neden oluyordu";

    const char* const CHANGELOG_ES =
        "- Corrección: al despertar de la pantalla de reposo con un "
        "panel de detalles de avión aún abierto, podían quedar restos "
        "de la hora/fecha visibles en el panel\n"
        "- Corrección: la hora en la pantalla de reposo podía "
        "superponerse con la fecha debajo, haciendo que la fecha se "
        "viera distorsionada tras actualizarse";

    const char* const CHANGELOG_IT =
        "- Correzione: risvegliandosi dal salvaschermo con un pannello "
        "dei dettagli dell'aereo ancora aperto, potevano rimanere "
        "residui di ora/data visibili nel pannello\n"
        "- Correzione: l'ora nel salvaschermo poteva sovrapporsi alla "
        "data sottostante, facendo apparire la data distorta dopo "
        "l'aggiornamento";

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
