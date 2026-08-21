#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: the Flight Options and System menus are now "
        "reorganized into a handful of large category buttons (Stats "
        "& Logbook, LED Alerts, Tools, Display) instead of long lists "
        "- easier to find your way around";

    const char* const CHANGELOG_DE =
        "- Neu: Die Menüs \"Flugoptionen\" und \"System\" sind jetzt "
        "in wenige große Kategorie-Buttons (Statistik & Flugbuch, "
        "LED-Alarme, Werkzeuge, Anzeige) statt langer Listen "
        "umstrukturiert - einfacher, sich zurechtzufinden";

    const char* const CHANGELOG_FR =
        "- Nouveau : les menus « Options de vol » et « Système » sont "
        "désormais réorganisés en quelques grands boutons de "
        "catégorie (Statistiques et carnet de vol, Alertes LED, "
        "Outils, Affichage) au lieu de longues listes - plus facile "
        "de s'y retrouver";

    const char* const CHANGELOG_TR =
        "- Yeni: \"Uçuş Seçenekleri\" ve \"Sistem\" menüleri artık "
        "uzun listeler yerine birkaç büyük kategori düğmesine "
        "(İstatistik ve Uçuş Defteri, LED Uyarıları, Araçlar, Ekran) "
        "göre yeniden düzenlendi - gezinmek artık daha kolay";

    const char* const CHANGELOG_ES =
        "- Novedad: los menús \"Opciones de vuelo\" y \"Sistema\" "
        "ahora están reorganizados en unos pocos botones de "
        "categoría grandes (Estadísticas y diario de vuelo, Alertas "
        "LED, Herramientas, Pantalla) en lugar de listas largas - más "
        "fácil de orientarse";

    const char* const CHANGELOG_IT =
        "- Novità: i menu \"Opzioni di volo\" e \"Sistema\" sono ora "
        "riorganizzati in pochi grandi pulsanti di categoria "
        "(Statistiche e diario di volo, Avvisi LED, Strumenti, "
        "Schermo) invece di lunghe liste - più facile orientarsi";

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
