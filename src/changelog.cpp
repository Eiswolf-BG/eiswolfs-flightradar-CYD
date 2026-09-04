#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Fix: improved aircraft callsign label positioning - near "
        "the left screen edge, the label could jump to the wrong "
        "side of the marker; for longer callsigns, a visible gap "
        "opened up between label and marker - both fixed, the "
        "label now reliably stays close to its marker";

    const char* const CHANGELOG_DE =
        "- Fix: Rufzeichen-Label-Positionierung weiter verbessert - "
        "bei Flugzeugen nahe dem linken Bildschirmrand konnte das "
        "Label auf die falsche Seite des Markers springen; bei "
        "längeren Rufzeichen entstand außerdem eine sichtbare "
        "Lücke zwischen Label und Marker - beides behoben, Label "
        "bleibt jetzt zuverlässig nah am zugehörigen Marker";

    const char* const CHANGELOG_FR =
        "- Correction : positionnement de l'étiquette d'indicatif "
        "d'appel encore amélioré - près du bord gauche de l'écran, "
        "l'étiquette pouvait basculer du mauvais côté du repère ; "
        "pour les indicatifs plus longs, un écart visible "
        "apparaissait entre l'étiquette et le repère - les deux "
        "corrigés, l'étiquette reste désormais fiablement proche "
        "de son repère";

    const char* const CHANGELOG_TR =
        "- Düzeltme: çağrı işareti etiketi konumlandırması daha da "
        "iyileştirildi - ekranın sol kenarına yakın uçaklarda "
        "etiket işaretin yanlış tarafına atlayabiliyordu; daha "
        "uzun çağrı işaretlerinde etiket ile işaret arasında "
        "gözle görülür bir boşluk oluşuyordu - ikisi de "
        "düzeltildi, etiket artık güvenilir şekilde işaretine "
        "yakın kalıyor";

    const char* const CHANGELOG_ES =
        "- Corrección: se mejoró aún más el posicionamiento de la "
        "etiqueta del indicativo - cerca del borde izquierdo de la "
        "pantalla, la etiqueta podía saltar al lado incorrecto del "
        "marcador; con indicativos más largos aparecía además un "
        "hueco visible entre la etiqueta y el marcador - ambos "
        "corregidos, la etiqueta ahora permanece de forma fiable "
        "cerca de su marcador";

    const char* const CHANGELOG_IT =
        "- Correzione: migliorato ulteriormente il posizionamento "
        "dell'etichetta del nominativo - vicino al bordo sinistro "
        "dello schermo, l'etichetta poteva saltare sul lato "
        "sbagliato del marcatore; con nominativi più lunghi si "
        "creava inoltre uno spazio visibile tra etichetta e "
        "marcatore - entrambi corretti, l'etichetta ora resta in "
        "modo affidabile vicina al proprio marcatore";

    const char* const CHANGELOG_PT =
        "- Correção: posicionamento do rótulo do indicativo "
        "melhorado ainda mais - perto da borda esquerda da tela, o "
        "rótulo podia saltar para o lado errado do marcador; com "
        "indicativos mais longos surgia também um espaço visível "
        "entre o rótulo e o marcador - ambos corrigidos, o rótulo "
        "agora permanece de forma confiável perto do seu marcador";

    const char* const CHANGELOG_NL =
        "- Fix: positionering van het roepnaamlabel verder "
        "verbeterd - bij vliegtuigen dicht bij de linkerrand van "
        "het scherm kon het label naar de verkeerde kant van de "
        "marker springen; bij langere roepnamen ontstond bovendien "
        "een zichtbare kloof tussen label en marker - beide "
        "opgelost, het label blijft nu betrouwbaar dicht bij zijn "
        "marker";

    const char* const TABLE[CHANGELOG_LANG_COUNT] = {
        CHANGELOG_EN, CHANGELOG_DE, CHANGELOG_FR, CHANGELOG_TR, CHANGELOG_ES, CHANGELOG_IT, CHANGELOG_PT, CHANGELOG_NL
    };
}

const char* changelogLatest() {
    uint8_t lang = SettingsStore::language();
    if (lang >= CHANGELOG_LANG_COUNT) lang = 0;
    return TABLE[lang];
}

}
