#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: \"Classic Radar\" mode in the Radar Display menu - "
        "comet-tail sweep, extra grid spokes, a sonar-ping ring, and a "
        "brief burst of signal noise when a new aircraft first appears\n"
        "- New: eye icon in the header for quick access to the Display "
        "Filters (ground vehicles/helicopters/low-altitude)\n"
        "- New: \"?\" help buttons right next to CRT Phosphor, Radar "
        "Pulse, and Classic Radar in the Radar Display menu, with a "
        "short explanation of each\n"
        "- Fix: the aircraft detail panel could get punctured by the "
        "scanline overlay the first time it was opened (\"Model: "
        "unknown\"/\"Type: unknown\" showing interference lines)";

    const char* const CHANGELOG_DE =
        "- Neu: \"Klassik-Radar\"-Modus im Radar-Darstellung-Menü - "
        "Kometenschweif-Sweep, zusätzliche Rasterspeichen, Sonar-Ping "
        "und kurzes Signal-Rauschen bei neu auftauchenden Flugzeugen\n"
        "- Neu: Auge-Symbol in der Kopfzeile für schnellen Zugriff auf "
        "die Anzeigefilter (Bodenfahrzeuge/Helikopter/Niedrigflieger)\n"
        "- Neu: \"?\"-Hilfe-Buttons direkt bei CRT-Phosphor, Radar-Puls "
        "und Klassik-Radar im Radar-Darstellung-Menü, mit kurzer "
        "Erklärung der jeweiligen Funktion\n"
        "- Fix: Detail-Panel wurde beim ersten Öffnen eines Flugzeugs "
        "vom Scanlinien-Overlay durchlöchert (\"Modell: unbekannt\"/"
        "\"Typ: unbekannt\" mit Störlinien)";

    const char* const CHANGELOG_FR =
        "- Nouveau : mode « Radar classique » dans le menu Affichage "
        "radar - traînée de comète derrière le balayage, rayons de "
        "grille supplémentaires, anneau sonar et bref bruit de signal "
        "lorsqu'un nouvel avion apparaît\n"
        "- Nouveau : icône œil dans l'en-tête pour un accès rapide aux "
        "Filtres d'affichage (véhicules au sol/hélicoptères/basse "
        "altitude)\n"
        "- Nouveau : boutons d'aide « ? » directement à côté de "
        "Phosphore CRT, Pulsation radar et Radar classique dans le menu "
        "Affichage radar, avec une brève explication de chaque "
        "fonction\n"
        "- Correction : le panneau de détail de l'avion pouvait être "
        "perforé par la superposition de lignes de balayage lors de sa "
        "première ouverture (« Modèle : inconnu »/« Type : inconnu » "
        "avec des lignes parasites)";

    const char* const CHANGELOG_TR =
        "- Yeni: Radar Görünümü menüsünde \"Klasik Radar\" modu - "
        "tarama çizgisinin arkasında kuyruk izi, ek ızgara çizgileri, "
        "sonar halkası ve yeni bir uçak ilk kez göründüğünde kısa bir "
        "sinyal paraziti\n"
        "- Yeni: Görüntü Filtrelerine (yer araçları/helikopterler/alçak "
        "irtifa) hızlı erişim için başlıkta göz simgesi\n"
        "- Yeni: Radar Görünümü menüsünde CRT Fosfor, Radar Nabzı ve "
        "Klasik Radar'ın hemen yanında, her birinin kısa açıklamasını "
        "veren \"?\" yardım düğmeleri\n"
        "- Düzeltme: uçak detay paneli ilk açıldığında tarama çizgisi "
        "katmanı tarafından deliniyordu (\"Model: bilinmiyor\"/\"Tip: "
        "bilinmiyor\" parazit çizgileriyle gösteriliyordu)";

    const char* const CHANGELOG_ES =
        "- Novedad: modo \"Radar clásico\" en el menú Visualización de "
        "radar - estela de cometa detrás del barrido, radios de "
        "cuadrícula adicionales, un anillo de sonar y un breve ruido "
        "de señal cuando aparece una nueva aeronave\n"
        "- Novedad: icono de ojo en la cabecera para acceso rápido a "
        "los Filtros de visualización (vehículos terrestres/"
        "helicópteros/baja altitud)\n"
        "- Novedad: botones de ayuda \"?\" justo junto a Fósforo CRT, "
        "Pulso de radar y Radar clásico en el menú Visualización de "
        "radar, con una breve explicación de cada función\n"
        "- Corrección: el panel de detalles de la aeronave podía "
        "quedar perforado por la superposición de líneas de escaneo la "
        "primera vez que se abría (\"Modelo: desconocido\"/\"Tipo: "
        "desconocido\" con líneas de interferencia)";

    const char* const CHANGELOG_IT =
        "- Novità: modalità \"Radar classico\" nel menu Visualizzazione "
        "radar - scia cometa dietro la linea di scansione, razze "
        "aggiuntive della griglia, un anello sonar e un breve disturbo "
        "del segnale quando appare un nuovo aereo\n"
        "- Novità: icona occhio nell'intestazione per accesso rapido "
        "ai Filtri di visualizzazione (veicoli a terra/elicotteri/"
        "bassa quota)\n"
        "- Novità: pulsanti di aiuto \"?\" direttamente accanto a "
        "Fosforo CRT, Impulso radar e Radar classico nel menu "
        "Visualizzazione radar, con una breve spiegazione di ciascuna "
        "funzione\n"
        "- Correzione: il pannello dettagli del velivolo poteva essere "
        "bucato dalla sovrapposizione di scanline alla prima apertura "
        "(\"Modello: sconosciuto\"/\"Tipo: sconosciuto\" con linee di "
        "disturbo)";

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
