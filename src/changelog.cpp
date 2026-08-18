#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: the app now quietly checks for firmware updates every "
        "few minutes in the background - a small red dot appears (Menu "
        "button, System tile, sleep screen) when a new version is "
        "available. Installing still always requires your explicit "
        "confirmation - nothing happens automatically.";

    const char* const CHANGELOG_DE =
        "- Neu: Die App prüft jetzt automatisch alle paar Minuten im "
        "Hintergrund auf neue Firmware-Updates - ein kleiner roter "
        "Punkt erscheint (Menu-Button, System-Kachel, Ruhebildschirm), "
        "sobald eine neue Version verfügbar ist. Installiert wird "
        "weiterhin nur nach deiner ausdrücklichen Bestätigung - nichts "
        "passiert automatisch.";

    const char* const CHANGELOG_FR =
        "- Nouveau : l'application vérifie désormais automatiquement, "
        "toutes les quelques minutes en arrière-plan, si une nouvelle "
        "mise à jour du firmware est disponible - un petit point rouge "
        "apparaît (bouton Menu, case Système, écran de veille) dès "
        "qu'une nouvelle version est disponible. L'installation reste "
        "toujours soumise à votre confirmation explicite - rien ne se "
        "passe automatiquement.";

    const char* const CHANGELOG_TR =
        "- Yeni: Uygulama artık arka planda birkaç dakikada bir "
        "otomatik olarak yeni bir bellenim güncellemesi olup olmadığını "
        "kontrol ediyor - yeni bir sürüm mevcut olduğunda küçük kırmızı "
        "bir nokta beliriyor (Menü düğmesi, Sistem kutusu, bekleme "
        "ekranı). Kurulum yine de yalnızca sizin açık onayınızla "
        "yapılır - hiçbir şey otomatik olarak gerçekleşmez.";

    const char* const CHANGELOG_ES =
        "- Nuevo: la app ahora comprueba automáticamente en segundo "
        "plano, cada pocos minutos, si hay una nueva actualización de "
        "firmware - aparece un pequeño punto rojo (botón Menú, casilla "
        "Sistema, salvapantallas) en cuanto hay una nueva versión "
        "disponible. La instalación sigue requiriendo siempre tu "
        "confirmación explícita - nada ocurre automáticamente.";

    const char* const CHANGELOG_IT =
        "- Novità: l'app ora controlla automaticamente in background, "
        "ogni pochi minuti, se è disponibile un nuovo aggiornamento del "
        "firmware - appare un piccolo punto rosso (pulsante Menu, "
        "riquadro Sistema, salvaschermo) non appena è disponibile una "
        "nuova versione. L'installazione richiede comunque sempre la "
        "tua conferma esplicita - nulla avviene automaticamente.";

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
