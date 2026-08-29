#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: manual SSID entry for hidden WiFi networks (during "
        "first-time setup and in WiFi settings)\n"
        "- New: auto-brightness using the built-in light sensor (System "
        "> Display > Brightness), can be turned off\n"
        "- New: animated rain effect on the radar when rain/"
        "thunderstorms are detected, drops fall in the actual wind "
        "direction, can be turned off in the Radar Display menu\n"
        "- New: military/government flight detection (best-effort, "
        "squawk-code based), can be turned off in the Radar Display "
        "menu\n"
        "- Changed: the color theme (Green/Amber/Blue) now applies "
        "system-wide to all menus and text, not just the radar screen\n"
        "- Fix: the aircraft detail panel could get punctured by the "
        "scanline overlay the first time it was opened";

    const char* const CHANGELOG_DE =
        "- Neu: Manuelle SSID-Eingabe für versteckte WLAN-Netzwerke "
        "(bei Ersteinrichtung und in den WLAN-Einstellungen)\n"
        "- Neu: Auto-Helligkeit über den eingebauten Lichtsensor "
        "(System > Anzeige > Helligkeit), abschaltbar\n"
        "- Neu: Animierter Regen-Effekt auf dem Radar bei erkanntem "
        "Regen/Gewitter, Tropfen fallen in tatsächliche Windrichtung, "
        "abschaltbar im Radar-Darstellung-Menü\n"
        "- Neu: Militär-/Behördenflug-Erkennung (Best-Effort, Squawk-"
        "Code-basiert), abschaltbar im Radar-Darstellung-Menü\n"
        "- Geändert: Farbthema (Grün/Amber/Blau) gilt jetzt systemweit "
        "für alle Menüs und Schriften, nicht mehr nur für den "
        "Radarscreen\n"
        "- Fix: Detail-Panel wurde beim ersten Öffnen eines Flugzeugs "
        "vom Scanlinien-Overlay durchlöchert";

    const char* const CHANGELOG_FR =
        "- Nouveau : saisie manuelle du SSID pour les réseaux WiFi "
        "masqués (lors de la configuration initiale et dans les "
        "paramètres WiFi)\n"
        "- Nouveau : luminosité automatique via le capteur de lumière "
        "intégré (Système > Affichage > Luminosité), désactivable\n"
        "- Nouveau : effet de pluie animé sur le radar en cas de pluie/"
        "orage détecté, les gouttes tombent dans la direction réelle "
        "du vent, désactivable dans le menu Affichage radar\n"
        "- Nouveau : détection des vols militaires/gouvernementaux (au "
        "mieux, basée sur le code squawk), désactivable dans le menu "
        "Affichage radar\n"
        "- Modifié : le thème de couleur (Vert/Ambre/Bleu) s'applique "
        "désormais à l'ensemble du système, tous les menus et textes, "
        "plus seulement à l'écran radar\n"
        "- Correction : le panneau de détail de l'avion pouvait être "
        "perforé par la superposition de lignes de balayage lors de sa "
        "première ouverture";

    const char* const CHANGELOG_TR =
        "- Yeni: Gizli WLAN ağları için manuel SSID girişi (ilk "
        "kurulumda ve WLAN ayarlarında)\n"
        "- Yeni: Dahili ışık sensörü üzerinden otomatik parlaklık "
        "(Sistem > Görünüm > Parlaklık), kapatılabilir\n"
        "- Yeni: Radarda algılanan yağmur/fırtınada animasyonlu yağmur "
        "efekti, damlalar gerçek rüzgar yönünde düşer, Radar Görünümü "
        "menüsünden kapatılabilir\n"
        "- Yeni: Askeri/resmi uçuş tespiti (en iyi çaba, squawk koduna "
        "dayalı), Radar Görünümü menüsünden kapatılabilir\n"
        "- Değişti: Renk teması (Yeşil/Amber/Mavi) artık sadece radar "
        "ekranı için değil, tüm sistem genelinde tüm menüler ve "
        "yazılar için geçerli\n"
        "- Düzeltme: uçak detay paneli ilk açıldığında tarama çizgisi "
        "katmanı tarafından deliniyordu";

    const char* const CHANGELOG_ES =
        "- Novedad: entrada manual de SSID para redes WiFi ocultas (en "
        "la configuración inicial y en los ajustes de WiFi)\n"
        "- Novedad: brillo automático mediante el sensor de luz "
        "integrado (Sistema > Pantalla > Brillo), se puede desactivar\n"
        "- Novedad: efecto de lluvia animado en el radar cuando se "
        "detecta lluvia/tormenta, las gotas caen en la dirección real "
        "del viento, se puede desactivar en el menú Visualización de "
        "radar\n"
        "- Novedad: detección de vuelos militares/gubernamentales "
        "(best-effort, basada en código squawk), se puede desactivar "
        "en el menú Visualización de radar\n"
        "- Cambiado: el tema de color (Verde/Ámbar/Azul) ahora se "
        "aplica a todo el sistema, todos los menús y textos, no solo a "
        "la pantalla de radar\n"
        "- Corrección: el panel de detalles de la aeronave podía "
        "quedar perforado por la superposición de líneas de escaneo la "
        "primera vez que se abría";

    const char* const CHANGELOG_IT =
        "- Novità: inserimento manuale dell'SSID per reti WiFi "
        "nascoste (durante la configurazione iniziale e nelle "
        "impostazioni WiFi)\n"
        "- Novità: luminosità automatica tramite il sensore di luce "
        "integrato (Sistema > Schermo > Luminosità), disattivabile\n"
        "- Novità: effetto pioggia animato sul radar quando viene "
        "rilevata pioggia/temporale, le gocce cadono nella direzione "
        "reale del vento, disattivabile nel menu Visualizzazione "
        "radar\n"
        "- Novità: rilevamento voli militari/governativi (best-effort, "
        "basato sul codice squawk), disattivabile nel menu "
        "Visualizzazione radar\n"
        "- Modificato: il tema colore (Verde/Ambra/Blu) ora si applica "
        "a tutto il sistema, tutti i menu e i testi, non solo alla "
        "schermata radar\n"
        "- Correzione: il pannello dettagli del velivolo poteva essere "
        "bucato dalla sovrapposizione di scanline alla prima apertura";

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
