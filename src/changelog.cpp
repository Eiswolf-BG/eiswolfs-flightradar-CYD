#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: flight phase indicator in the aircraft detail panel "
        "(TAKEOFF, CLIMB, CRUISE, DESCENT, APPROACH, LANDING, or LOW "
        "PASS), shown next to the climb/descent line\n"
        "- New: ADS-B data quality indicator (GOOD/PARTIAL) next to "
        "the altitude line, based on how many core fields the "
        "aircraft is actually transmitting\n"
        "- New: bearing and elevation angle combined into a single "
        "compact \"Look: NE 21°\" line\n"
        "- New: Type Watchlist - watch up to 5 aircraft types (e.g. "
        "A380), triggers the same alert (LED, web sound/badge, ntfy "
        "push) as the callsign/squawk watchlist\n"
        "- Fix: the two scrolling lines in the detail panel are now "
        "grouped together at the bottom, for a more consistent "
        "layout";

    const char* const CHANGELOG_DE =
        "- Neu: Flugphasen-Anzeige im Flugzeug-Steckbrief (TAKEOFF, "
        "CLIMB, CRUISE, DESCENT, APPROACH, LANDING oder LOW PASS), "
        "angezeigt neben der Steig-/Sinkflug-Zeile\n"
        "- Neu: ADS-B-Datenqualitaetsanzeige (GOOD/PARTIAL) neben der "
        "Hoehen-Zeile, basierend darauf, wie viele Kernfelder das "
        "Flugzeug tatsaechlich sendet\n"
        "- Neu: Peilung und Hoehenwinkel jetzt in einer kompakten "
        "\"Look: NE 21°\"-Zeile zusammengefasst\n"
        "- Neu: Typ-Wachliste - bis zu 5 Flugzeugtypen beobachten "
        "(z.B. A380), loest denselben Alarm (LED, Web-Sound/-Badge, "
        "ntfy-Push) wie die Rufzeichen-/Squawk-Wachliste aus\n"
        "- Fix: Die beiden scrollenden Zeilen im Steckbrief stehen "
        "jetzt gemeinsam am Ende des Panels, fuer ein einheitlicheres "
        "Erscheinungsbild";

    const char* const CHANGELOG_FR =
        "- Nouveau : indicateur de phase de vol dans le panneau de "
        "détails (TAKEOFF, CLIMB, CRUISE, DESCENT, APPROACH, LANDING "
        "ou LOW PASS), affiché à côté de la ligne montée/descente\n"
        "- Nouveau : indicateur de qualité des données ADS-B (GOOD/"
        "PARTIAL) à côté de la ligne d'altitude, selon le nombre de "
        "champs essentiels réellement transmis par l'avion\n"
        "- Nouveau : relèvement et angle d'élévation désormais "
        "combinés en une seule ligne compacte « Look: NE 21° »\n"
        "- Nouveau : veille de type - surveillez jusqu'à 5 types "
        "d'avion (ex. A380), déclenche la même alerte (LED, son/badge "
        "web, push ntfy) que la veille indicatif/squawk\n"
        "- Correction : les deux lignes défilantes du panneau de "
        "détails sont désormais regroupées en bas, pour une "
        "présentation plus cohérente";

    const char* const CHANGELOG_TR =
        "- Yeni: Uçak ayrıntı panelinde uçuş fazı göstergesi "
        "(TAKEOFF, CLIMB, CRUISE, DESCENT, APPROACH, LANDING veya LOW "
        "PASS), tırmanma/alçalma satırının yanında gösterilir\n"
        "- Yeni: Uçağın gerçekte kaç temel alanı gönderdiğine bağlı "
        "olarak irtifa satırının yanında ADS-B veri kalitesi "
        "göstergesi (GOOD/PARTIAL)\n"
        "- Yeni: Kerteriz ve yükseliş açısı artık tek, kompakt bir "
        "\"Look: NE 21°\" satırında birleştirildi\n"
        "- Yeni: Tip İzleme - 5 adete kadar uçak tipini izleyin (örn. "
        "A380), çağrı işareti/squawk izleme ile aynı uyarıyı (LED, "
        "web sesi/rozeti, ntfy push) tetikler\n"
        "- Düzeltme: Ayrıntı panelindeki kayan iki satır artık daha "
        "tutarlı bir görünüm için birlikte panelin altına taşındı";

    const char* const CHANGELOG_ES =
        "- Novedad: indicador de fase de vuelo en el panel de "
        "detalles (TAKEOFF, CLIMB, CRUISE, DESCENT, APPROACH, LANDING "
        "o LOW PASS), mostrado junto a la línea de ascenso/descenso\n"
        "- Novedad: indicador de calidad de datos ADS-B (GOOD/"
        "PARTIAL) junto a la línea de altitud, según cuántos campos "
        "esenciales transmite realmente el avión\n"
        "- Novedad: el rumbo y el ángulo de elevación ahora "
        "combinados en una sola línea compacta \"Look: NE 21°\"\n"
        "- Novedad: vigilancia de tipo - vigile hasta 5 tipos de "
        "avión (ej. A380), activa la misma alerta (LED, sonido/"
        "insignia web, push ntfy) que la vigilancia de indicativo/"
        "squawk\n"
        "- Corrección: las dos líneas con desplazamiento del panel de "
        "detalles ahora están agrupadas al final, para una "
        "presentación más uniforme";

    const char* const CHANGELOG_IT =
        "- Novità: indicatore di fase di volo nel pannello dettagli "
        "(TAKEOFF, CLIMB, CRUISE, DESCENT, APPROACH, LANDING o LOW "
        "PASS), mostrato accanto alla riga salita/discesa\n"
        "- Novità: indicatore di qualità dei dati ADS-B (GOOD/"
        "PARTIAL) accanto alla riga altitudine, in base a quanti "
        "campi essenziali l'aereo trasmette effettivamente\n"
        "- Novità: rilevamento e angolo di elevazione ora combinati "
        "in un'unica riga compatta \"Look: NE 21°\"\n"
        "- Novità: controllo tipo - monitora fino a 5 tipi di aereo "
        "(es. A380), attiva lo stesso allarme (LED, suono/badge web, "
        "push ntfy) del controllo nominativo/squawk\n"
        "- Correzione: le due righe scorrevoli del pannello dettagli "
        "ora sono raggruppate in fondo, per un aspetto più coerente";

    const char* const CHANGELOG_PT =
        "- Novo: indicador de fase de voo no painel de detalhes "
        "(TAKEOFF, CLIMB, CRUISE, DESCENT, APPROACH, LANDING ou LOW "
        "PASS), exibido ao lado da linha de subida/descida\n"
        "- Novo: indicador de qualidade de dados ADS-B (GOOD/"
        "PARTIAL) ao lado da linha de altitude, com base em quantos "
        "campos essenciais a aeronave realmente transmite\n"
        "- Novo: rumo e ângulo de elevação agora combinados em uma "
        "única linha compacta \"Look: NE 21°\"\n"
        "- Novo: Monitoramento de Tipo - monitore até 5 tipos de "
        "aeronave (ex. A380), aciona o mesmo alerta (LED, som/selo "
        "web, push ntfy) que o monitoramento de indicativo/squawk\n"
        "- Correção: as duas linhas com rolagem do painel de "
        "detalhes agora ficam agrupadas no final, para uma "
        "apresentação mais consistente";

    const char* const CHANGELOG_NL =
        "- Nieuw: vliegfase-indicator in het detailvenster (TAKEOFF, "
        "CLIMB, CRUISE, DESCENT, APPROACH, LANDING of LOW PASS), "
        "getoond naast de klim-/daallijn\n"
        "- Nieuw: ADS-B-datakwaliteitsindicator (GOOD/PARTIAL) naast "
        "de hoogtelijn, gebaseerd op hoeveel kernvelden het vliegtuig "
        "daadwerkelijk verzendt\n"
        "- Nieuw: peiling en elevatiehoek nu gecombineerd in een "
        "compacte \"Look: NE 21°\"-regel\n"
        "- Nieuw: Type-bewaking - volg tot 5 vliegtuigtypes (bijv. "
        "A380), activeert dezelfde melding (LED, websound/-badge, "
        "ntfy-push) als de roepnaam-/squawk-bewaking\n"
        "- Fix: de twee scrollende regels in het detailvenster staan "
        "nu samen onderaan, voor een consistentere weergave";

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
