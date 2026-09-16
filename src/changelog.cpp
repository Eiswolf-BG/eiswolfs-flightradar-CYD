#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: country of registration in the aircraft detail panel, "
        "derived from the ICAO hex address (covers ~78 countries/"
        "regions), shown next to the aircraft type\n"
        "- New: \"seen today\" status in the detail panel - NEW TODAY, "
        "SEEN Nx TODAY, or RETURNED AFTER 2h 14m depending on how "
        "often/when the aircraft was seen today\n"
        "- New: the \"Previously seen\" line now also shows the time "
        "of the last earlier sighting, not just the date\n"
        "- Fix: the airline filter's \"Show only\" (whitelist) mode "
        "could accidentally hide ALL aircraft when the list was "
        "empty, making the radar appear completely empty even though "
        "aircraft were detected (only visible as \"filtered\" in the "
        "bottom-right event indicator) - happened simply by tapping "
        "the \"Show only\"/\"Hide\" switch once without also adding an "
        "airline to the list. An empty whitelist no longer blocks "
        "anything";

    const char* const CHANGELOG_DE =
        "- Neu: Herkunftsland im Flugzeug-Steckbrief, abgeleitet aus "
        "dem ICAO-Hex-Code (~78 Laender/Regionen abgedeckt), "
        "angezeigt neben dem Flugzeugtyp\n"
        "- Neu: \"Heute gesehen\"-Status im Steckbrief - NEW TODAY, "
        "SEEN Nx TODAY oder RETURNED AFTER 2h 14m, je nachdem wie "
        "oft/wann das Flugzeug heute schon gesehen wurde\n"
        "- Neu: Die \"Previously Seen\"-Zeile zeigt jetzt zusaetzlich "
        "zum Datum auch die Uhrzeit der letzten fruehreren Sichtung\n"
        "- Fix: Der Airline-Filter konnte im \"Nur anzeigen\"-Modus "
        "(Whitelist) bei leerer Liste versehentlich SAEMTLICHE "
        "Flugzeuge ausblenden, sodass der Radar komplett leer "
        "erschien, obwohl Flugzeuge erkannt wurden (nur noch als "
        "\"gefiltert\" in der Ereignis-Ecke unten rechts sichtbar) - "
        "passierte schon durch einmaliges Antippen des \"Nur "
        "anzeigen\"/\"Ausblenden\"-Schalters, ohne dabei auch eine "
        "Airline in die Liste einzutragen. Eine leere Whitelist "
        "blockiert jetzt nichts mehr";

    const char* const CHANGELOG_FR =
        "- Nouveau : pays d'immatriculation dans le panneau de "
        "détails, déduit de l'adresse hexadécimale ICAO (couvre "
        "~78 pays/régions), affiché à côté du type d'avion\n"
        "- Nouveau : statut « vu aujourd'hui » dans le panneau de "
        "détails - NEW TODAY, SEEN Nx TODAY ou RETURNED AFTER 2h 14m "
        "selon la fréquence/le moment où l'avion a été vu aujourd'hui\n"
        "- Nouveau : la ligne « Previously seen » affiche désormais "
        "aussi l'heure de la dernière observation, pas seulement la "
        "date\n"
        "- Correction : le mode « Afficher seulement » (liste "
        "blanche) du filtre compagnies pouvait masquer TOUS les "
        "avions par accident quand la liste était vide, donnant "
        "l'impression d'un radar completement vide alors que des "
        "avions étaient détectés (visibles uniquement comme "
        "« filtré » dans l'indicateur d'événement en bas à droite) - "
        "il suffisait d'appuyer une fois sur le bouton « Afficher "
        "seulement »/« Masquer » sans ajouter de compagnie à la "
        "liste. Une liste blanche vide ne bloque plus rien";

    const char* const CHANGELOG_TR =
        "- Yeni: Uçak ayrıntı panelinde, ICAO hex adresinden "
        "türetilen tescil ülkesi (~78 ülke/bölge kapsanıyor), uçak "
        "tipinin yanında gösterilir\n"
        "- Yeni: Ayrıntı panelinde \"bugün görüldü\" durumu - uçağın "
        "bugün ne sıklıkta/ne zaman görüldüğüne bağlı olarak NEW "
        "TODAY, SEEN Nx TODAY veya RETURNED AFTER 2h 14m\n"
        "- Yeni: \"Previously seen\" satırı artık tarihe ek olarak "
        "son önceki görülme saatini de gösteriyor\n"
        "- Düzeltme: Havayolu filtresinin \"Sadece göster\" "
        "(beyaz liste) modu, liste boşken yanlışlıkla TÜM uçakları "
        "gizleyebiliyordu; uçaklar algılanmasına rağmen radar tamamen "
        "boş görünüyordu (sadece sağ alttaki olay göstergesinde "
        "\"filtrelendi\" olarak görünüyordu) - listeye bir havayolu "
        "eklemeden \"Sadece göster\"/\"Gizle\" anahtarına bir kez "
        "dokunmak yeterliydi. Boş bir beyaz liste artık hiçbir şeyi "
        "engellemiyor";

    const char* const CHANGELOG_ES =
        "- Novedad: país de matrícula en el panel de detalles, "
        "obtenido a partir de la dirección hexadecimal ICAO (cubre "
        "~78 países/regiones), mostrado junto al tipo de avión\n"
        "- Novedad: estado \"visto hoy\" en el panel de detalles - "
        "NEW TODAY, SEEN Nx TODAY o RETURNED AFTER 2h 14m según con "
        "qué frecuencia/cuándo se vio el avión hoy\n"
        "- Novedad: la línea \"Previously seen\" ahora también "
        "muestra la hora del último avistamiento anterior, no solo "
        "la fecha\n"
        "- Corrección: el modo \"Mostrar solo\" (lista blanca) del "
        "filtro de aerolíneas podía ocultar accidentalmente TODOS "
        "los aviones cuando la lista estaba vacía, haciendo que el "
        "radar pareciera completamente vacío aunque se detectaran "
        "aviones (solo visibles como \"filtrado\" en el indicador de "
        "eventos abajo a la derecha) - bastaba con tocar una vez el "
        "interruptor \"Mostrar solo\"/\"Ocultar\" sin añadir ninguna "
        "aerolínea a la lista. Una lista blanca vacía ya no bloquea "
        "nada";

    const char* const CHANGELOG_IT =
        "- Novità: paese di immatricolazione nel pannello dettagli, "
        "ricavato dall'indirizzo esadecimale ICAO (copre ~78 paesi/"
        "regioni), mostrato accanto al tipo di aereo\n"
        "- Novità: stato \"visto oggi\" nel pannello dettagli - NEW "
        "TODAY, SEEN Nx TODAY o RETURNED AFTER 2h 14m a seconda di "
        "quanto spesso/quando l'aereo è stato visto oggi\n"
        "- Novità: la riga \"Previously seen\" ora mostra anche "
        "l'ora dell'ultimo avvistamento precedente, non solo la data\n"
        "- Correzione: la modalità \"Mostra solo\" (lista bianca) del "
        "filtro compagnie poteva nascondere accidentalmente TUTTI gli "
        "aerei quando l'elenco era vuoto, facendo apparire il radar "
        "completamente vuoto pur essendo stati rilevati aerei "
        "(visibili solo come \"filtrato\" nell'indicatore eventi in "
        "basso a destra) - bastava toccare una volta l'interruttore "
        "\"Mostra solo\"/\"Nascondi\" senza aggiungere una compagnia "
        "all'elenco. Un elenco bianco vuoto ora non blocca più nulla";

    const char* const CHANGELOG_PT =
        "- Novo: país de registro no painel de detalhes, derivado do "
        "endereço hexadecimal ICAO (cobre ~78 países/regiões), "
        "exibido ao lado do tipo de aeronave\n"
        "- Novo: status \"visto hoje\" no painel de detalhes - NEW "
        "TODAY, SEEN Nx TODAY ou RETURNED AFTER 2h 14m dependendo de "
        "quantas vezes/quando a aeronave foi vista hoje\n"
        "- Novo: a linha \"Previously seen\" agora também mostra o "
        "horário do último avistamento anterior, não apenas a data\n"
        "- Correção: o modo \"Mostrar apenas\" (lista branca) do "
        "filtro de companhias aéreas podia ocultar acidentalmente "
        "TODAS as aeronaves quando a lista estava vazia, fazendo o "
        "radar parecer completamente vazio mesmo com aeronaves "
        "detectadas (visíveis apenas como \"filtrado\" no indicador "
        "de eventos no canto inferior direito) - bastava tocar uma "
        "vez no alternador \"Mostrar apenas\"/\"Ocultar\" sem "
        "adicionar nenhuma companhia à lista. Uma lista branca vazia "
        "agora não bloqueia mais nada";

    const char* const CHANGELOG_NL =
        "- Nieuw: registratieland in het detailvenster, afgeleid uit "
        "het ICAO-hexadres (dekt ~78 landen/regio's), getoond naast "
        "het vliegtuigtype\n"
        "- Nieuw: \"vandaag gezien\"-status in het detailvenster - "
        "NEW TODAY, SEEN Nx TODAY of RETURNED AFTER 2h 14m, "
        "afhankelijk van hoe vaak/wanneer het vliegtuig vandaag is "
        "gezien\n"
        "- Nieuw: de \"Previously seen\"-regel toont nu ook het "
        "tijdstip van de laatste eerdere waarneming, niet alleen de "
        "datum\n"
        "- Fix: de \"Alleen tonen\"-modus (whitelist) van de "
        "luchtvaartmaatschappijenfilter kon per ongeluk ALLE "
        "vliegtuigen verbergen wanneer de lijst leeg was, waardoor de "
        "radar volledig leeg leek terwijl er wel vliegtuigen werden "
        "gedetecteerd (alleen zichtbaar als \"gefilterd\" in de "
        "gebeurtenisindicator rechtsonder) - dit gebeurde al door "
        "eenmaal op de \"Alleen tonen\"/\"Verbergen\"-schakelaar te "
        "tikken zonder ook een maatschappij aan de lijst toe te "
        "voegen. Een lege whitelist blokkeert nu niets meer";

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
