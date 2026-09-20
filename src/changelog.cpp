#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: Route Watchlist - a 4th watchlist type matching by "
        "departure and/or destination airport (ICAO), both fields "
        "optional (Menu -> Flight Options -> Lists -> Route Watch)\n"
        "- New: \"Interesting Only\" display filter - shows only "
        "aircraft matching an existing detection (military/"
        "government, emergency squawk, any watchlist hit, or Heavy) "
        "(Menu -> Flight Options -> Display Filters)\n"
        "- Fix: overlapping text on the Route Watchlist screen in "
        "longer languages\n"
        "- Fix: display filters (airline filter, hide ground "
        "vehicles, helicopters only, low flyers only, interesting "
        "only) now behave consistently across the radar, aircraft "
        "list, live traffic, and web map views (previously some only "
        "applied to the radar screen)";

    const char* const CHANGELOG_DE =
        "- Neu: Routen-Wachliste - eine 4. Wachlisten-Art, die nach "
        "Start- und/oder Zielflughafen (ICAO) abgleicht, beide "
        "Felder optional (Menue -> Flugoptionen -> Listen -> Route "
        "Watch)\n"
        "- Neu: Anzeigefilter \"Interessant\" - zeigt nur Flugzeuge, "
        "auf die eine bereits bestehende Erkennung zutrifft (Militaer/"
        "Behoerde, Notfall-Squawk, ein Wachlisten-Treffer oder Heavy) "
        "(Menue -> Flugoptionen -> Anzeigefilter)\n"
        "- Fix: sich ueberlappender Text auf dem Routen-Wachliste-"
        "Bildschirm bei laengeren Sprachen\n"
        "- Fix: Anzeigefilter (Airline-Filter, Bodenfahrzeuge "
        "ausblenden, Nur Helikopter, Nur Niedrigflieger, Nur "
        "Interessant) wirken jetzt einheitlich auf Radar, "
        "Flugzeugliste, Live-Traffic und Web-Karte (vorher wirkten "
        "manche nur auf dem Radarbildschirm)";

    const char* const CHANGELOG_FR =
        "- Nouveau : Liste de routes - un 4e type de liste de "
        "surveillance qui compare l'aéroport de départ et/ou de "
        "destination (OACI), les deux champs étant optionnels (Menu "
        "-> Options de vol -> Listes -> Liste Routes)\n"
        "- Nouveau : Filtre d'affichage \"Intéressant\" - affiche "
        "uniquement les avions correspondant à une détection déjà "
        "existante (militaire/gouvernemental, squawk d'urgence, une "
        "correspondance sur une liste de surveillance, ou Heavy) "
        "(Menu -> Options de vol -> Filtres d'affichage)\n"
        "- Correction : texte qui se chevauchait sur l'écran de la "
        "liste de routes dans les langues plus longues\n"
        "- Correction : les filtres d'affichage (filtre compagnie, "
        "masquer les véhicules au sol, hélicoptères uniquement, vols "
        "bas uniquement, intéressant uniquement) se comportent "
        "désormais de manière cohérente sur le radar, la liste des "
        "avions, le trafic en direct et la carte web (certains ne "
        "s'appliquaient auparavant qu'à l'écran radar)";

    const char* const CHANGELOG_TR =
        "- Yeni: Rota İzleme Listesi - kalkış ve/veya varış "
        "havalimanına (ICAO) göre eşleştiren 4. bir izleme listesi "
        "türü, her iki alan da isteğe bağlı (Menü -> Uçuş "
        "Seçenekleri -> Listeler -> Rota İzleme)\n"
        "- Yeni: \"Sadece İlginç\" görüntüleme filtresi - yalnızca "
        "mevcut bir tespitle eşleşen uçakları gösterir (askeri/"
        "resmi, acil durum squawk kodu, herhangi bir izleme listesi "
        "eşleşmesi veya Heavy) (Menü -> Uçuş Seçenekleri -> "
        "Görüntüleme Filtreleri)\n"
        "- Düzeltme: daha uzun dillerde Rota İzleme ekranındaki üst "
        "üste binen metin\n"
        "- Düzeltme: görüntüleme filtreleri (havayolu filtresi, yer "
        "araçlarını gizle, sadece helikopterler, sadece alçak "
        "uçuşlar, sadece ilginç) artık radar, uçak listesi, canlı "
        "trafik ve web haritası görünümlerinde tutarlı şekilde "
        "çalışıyor (öncesinde bazıları yalnızca radar ekranında "
        "etkiliydi)";

    const char* const CHANGELOG_ES =
        "- Novedad: Lista de rutas - un 4.º tipo de lista de "
        "vigilancia que compara el aeropuerto de salida y/o destino "
        "(OACI), ambos campos opcionales (Menú -> Opciones de vuelo "
        "-> Listas -> Lista de Rutas)\n"
        "- Novedad: Filtro de visualización \"Interesante\" - "
        "muestra solo aeronaves que coinciden con una detección ya "
        "existente (militar/gubernamental, squawk de emergencia, una "
        "coincidencia en cualquier lista de vigilancia, o Heavy) "
        "(Menú -> Opciones de vuelo -> Filtros de Visualización)\n"
        "- Corrección: texto superpuesto en la pantalla de la Lista "
        "de Rutas en idiomas más largos\n"
        "- Corrección: los filtros de visualización (filtro de "
        "aerolínea, ocultar vehículos terrestres, solo helicópteros, "
        "solo vuelos bajos, solo interesante) ahora se comportan de "
        "forma consistente en el radar, la lista de aeronaves, el "
        "tráfico en vivo y el mapa web (antes algunos solo se "
        "aplicaban a la pantalla del radar)";

    const char* const CHANGELOG_IT =
        "- Novità: Lista rotte - un 4º tipo di lista di controllo "
        "che confronta l'aeroporto di partenza e/o di destinazione "
        "(ICAO), entrambi i campi opzionali (Menu -> Opzioni di volo "
        "-> Liste -> Lista Rotte)\n"
        "- Novità: Filtro di visualizzazione \"Interessante\" - "
        "mostra solo aerei che corrispondono a un rilevamento già "
        "esistente (militare/governativo, squawk di emergenza, una "
        "corrispondenza su qualsiasi lista di controllo, o Heavy) "
        "(Menu -> Opzioni di volo -> Filtri di Visualizzazione)\n"
        "- Correzione: testo sovrapposto nella schermata Lista Rotte "
        "nelle lingue più lunghe\n"
        "- Correzione: i filtri di visualizzazione (filtro compagnia "
        "aerea, nascondi veicoli a terra, solo elicotteri, solo voli "
        "bassi, solo interessante) ora si comportano in modo "
        "coerente su radar, lista aerei, traffico in tempo reale e "
        "mappa web (prima alcuni si applicavano solo alla schermata "
        "radar)";

    const char* const CHANGELOG_PT =
        "- Novo: Lista de Rotas - um 4º tipo de lista de observação "
        "que compara o aeroporto de partida e/ou destino (ICAO), "
        "ambos os campos opcionais (Menu -> Opções de Voo -> Listas "
        "-> Lista de Rotas)\n"
        "- Novo: Filtro de exibição \"Interessante\" - mostra apenas "
        "aeronaves que correspondem a uma detecção já existente "
        "(militar/governamental, squawk de emergência, uma "
        "correspondência em qualquer lista de observação, ou Heavy) "
        "(Menu -> Opções de Voo -> Filtros de Exibição)\n"
        "- Correção: texto sobreposto na tela da Lista de Rotas em "
        "idiomas mais longos\n"
        "- Correção: os filtros de exibição (filtro de companhia "
        "aérea, ocultar veículos terrestres, somente helicópteros, "
        "somente voos baixos, somente interessante) agora se "
        "comportam de forma consistente no radar, na lista de "
        "aeronaves, no tráfego ao vivo e no mapa web (antes alguns "
        "só se aplicavam à tela do radar)";

    const char* const CHANGELOG_NL =
        "- Nieuw: Routelijst - een 4e type volglijst dat overeenkomt "
        "op vertrek- en/of bestemmingsluchthaven (ICAO), beide "
        "velden optioneel (Menu -> Vluchtopties -> Lijsten -> "
        "Routelijst)\n"
        "- Nieuw: Weergavefilter \"Interessant\" - toont alleen "
        "vliegtuigen die overeenkomen met een reeds bestaande "
        "detectie (militair/overheid, noodsquawk, een treffer op een "
        "volglijst, of Heavy) (Menu -> Vluchtopties -> "
        "Weergavefilters)\n"
        "- Fix: overlappende tekst op het Routelijst-scherm bij "
        "langere talen\n"
        "- Fix: weergavefilters (luchtvaartmaatschappijfilter, "
        "grondvoertuigen verbergen, alleen helikopters, alleen "
        "laagvliegers, alleen interessant) gedragen zich nu "
        "consistent op radar, vliegtuiglijst, live traffic en "
        "webkaart (voorheen golden sommige alleen voor het "
        "radarscherm)";

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
