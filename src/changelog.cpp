#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- Fix: sleep screen no longer flickers - clock and date now "
        "only redraw when they actually change\n"
        "- New: the radar hint line now shows how many aircraft are "
        "currently visible";

    const char* const CHANGELOG_DE =
        "- Fix: Ruhebildschirm flackert nicht mehr - Uhrzeit und Datum "
        "werden nur noch bei tatsächlicher Änderung neu gezeichnet\n"
        "- Neu: Die Hinweiszeile im Radar zeigt jetzt an, wie viele "
        "Flugzeuge gerade sichtbar sind";

    const char* const CHANGELOG_FR =
        "- Correction : l'écran de veille ne clignote plus - l'heure et "
        "la date ne sont redessinées que lorsqu'elles changent "
        "réellement\n"
        "- Nouveau : la ligne d'info du radar indique maintenant combien "
        "d'avions sont actuellement visibles";

    const char* const CHANGELOG_TR =
        "- Düzeltme: Bekleme ekranı artık titremiyor - saat ve tarih "
        "yalnızca gerçekten değiştiğinde yeniden çiziliyor\n"
        "- Yeni: Radar bilgi satırı artık şu anda kaç uçağın görünür "
        "olduğunu gösteriyor";

    const char* const CHANGELOG_ES =
        "- Corrección: la pantalla de reposo ya no parpadea - la hora y "
        "la fecha solo se redibujan cuando cambian realmente\n"
        "- Nuevo: la línea de información del radar ahora muestra "
        "cuántos aviones son visibles actualmente";

    const char* const CHANGELOG_IT =
        "- Correzione: la schermata di riposo non sfarfalla più - ora e "
        "data vengono ridisegnate solo quando cambiano davvero\n"
        "- Novità: la riga informativa del radar ora mostra quanti "
        "aerei sono attualmente visibili";

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
