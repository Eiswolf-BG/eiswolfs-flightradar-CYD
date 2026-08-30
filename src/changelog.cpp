#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: the route line in the aircraft detail panel now also "
        "shows which format is currently active (\"(IATA)\"/\"(ICAO)\")\n"
        "- New: on/off switch for the update LED signal (Menu > System, "
        "in the update section) - turn it off if you don't want the LED "
        "to flash magenta three times when an update is available\n"
        "- Improved: completely reworked terminal boot sequence - no "
        "more cut-off text, large centered \"Booting...\" title, lines "
        "appear one after another (split-flap style), more even screen "
        "fill\n"
        "- Fix: the boot sequence no longer appears during first-time "
        "setup, only starting with the first restart after setup is "
        "complete\n"
        "- Improved: README with an updated hero photo and a new photo "
        "gallery page (menus, color themes, alerts, update process)";

    const char* const CHANGELOG_DE =
        "- Neu: Die Routen-Zeile im Flugzeug-Detail-Panel zeigt jetzt "
        "zusaetzlich, welches Format gerade aktiv ist (\"(IATA)\"/"
        "\"(ICAO)\")\n"
        "- Neu: Ein/Aus-Schalter fuer das LED-Update-Signal (Menue > "
        "System, im Update-Bereich) - laesst sich abschalten, falls das "
        "dreimalige Magenta-Blinken bei verfuegbaren Updates nicht "
        "gewuenscht ist\n"
        "- Verbessert: Terminal-Boot-Sequenz komplett ueberarbeitet - "
        "kein abgeschnittener Text mehr, grosser zentrierter "
        "\"Booting...\"-Titel, Zeilen erscheinen nacheinander (Split-"
        "Flap-Stil), gleichmaessigere Bildschirmfuellung\n"
        "- Fix: Boot-Sequenz erscheint nicht mehr waehrend der "
        "Ersteinrichtung, sondern erst ab dem ersten Neustart nach "
        "abgeschlossener Einrichtung\n"
        "- Verbessert: README mit aktualisiertem Hauptfoto und neuer "
        "Foto-Galerie-Seite (Menues, Farbthemen, Alarme, Update-"
        "Vorgang)";

    const char* const CHANGELOG_FR =
        "- Nouveau : la ligne de route dans le panneau de detail de "
        "l'avion indique maintenant aussi le format actuellement actif "
        "(\"(IATA)\"/\"(ICAO)\")\n"
        "- Nouveau : interrupteur pour le signal LED de mise a jour "
        "(Menu > Systeme, dans la section mise a jour) - a desactiver "
        "si vous ne voulez pas que la LED clignote trois fois en "
        "magenta lorsqu'une mise a jour est disponible\n"
        "- Ameliore : sequence de demarrage type terminal entierement "
        "revue - plus de texte coupe, grand titre \"Booting...\" "
        "centre, les lignes apparaissent l'une apres l'autre (style "
        "tableau d'affichage a palettes), remplissage d'ecran plus "
        "equilibre\n"
        "- Correction : la sequence de demarrage n'apparait plus "
        "pendant la configuration initiale, seulement a partir du "
        "premier redemarrage une fois la configuration terminee\n"
        "- Ameliore : README avec une photo principale mise a jour et "
        "une nouvelle page galerie photo (menus, themes de couleur, "
        "alertes, processus de mise a jour)";

    const char* const CHANGELOG_TR =
        "- Yeni: Uçak detay panelindeki rota satırı artık hangi "
        "formatın etkin olduğunu da gösteriyor (\"(IATA)\"/\"(ICAO)\")\n"
        "- Yeni: Güncelleme LED sinyali için açma/kapama düğmesi (Menü "
        "> Sistem, güncelleme bölümünde) - bir güncelleme mevcut "
        "olduğunda LED'in üç kez magenta renginde yanıp sönmesini "
        "istemiyorsanız kapatabilirsiniz\n"
        "- İyileştirme: Terminal önyükleme dizisi baştan sona yeniden "
        "tasarlandı - artık kesilen metin yok, büyük ortalanmış "
        "\"Booting...\" başlığı, satırlar art arda görünüyor (split-"
        "flap tarzı), daha dengeli ekran doluluğu\n"
        "- Düzeltme: Önyükleme dizisi artık ilk kurulum sırasında "
        "görünmüyor, yalnızca kurulum tamamlandıktan sonraki ilk "
        "yeniden başlatmadan itibaren başlıyor\n"
        "- İyileştirme: Güncellenmiş ana fotoğraf ve yeni bir fotoğraf "
        "galerisi sayfası içeren README (menüler, renk temaları, "
        "alarmlar, güncelleme süreci)";

    const char* const CHANGELOG_ES =
        "- Novedad: la línea de ruta en el panel de detalles del avión "
        "ahora también muestra qué formato está activo (\"(IATA)\"/"
        "\"(ICAO)\")\n"
        "- Novedad: interruptor para la señal LED de actualización "
        "(Menú > Sistema, en la sección de actualización) - desactívalo "
        "si no quieres que el LED parpadee tres veces en magenta "
        "cuando haya una actualización disponible\n"
        "- Mejorado: secuencia de arranque tipo terminal completamente "
        "renovada - ya no se corta el texto, título \"Booting...\" "
        "grande y centrado, las líneas aparecen una tras otra (estilo "
        "panel de aeropuerto), llenado de pantalla más equilibrado\n"
        "- Corrección: la secuencia de arranque ya no aparece durante "
        "la configuración inicial, solo a partir del primer reinicio "
        "tras completar la configuración\n"
        "- Mejorado: README con foto principal actualizada y una nueva "
        "página de galería de fotos (menús, temas de color, alertas, "
        "proceso de actualización)";

    const char* const CHANGELOG_IT =
        "- Novità: la riga della rotta nel pannello dettagli aereo ora "
        "mostra anche quale formato è attivo (\"(IATA)\"/\"(ICAO)\")\n"
        "- Novità: interruttore per il segnale LED di aggiornamento "
        "(Menu > Sistema, nella sezione aggiornamento) - disattivalo "
        "se non vuoi che il LED lampeggi tre volte in magenta quando è "
        "disponibile un aggiornamento\n"
        "- Migliorato: sequenza di avvio in stile terminale "
        "completamente rivista - niente più testo tagliato, grande "
        "titolo \"Booting...\" centrato, le righe appaiono una dopo "
        "l'altra (stile split-flap), riempimento schermo più "
        "equilibrato\n"
        "- Correzione: la sequenza di avvio non appare più durante la "
        "configurazione iniziale, solo a partire dal primo riavvio "
        "dopo aver completato la configurazione\n"
        "- Migliorato: README con foto principale aggiornata e nuova "
        "pagina galleria foto (menu, temi colore, allarmi, processo di "
        "aggiornamento)";

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
