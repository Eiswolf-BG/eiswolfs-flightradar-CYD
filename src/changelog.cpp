#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the OTA update screens (installing progress and success) "
        "now show a small logo at the bottom\n"
        "- Fix: the OTA installing-progress screen and the post-update "
        "success screen no longer flicker (this fix lives in the new "
        "firmware, so this very update may still flicker - it takes "
        "effect starting with your next update)";

    const char* const CHANGELOG_DE =
        "- Neu: Die OTA-Update-Bildschirme (Installation und Erfolg) "
        "zeigen jetzt unten ein kleines Logo\n"
        "- Fix: Der Installations-Fortschrittsbildschirm und der "
        "Erfolgsbildschirm nach dem Update flackern nicht mehr (der Fix "
        "steckt in der NEUEN Firmware - dieses eine Update hier kann "
        "daher noch flackern, erst ab dem naechsten Update wirkt er)";

    const char* const CHANGELOG_FR =
        "- Nouveau : les écrans de mise à jour OTA (installation et "
        "succès) affichent désormais un petit logo en bas\n"
        "- Correction : l'écran de progression de l'installation et "
        "l'écran de succès après la mise à jour ne clignotent plus (ce "
        "correctif se trouve dans le nouveau firmware - cette mise à "
        "jour elle-même peut donc encore clignoter, l'effet ne se fera "
        "sentir qu'à partir de la prochaine mise à jour)";

    const char* const CHANGELOG_TR =
        "- Yeni: OTA güncelleme ekranları (yükleme ve başarı) artık "
        "altta küçük bir logo gösteriyor\n"
        "- Düzeltme: Yükleme ilerleme ekranı ve güncelleme sonrası "
        "başarı ekranı artık titremiyor (bu düzeltme yeni ürün "
        "yazılımında yer alıyor - bu nedenle şu anki güncellemenin "
        "kendisi hâlâ titreyebilir, etkisi ancak bir sonraki "
        "güncellemeden itibaren görülür)";

    const char* const CHANGELOG_ES =
        "- Novedad: las pantallas de actualización OTA (instalación y "
        "éxito) ahora muestran un pequeño logotipo abajo\n"
        "- Corrección: la pantalla de progreso de instalación y la "
        "pantalla de éxito tras la actualización ya no parpadean (esta "
        "corrección está en el nuevo firmware, así que esta misma "
        "actualización aún puede parpadear - surtirá efecto a partir de "
        "la siguiente actualización)";

    const char* const CHANGELOG_IT =
        "- Novità: le schermate di aggiornamento OTA (installazione e "
        "successo) ora mostrano un piccolo logo in basso\n"
        "- Correzione: la schermata di avanzamento dell'installazione e "
        "quella di successo dopo l'aggiornamento non sfarfallano più "
        "(questa correzione è nel nuovo firmware, quindi questo stesso "
        "aggiornamento potrebbe ancora sfarfallare - avrà effetto a "
        "partire dal prossimo aggiornamento)";

    const char* const CHANGELOG_PT =
        "- Novo: as telas de atualização OTA (instalação e sucesso) "
        "agora mostram um pequeno logotipo na parte inferior\n"
        "- Correção: a tela de progresso da instalação e a tela de "
        "sucesso após a atualização não piscam mais (essa correção está "
        "no novo firmware, então esta própria atualização ainda pode "
        "piscar - o efeito só vale a partir da próxima atualização)";

    const char* const CHANGELOG_NL =
        "- Nieuw: de OTA-update-schermen (installatie en succes) tonen "
        "nu onderaan een klein logo\n"
        "- Fix: het installatie-voortgangsscherm en het successcherm na "
        "de update flikkeren niet meer (deze fix zit in de NIEUWE "
        "firmware - deze update zelf kan dus nog flikkeren, pas vanaf de "
        "volgende update werkt de fix)";

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
