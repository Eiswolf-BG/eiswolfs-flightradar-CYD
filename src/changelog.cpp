#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: a \"Rotate screen (180°)\" switch in Menu > System > "
        "Display flips the screen (and touch input) upside down - useful "
        "when the device stands on a table, since the panel's viewing "
        "angle otherwise washes out the radar circles when looked at "
        "from above";

    const char* const CHANGELOG_DE =
        "- Neu: Ein Schalter \"Bildschirm drehen (180°)\" im Menü > "
        "System > Anzeige dreht Bild und Touch-Eingabe auf den Kopf - "
        "nützlich, wenn das Gerät auf einem Tisch steht, da der "
        "Blickwinkel des Panels die Radarkreise von oben betrachtet "
        "sonst auswäscht";

    const char* const CHANGELOG_FR =
        "- Nouveau : un interrupteur \"Tourner l'écran (180°)\" dans "
        "Menu > Système > Affichage retourne l'écran (et la saisie "
        "tactile) - utile lorsque l'appareil est posé sur une table, car "
        "l'angle de vue du panneau délave sinon les cercles radar vus "
        "d'en haut";

    const char* const CHANGELOG_TR =
        "- Yeni: Menü > Sistem > Ekran'daki \"Ekranı 180° döndür\" "
        "anahtarı ekranı (ve dokunmatik girişi) ters çevirir - cihaz bir "
        "masa üzerinde dururken kullanışlıdır, çünkü panelin görüş açısı "
        "yukarıdan bakıldığında radar çemberlerini söndüren bir etki "
        "yaratır";

    const char* const CHANGELOG_ES =
        "- Novedad: un interruptor \"Girar pantalla (180°)\" en Menú > "
        "Sistema > Pantalla invierte la pantalla (y la entrada táctil) - "
        "útil cuando el dispositivo está sobre una mesa, ya que el "
        "ángulo de visión del panel visto desde arriba deslava los "
        "círculos del radar";

    const char* const CHANGELOG_IT =
        "- Novità: un interruttore \"Ruota schermo (180°)\" in Menu > "
        "Sistema > Schermo capovolge lo schermo (e l'input touch) - "
        "utile quando il dispositivo è appoggiato su un tavolo, poiché "
        "l'angolo di visione del pannello visto dall'alto sbiadisce i "
        "cerchi del radar";

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
