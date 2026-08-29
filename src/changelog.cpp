#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: Squawk Watchlist - store your own squawk codes to trigger "
        "the proximity alert\n"
        "- New: ISS marker - shows the International Space Station as a "
        "special marker when it's passing within your radar range (can "
        "be turned off)\n"
        "- New: flight trail for the selected aircraft - fading trail of "
        "its last few known positions\n"
        "- New: \"Nostalgic\" mode - subtle screen flicker + corner "
        "vignette, toggle in the Radar Display menu\n"
        "- New: scanline overlay across the radar area\n"
        "- New: terminal-style boot sequence on startup\n"
        "- New: \"Modes\" button on the radar screen for quick access to "
        "the Radar Display menu\n"
        "- Improved: the CRT phosphor fade effect now applies to all "
        "marker colors, not just green\n"
        "- Fix: OTA updates could occasionally get stuck on the update "
        "screen";

    const char* const CHANGELOG_DE =
        "- Neu: Squawk-Wachposten - eigene Squawk-Codes hinterlegen, die "
        "den Näherungsalarm auslösen\n"
        "- Neu: ISS-Marker - zeigt die Internationale Raumstation als "
        "Sondermarker, wenn sie gerade innerhalb der Radar-Reichweite "
        "vorbeifliegt (abschaltbar)\n"
        "- Neu: Flugbahn-Trail für das ausgewählte Flugzeug - "
        "verblassende Spur seiner letzten Positionen\n"
        "- Neu: \"Nostalgisch\"-Modus - leichtes Bildschirm-Flackern + "
        "Ecken-Vignette, Schalter im Radar-Darstellung-Menü\n"
        "- Neu: Scanlinien-Overlay über dem Radarbereich\n"
        "- Neu: Terminal-Boot-Sequenz beim Start\n"
        "- Neu: \"Modes\"-Button auf dem Radarscreen für schnellen "
        "Zugriff aufs Radar-Darstellung-Menü\n"
        "- Verbessert: Der CRT-Phosphor-Fade-Effekt gilt jetzt für alle "
        "Marker-Farben, nicht nur Grün\n"
        "- Fix: OTA-Updates blieben gelegentlich auf dem Update-"
        "Bildschirm hängen";

    const char* const CHANGELOG_FR =
        "- Nouveau : liste de surveillance squawk - enregistrez vos "
        "propres codes squawk pour déclencher l'alerte de proximité\n"
        "- Nouveau : marqueur ISS - affiche la Station spatiale "
        "internationale comme marqueur spécial lorsqu'elle passe dans "
        "votre portée radar (désactivable)\n"
        "- Nouveau : traînée de vol pour l'avion sélectionné - trace qui "
        "s'estompe de ses dernières positions connues\n"
        "- Nouveau : mode « nostalgique » - léger scintillement d'écran "
        "+ vignette dans les coins, activable dans le menu Affichage "
        "radar\n"
        "- Nouveau : superposition de lignes de balayage sur la zone "
        "radar\n"
        "- Nouveau : séquence de démarrage façon terminal au lancement\n"
        "- Nouveau : bouton « Modes » sur l'écran radar pour un accès "
        "rapide au menu Affichage radar\n"
        "- Amélioré : l'effet de fondu phosphore CRT s'applique "
        "désormais à toutes les couleurs de marqueurs, pas seulement au "
        "vert\n"
        "- Correction : les mises à jour OTA pouvaient occasionnellement "
        "rester bloquées sur l'écran de mise à jour";

    const char* const CHANGELOG_TR =
        "- Yeni: Squawk izleme listesi - yakınlık uyarısını "
        "tetikleyecek kendi squawk kodlarınızı kaydedin\n"
        "- Yeni: ISS işareti - Uluslararası Uzay İstasyonu radar "
        "menzilinizden geçerken özel bir işaret olarak gösterilir "
        "(kapatılabilir)\n"
        "- Yeni: seçili uçak için uçuş izi - son birkaç bilinen "
        "konumunun solan izi\n"
        "- Yeni: \"Nostaljik\" mod - hafif ekran titremesi + köşe "
        "vinyeti, Radar Görünümü menüsünden açılıp kapatılabilir\n"
        "- Yeni: radar alanı üzerinde tarama çizgisi katmanı\n"
        "- Yeni: başlangıçta terminal tarzı önyükleme dizisi\n"
        "- Yeni: Radar Görünümü menüsüne hızlı erişim için radar "
        "ekranında \"Modes\" düğmesi\n"
        "- İyileştirme: CRT fosfor sönümleme efekti artık sadece "
        "yeşilde değil tüm işaretçi renklerinde geçerli\n"
        "- Düzeltme: OTA güncellemeleri bazen güncelleme ekranında "
        "takılı kalabiliyordu";

    const char* const CHANGELOG_ES =
        "- Novedad: lista de vigilancia de squawk - guarda tus propios "
        "códigos squawk para activar la alerta de proximidad\n"
        "- Novedad: marcador ISS - muestra la Estación Espacial "
        "Internacional como marcador especial cuando pasa dentro de tu "
        "alcance de radar (se puede desactivar)\n"
        "- Novedad: estela de vuelo para el avión seleccionado - rastro "
        "que se desvanece de sus últimas posiciones conocidas\n"
        "- Novedad: modo \"nostálgico\" - ligero parpadeo de pantalla + "
        "viñeta en las esquinas, activable en el menú Visualización de "
        "radar\n"
        "- Novedad: superposición de líneas de escaneo sobre el área de "
        "radar\n"
        "- Novedad: secuencia de arranque estilo terminal al iniciar\n"
        "- Novedad: botón \"Modes\" en la pantalla de radar para acceso "
        "rápido al menú Visualización de radar\n"
        "- Mejorado: el efecto de desvanecimiento fósforo CRT ahora se "
        "aplica a todos los colores de marcador, no solo al verde\n"
        "- Corrección: las actualizaciones OTA podían quedarse "
        "ocasionalmente atascadas en la pantalla de actualización";

    const char* const CHANGELOG_IT =
        "- Novità: lista di controllo squawk - salva i tuoi codici "
        "squawk personali per attivare l'allarme di prossimità\n"
        "- Novità: marcatore ISS - mostra la Stazione Spaziale "
        "Internazionale come marcatore speciale quando passa entro il "
        "raggio del tuo radar (disattivabile)\n"
        "- Novità: scia di volo per l'aereo selezionato - traccia "
        "sfumata delle sue ultime posizioni note\n"
        "- Novità: modalità \"nostalgica\" - leggero sfarfallio dello "
        "schermo + vignetta negli angoli, attivabile nel menu "
        "Visualizzazione radar\n"
        "- Novità: overlay di scanline sull'area radar\n"
        "- Novità: sequenza di avvio in stile terminale\n"
        "- Novità: pulsante \"Modes\" sulla schermata radar per accesso "
        "rapido al menu Visualizzazione radar\n"
        "- Migliorato: l'effetto di dissolvenza fosforo CRT ora si "
        "applica a tutti i colori dei marcatori, non solo al verde\n"
        "- Correzione: gli aggiornamenti OTA potevano occasionalmente "
        "bloccarsi sulla schermata di aggiornamento";

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
