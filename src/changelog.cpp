#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- Fix: menus and settings screens now respect the display "
        "timeout again - after 2 minutes without a tap they return "
        "automatically to the radar screen\n"
        "- Fix: occasionally seeing the same aircraft twice on the radar "
        "(duplicate reports from the data source) is now filtered out\n"
        "- Improved: reworked GitHub screen layout";

    const char* const CHANGELOG_DE =
        "- Behoben: Menues und Einstellungsseiten respektieren jetzt "
        "wieder den Bildschirm-Timeout - nach 2 Minuten ohne Antippen "
        "springt es automatisch zum Radarscreen zurueck\n"
        "- Behoben: gelegentlich doppelt angezeigte Flugzeuge auf dem "
        "Radar (doppelte Meldungen der Datenquelle) werden jetzt "
        "herausgefiltert\n"
        "- Verbessert: GitHub-Screen-Layout ueberarbeitet";

    const char* const CHANGELOG_FR =
        "- Corrigé : les menus et écrans de réglages respectent à "
        "nouveau le délai d'extinction de l'écran - après 2 minutes "
        "sans contact, retour automatique à l'écran radar\n"
        "- Corrigé : un même avion parfois affiché deux fois sur le "
        "radar (signalements en double de la source de données) est "
        "maintenant filtré\n"
        "- Amélioré : mise en page de l'écran GitHub revue";

    const char* const CHANGELOG_TR =
        "- Düzeltme: menüler ve ayar ekranları artık ekran zaman "
        "aşımına yeniden uyuyor - 2 dakika dokunulmazsa otomatik "
        "olarak radar ekranına döner\n"
        "- Düzeltme: radarda bazen aynı uçağın iki kez görünmesi (veri "
        "kaynağından gelen yinelenen bildirimler) artık filtreleniyor\n"
        "- İyileştirme: GitHub ekranı düzeni yenilendi";

    const char* const CHANGELOG_ES =
        "- Corregido: los menús y pantallas de ajustes vuelven a "
        "respetar el tiempo de espera de la pantalla - tras 2 minutos "
        "sin tocar, regresan automáticamente a la pantalla del radar\n"
        "- Corregido: ver ocasionalmente el mismo avión dos veces en el "
        "radar (avisos duplicados de la fuente de datos) ahora se "
        "filtra\n"
        "- Mejorado: diseño de la pantalla de GitHub renovado";

    const char* const CHANGELOG_IT =
        "- Corretto: i menu e le schermate delle impostazioni ora "
        "rispettano di nuovo il timeout dello schermo - dopo 2 minuti "
        "senza toccare lo schermo tornano automaticamente al radar\n"
        "- Corretto: vedere occasionalmente lo stesso aereo due volte "
        "sul radar (segnalazioni duplicate della fonte dati) ora viene "
        "filtrato\n"
        "- Migliorato: layout della schermata GitHub rinnovato";

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
