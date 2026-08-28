#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: switch IATA/ICAO airport codes directly from the aircraft "
        "detail panel (tap the route line), in addition to the Units menu\n"
        "- New: \"Show low-altitude aircraft only\" filter\n"
        "- New: the empty-sky message now shows which filters are active "
        "and is tappable - jumps straight to the filter menu\n"
        "- Changed: reorganized menu - new \"Lists\" submenu (aircraft "
        "list, watchlist), new \"Display Filters\" submenu (airline "
        "filter, ground vehicles, helicopters only, low altitude only), "
        "\"Tools\" now only has location presets and watchlist alert\n"
        "- Changed: radar display style (CRT pulse effect) moved to the "
        "top of the Display menu, now on by default on new/reset devices\n"
        "- Fix: the heartbeat LED could occasionally stay silently off "
        "due to a race condition between the two CPU cores\n"
        "- Fix: the heartbeat now blinks through briefly (white flash) "
        "even during an active proximity/watchlist alert, instead of "
        "being fully suppressed - emergency alerts still take absolute "
        "priority\n"
        "- Improved: more robust ADS-B request behavior (backs off on "
        "rate limits, respects Retry-After)";

    const char* const CHANGELOG_DE =
        "- Neu: IATA/ICAO-Flughafencodes jetzt auch direkt im Flugzeug-"
        "Detail-Panel umschaltbar (Tap auf die Routenzeile), zusätzlich "
        "zum Einheiten-Menü\n"
        "- Neu: Filter \"Nur niedrig fliegende Flugzeuge anzeigen\"\n"
        "- Neu: Der Hinweis bei leerem Radar zeigt jetzt an, welche "
        "Filter aktiv sind, und ist antippbar - springt direkt ins "
        "Filter-Menü\n"
        "- Geändert: Menüstruktur überarbeitet - neues \"Listen\"-"
        "Untermenü (Flugzeugliste, Beobachtungsliste), neues "
        "\"Anzeigefilter\"-Untermenü (Airline-Filter, Bodenfahrzeuge, "
        "Nur Helikopter, Nur Niedrigflieger), \"Werkzeuge\" enthält jetzt "
        "nur noch Standort-Presets und Beobachtungsalarm\n"
        "- Geändert: Radar-Darstellung (CRT-Puls-Effekt) jetzt oben im "
        "Anzeige-Menü, standardmäßig aktiviert bei neuen/zurückgesetzten "
        "Geräten\n"
        "- Fix: Die Heartbeat-LED konnte durch eine Race Condition "
        "zwischen den beiden CPU-Kernen gelegentlich stumm ausbleiben\n"
        "- Fix: Der Heartbeat blinkt jetzt auch bei aktivem Näherungs-/"
        "Beobachtungsalarm kurz sichtbar durch (weißer Blitz), statt "
        "komplett unterdrückt zu werden - Notfall-Alarm hat weiterhin "
        "absolute Priorität\n"
        "- Verbessert: Robusteres Abfrageverhalten gegenüber der ADS-B-"
        "Datenquelle (Backoff bei Rate-Limits, respektiert Retry-After)";

    const char* const CHANGELOG_FR =
        "- Nouveau : basculer les codes aéroport IATA/OACI directement "
        "depuis le panneau de détail de l'avion (tap sur la ligne de "
        "route), en plus du menu Unités\n"
        "- Nouveau : filtre \"Afficher uniquement les avions à basse "
        "altitude\"\n"
        "- Nouveau : le message ciel vide indique désormais quels "
        "filtres sont actifs et est tactile - accès direct au menu de "
        "filtres\n"
        "- Modifié : réorganisation des menus - nouveau sous-menu "
        "\"Listes\" (liste des avions, liste de surveillance), nouveau "
        "sous-menu \"Filtres d'affichage\" (filtre compagnie, véhicules "
        "au sol, hélicoptères uniquement, basse altitude uniquement), "
        "\"Outils\" ne contient plus que les lieux enregistrés et "
        "l'alerte de liste de surveillance\n"
        "- Modifié : le style d'affichage radar (effet impulsion CRT) "
        "déplacé en haut du menu Affichage, activé par défaut sur les "
        "appareils neufs/réinitialisés\n"
        "- Correction : la LED de battement de cœur pouvait "
        "occasionnellement rester éteinte à cause d'une situation de "
        "compétition entre les deux cœurs du processeur\n"
        "- Correction : le battement de cœur clignote désormais "
        "brièvement (flash blanc) même pendant une alerte de proximité/"
        "liste de surveillance active, au lieu d'être totalement "
        "supprimé - l'alerte d'urgence garde une priorité absolue\n"
        "- Amélioré : comportement de requête plus robuste envers la "
        "source de données ADS-B (ralentissement en cas de limite de "
        "débit, respecte Retry-After)";

    const char* const CHANGELOG_TR =
        "- Yeni: IATA/ICAO havalimanı kodları artık uçak detay "
        "panelinden de doğrudan değiştirilebiliyor (rota satırına "
        "dokun), Birimler menüsüne ek olarak\n"
        "- Yeni: \"Sadece alçak irtifada uçan uçakları göster\" filtresi\n"
        "- Yeni: Boş gökyüzü mesajı artık hangi filtrelerin aktif "
        "olduğunu gösteriyor ve dokunulabilir - doğrudan filtre "
        "menüsüne atlıyor\n"
        "- Değişti: Menü yapısı yeniden düzenlendi - yeni \"Listeler\" "
        "alt menüsü (uçak listesi, izleme listesi), yeni \"Görüntü "
        "Filtreleri\" alt menüsü (havayolu filtresi, kara araçları, "
        "sadece helikopterler, sadece alçak irtifa), \"Araçlar\" artık "
        "yalnızca konum ön ayarlarını ve izleme listesi uyarısını "
        "içeriyor\n"
        "- Değişti: Radar görünümü (CRT nabız efekti) Ekran menüsünün "
        "en üstüne taşındı, yeni/sıfırlanmış cihazlarda artık "
        "varsayılan olarak açık\n"
        "- Düzeltme: Kalp atışı LED'i, iki CPU çekirdeği arasındaki bir "
        "yarış durumu nedeniyle bazen sessizce kapalı kalabiliyordu\n"
        "- Düzeltme: Kalp atışı artık aktif bir yakınlık/izleme "
        "uyarısı sırasında da kısaca (beyaz flaş) görünüyor, tamamen "
        "bastırılmak yerine - acil durum uyarısı hâlâ mutlak önceliğe "
        "sahip\n"
        "- İyileştirme: ADS-B veri kaynağına karşı daha sağlam istek "
        "davranışı (hız sınırlarında yavaşlama, Retry-After'a uyum)";

    const char* const CHANGELOG_ES =
        "- Novedad: cambiar entre códigos de aeropuerto IATA/ICAO "
        "directamente desde el panel de detalles de la aeronave (toca "
        "la línea de ruta), además del menú Unidades\n"
        "- Novedad: filtro \"Mostrar solo aviones a baja altitud\"\n"
        "- Novedad: el mensaje de cielo vacío ahora muestra qué filtros "
        "están activos y se puede tocar - salta directamente al menú "
        "de filtros\n"
        "- Cambiado: menús reorganizados - nuevo submenú \"Listas\" "
        "(lista de aeronaves, lista de seguimiento), nuevo submenú "
        "\"Filtros de visualización\" (filtro de aerolínea, vehículos "
        "terrestres, solo helicópteros, solo baja altitud), "
        "\"Herramientas\" ahora solo contiene ubicaciones guardadas y "
        "alerta de lista de seguimiento\n"
        "- Cambiado: el estilo de visualización del radar (efecto "
        "pulso CRT) se movió a la parte superior del menú Pantalla, "
        "activado por defecto en dispositivos nuevos/restablecidos\n"
        "- Corrección: el LED de latido podía quedarse apagado "
        "silenciosamente en ocasiones debido a una condición de "
        "carrera entre los dos núcleos de la CPU\n"
        "- Corrección: el latido ahora parpadea brevemente (destello "
        "blanco) incluso durante una alerta de proximidad/lista de "
        "seguimiento activa, en vez de suprimirse por completo - la "
        "alerta de emergencia sigue teniendo prioridad absoluta\n"
        "- Mejorado: comportamiento de solicitud más robusto frente a "
        "la fuente de datos ADS-B (retroceso ante límites de tasa, "
        "respeta Retry-After)";

    const char* const CHANGELOG_IT =
        "- Novità: cambia i codici aeroportuali IATA/ICAO direttamente "
        "dal pannello dettagli del velivolo (tocca la riga della "
        "rotta), oltre che dal menu Unità\n"
        "- Novità: filtro \"Mostra solo aerei a bassa quota\"\n"
        "- Novità: il messaggio di cielo vuoto ora mostra quali filtri "
        "sono attivi ed è toccabile - salta direttamente al menu dei "
        "filtri\n"
        "- Modificato: menu riorganizzati - nuovo sottomenu \"Elenchi\" "
        "(elenco velivoli, lista di controllo), nuovo sottomenu "
        "\"Filtri di visualizzazione\" (filtro compagnia, veicoli a "
        "terra, solo elicotteri, solo bassa quota), \"Strumenti\" ora "
        "contiene solo posizioni salvate e avviso lista di controllo\n"
        "- Modificato: lo stile di visualizzazione radar (effetto "
        "impulso CRT) spostato in cima al menu Schermo, ora attivo per "
        "default sui dispositivi nuovi/ripristinati\n"
        "- Correzione: il LED del battito poteva occasionalmente "
        "restare spento silenziosamente a causa di una race condition "
        "tra i due core della CPU\n"
        "- Correzione: il battito ora lampeggia brevemente (flash "
        "bianco) anche durante un avviso di prossimità/lista di "
        "controllo attivo, invece di essere completamente soppresso - "
        "l'avviso di emergenza mantiene comunque la priorità assoluta\n"
        "- Migliorato: comportamento delle richieste più robusto verso "
        "la fonte dati ADS-B (rallentamento sui limiti di frequenza, "
        "rispetta Retry-After)";

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
