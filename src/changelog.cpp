#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: short weather forecast (temperature + conditions, 3 "
        "hours ahead) in the weather info window\n"
        "- New: bearing (compass direction to the selected aircraft) "
        "shown in the detail panel\n"
        "- Fix: the weather info window now scrolls automatically for "
        "longer text (e.g. a long aviation weather report)\n"
        "- Removed: the photo-toggle on the QR code screen "
        "(planespotters.net proved unreliable)";

    const char* const CHANGELOG_DE =
        "- Neu: Kurzvorhersage (Temperatur + Wetterlage, 3 Stunden "
        "voraus) im Wetter-Info-Fenster\n"
        "- Neu: Peilung (Himmelsrichtung zum ausgewählten Flugzeug) im "
        "Detail-Panel\n"
        "- Fix: Das Wetter-Info-Fenster scrollt jetzt automatisch bei "
        "längeren Texten (z.B. langem Flugwetterbericht)\n"
        "- Entfernt: Die Foto-Umschaltfunktion beim QR-Code "
        "(planespotters.net war unzuverlässig)";

    const char* const CHANGELOG_FR =
        "- Nouveau : courte prévision météo (température + conditions, "
        "dans 3 heures) dans la fenêtre d'info météo\n"
        "- Nouveau : relèvement (direction vers l'avion sélectionné) "
        "affiché dans le panneau de détails\n"
        "- Correction : la fenêtre d'info météo défile désormais "
        "automatiquement pour les textes plus longs (par ex. un long "
        "bulletin météo aéronautique)\n"
        "- Supprimé : le bouton photo sur l'écran du QR code "
        "(planespotters.net s'est révélé peu fiable)";

    const char* const CHANGELOG_TR =
        "- Yeni: Hava durumu bilgi penceresinde kısa vadeli tahmin (3 "
        "saat sonrası için sıcaklık + hava durumu)\n"
        "- Yeni: Detay panelinde seçili uçağa olan yön (pusula yönü) "
        "gösteriliyor\n"
        "- Düzeltme: Hava durumu bilgi penceresi artık uzun metinlerde "
        "(örn. uzun METAR raporu) otomatik olarak kayıyor\n"
        "- Kaldırıldı: QR kod ekranındaki fotoğraf geçiş düğmesi "
        "(planespotters.net güvenilir değildi)";

    const char* const CHANGELOG_ES =
        "- Novedad: previsión meteorológica breve (temperatura + "
        "condición, dentro de 3 horas) en la ventana de información "
        "meteorológica\n"
        "- Novedad: rumbo (dirección hacia el avión seleccionado) en el "
        "panel de detalles\n"
        "- Corrección: la ventana de información meteorológica ahora se "
        "desplaza automáticamente con textos más largos (p. ej. un "
        "informe METAR largo)\n"
        "- Eliminado: el botón de cambio a fotos en la pantalla del "
        "código QR (planespotters.net resultó poco fiable)";

    const char* const CHANGELOG_IT =
        "- Novità: breve previsione meteo (temperatura + condizione, "
        "tra 3 ore) nella finestra informazioni meteo\n"
        "- Novità: rilevamento (direzione verso l'aereo selezionato) "
        "nel pannello dei dettagli\n"
        "- Correzione: la finestra informazioni meteo ora scorre "
        "automaticamente per testi più lunghi (es. un lungo bollettino "
        "METAR)\n"
        "- Rimosso: il pulsante per le foto nella schermata del codice "
        "QR (planespotters.net si è rivelato inaffidabile)";

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
