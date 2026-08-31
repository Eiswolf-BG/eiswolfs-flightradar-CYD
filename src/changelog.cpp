#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- Changed: the \"Overlay\" switch (Mode menu) is now named "
        "\"Overlay\" consistently in all languages and controls all "
        "three optional corner displays (airport/weather/event "
        "rotation) together, instead of only the event corner\n"
        "- Changed: military/government detection was removed from "
        "the event rotation (bottom-right corner) - still visible via "
        "the marker ring and the detail panel line\n"
        "- Removed: the \"Watchlist alert\" switch - a watchlist match "
        "now always triggers the alarm automatically, no separate "
        "setting needed\n"
        "- Changed: removed the \"Tools\" menu entry - location "
        "presets are now directly reachable from the Flight Options "
        "main menu\n"
        "- New: rain effect now also on the screensaver (vertical, no "
        "wind direction) and in the web live radar (with wind "
        "direction, like the radar screen), depending on the rain "
        "setting\n"
        "- New: rain intensity (drop count/speed) now follows the "
        "actual rain strength from the weather data (light/moderate/"
        "heavy)\n"
        "- Improved: the altitude color legend now also explains the "
        "military/government ring and the Heavy aircraft symbol\n"
        "- Fix: rain on the screensaver used to sit below the logo/"
        "clock instead of above it";

    const char* const CHANGELOG_DE =
        "- Geaendert: \"Overlay\"-Schalter (Mode-Menue) heisst jetzt "
        "einheitlich \"Overlay\" und steuert alle drei optionalen "
        "Eck-Anzeigen (Flughafen/Wetter/Ereignis-Rotation) gemeinsam, "
        "statt nur die Ereignis-Ecke\n"
        "- Geaendert: Militaer-/Behoerdenerkennung aus der Ereignis-"
        "Rotation (untere rechte Ecke) entfernt - bleibt weiterhin "
        "ueber den Ring am Marker und die Detail-Panel-Zeile "
        "sichtbar\n"
        "- Entfernt: \"Beobachtungsalarm\"-Schalter - ein Watchlist-"
        "Treffer loest jetzt immer automatisch den Alarm aus\n"
        "- Geaendert: \"Werkzeuge\"-Menuepunkt entfernt - Standort-"
        "Presets sind jetzt direkt im Flugoptionen-Hauptmenue "
        "erreichbar\n"
        "- Neu: Regen-Effekt jetzt auch auf dem Ruhebildschirm "
        "(senkrecht, ohne Windrichtung) und im Web-Live-Radar (mit "
        "Windrichtung, wie der Radarscreen)\n"
        "- Neu: Regen-Intensitaet (Tropfenanzahl/Geschwindigkeit) "
        "richtet sich jetzt nach der tatsaechlichen Regenstaerke aus "
        "den Wetterdaten (leicht/mittel/stark)\n"
        "- Verbessert: Hoehen-Farben-Legende erklaert jetzt auch den "
        "Militaer-/Behoerden-Ring und das Heavy-Flugzeug-Symbol\n"
        "- Fix: Regen auf dem Ruhebildschirm lag vorher unter Logo/"
        "Uhrzeit statt darueber";

    const char* const CHANGELOG_FR =
        "- Modifie : l'interrupteur \"Overlay\" (menu Mode) s'appelle "
        "maintenant \"Overlay\" dans toutes les langues et controle "
        "ensemble les trois affichages de coin optionnels (aeroport/"
        "meteo/rotation d'evenements), au lieu du seul coin "
        "evenements\n"
        "- Modifie : la detection militaire/gouvernementale a ete "
        "retiree de la rotation d'evenements (coin inferieur droit) - "
        "reste visible via l'anneau du marqueur et la ligne du panneau "
        "de details\n"
        "- Supprime : l'interrupteur \"Alerte de surveillance\" - une "
        "correspondance de liste de surveillance declenche desormais "
        "toujours automatiquement l'alarme\n"
        "- Modifie : suppression du menu \"Outils\" - les presets de "
        "localisation sont desormais directement accessibles depuis "
        "le menu principal Options de vol\n"
        "- Nouveau : effet de pluie desormais aussi sur l'ecran de "
        "veille (vertical, sans direction du vent) et dans le radar "
        "web en direct (avec direction du vent, comme l'ecran "
        "radar)\n"
        "- Nouveau : l'intensite de la pluie (nombre de gouttes/"
        "vitesse) suit desormais l'intensite reelle des precipitations "
        "des donnees meteo (legere/moyenne/forte)\n"
        "- Ameliore : la legende des couleurs d'altitude explique "
        "desormais aussi l'anneau militaire/gouvernemental et le "
        "symbole d'avion Heavy\n"
        "- Correction : la pluie sur l'ecran de veille se trouvait "
        "auparavant sous le logo/l'heure au lieu d'etre au-dessus";

    const char* const CHANGELOG_TR =
        "- Değiştirildi: \"Overlay\" düğmesi (Mode menüsü) artık tüm "
        "dillerde tutarlı şekilde \"Overlay\" olarak adlandırılıyor ve "
        "yalnızca olay köşesi yerine üç isteğe bağlı köşe göstergesini "
        "(havalimanı/hava durumu/olay dönüşü) birlikte kontrol "
        "ediyor\n"
        "- Değiştirildi: askeri/resmi tespiti olay dönüşünden (sağ alt "
        "köşe) kaldırıldı - işaretçideki halka ve detay panel satırı "
        "üzerinden görünmeye devam ediyor\n"
        "- Kaldırıldı: \"İzleme uyarısı\" düğmesi - bir izleme listesi "
        "eşleşmesi artık her zaman otomatik olarak alarmı tetikliyor\n"
        "- Değiştirildi: \"Araçlar\" menü öğesi kaldırıldı - konum ön "
        "ayarlarına artık doğrudan Uçuş Seçenekleri ana menüsünden "
        "ulaşılabiliyor\n"
        "- Yeni: yağmur efekti artık ekran koruyucuda da (dikey, "
        "rüzgar yönü olmadan) ve web canlı radarında da (rüzgar "
        "yönüyle, radar ekranı gibi) mevcut\n"
        "- Yeni: yağmur yoğunluğu (damla sayısı/hızı) artık hava "
        "durumu verilerindeki gerçek yağmur şiddetine göre "
        "ayarlanıyor (hafif/orta/şiddetli)\n"
        "- İyileştirme: yükseklik renk lejantı artık askeri/resmi "
        "halkasını ve Heavy uçak simgesini de açıklıyor\n"
        "- Düzeltme: ekran koruyucudaki yağmur önceden logo/saatin "
        "üstünde değil altında görünüyordu";

    const char* const CHANGELOG_ES =
        "- Cambiado: el interruptor \"Overlay\" (menú Modo) ahora se "
        "llama \"Overlay\" de forma coherente en todos los idiomas y "
        "controla juntas las tres indicaciones de esquina opcionales "
        "(aeropuerto/clima/rotación de eventos), en lugar de solo la "
        "esquina de eventos\n"
        "- Cambiado: se eliminó la detección militar/gubernamental de "
        "la rotación de eventos (esquina inferior derecha) - sigue "
        "siendo visible mediante el anillo del marcador y la línea "
        "del panel de detalles\n"
        "- Eliminado: el interruptor \"Alerta de seguimiento\" - una "
        "coincidencia de la lista de seguimiento ahora siempre activa "
        "la alarma automáticamente\n"
        "- Cambiado: se eliminó el elemento de menú \"Herramientas\" - "
        "los presets de ubicación ahora son accesibles directamente "
        "desde el menú principal de Opciones de vuelo\n"
        "- Novedad: efecto de lluvia ahora también en el salvapantallas "
        "(vertical, sin dirección del viento) y en el radar web en "
        "vivo (con dirección del viento, como la pantalla del radar)\n"
        "- Novedad: la intensidad de la lluvia (cantidad de gotas/"
        "velocidad) ahora se basa en la intensidad real de la lluvia "
        "según los datos meteorológicos (ligera/moderada/fuerte)\n"
        "- Mejorado: la leyenda de colores de altitud ahora también "
        "explica el anillo militar/gubernamental y el símbolo de "
        "avión Heavy\n"
        "- Corrección: la lluvia en el salvapantallas antes aparecía "
        "debajo del logo/reloj en lugar de encima";

    const char* const CHANGELOG_IT =
        "- Modificato: l'interruttore \"Overlay\" (menu Modalità) ora "
        "si chiama \"Overlay\" in modo coerente in tutte le lingue e "
        "controlla insieme tutti e tre gli indicatori d'angolo "
        "opzionali (aeroporto/meteo/rotazione eventi), invece del "
        "solo angolo eventi\n"
        "- Modificato: il rilevamento militare/governativo è stato "
        "rimosso dalla rotazione eventi (angolo in basso a destra) - "
        "resta visibile tramite l'anello del marcatore e la riga del "
        "pannello dettagli\n"
        "- Rimosso: l'interruttore \"Allarme di controllo\" - una "
        "corrispondenza della lista di controllo ora attiva sempre "
        "automaticamente l'allarme\n"
        "- Modificato: rimossa la voce di menu \"Strumenti\" - i "
        "preset di posizione sono ora accessibili direttamente dal "
        "menu principale Opzioni di volo\n"
        "- Novità: effetto pioggia ora anche sullo screensaver "
        "(verticale, senza direzione del vento) e nel radar live web "
        "(con direzione del vento, come la schermata radar)\n"
        "- Novità: l'intensità della pioggia (numero di gocce/"
        "velocità) ora segue l'intensità reale della pioggia dai dati "
        "meteo (leggera/moderata/forte)\n"
        "- Migliorato: la legenda dei colori altitudine ora spiega "
        "anche l'anello militare/governativo e il simbolo dell'aereo "
        "Heavy\n"
        "- Correzione: la pioggia sullo screensaver prima si trovava "
        "sotto il logo/orologio invece che sopra";

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
