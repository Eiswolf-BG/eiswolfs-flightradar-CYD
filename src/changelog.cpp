#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: Live Traffic screen now shows a traffic direction "
        "breakdown (8-sector compass distribution) and the dominant "
        "flow direction\n"
        "- New: Live Traffic screen now shows an altitude "
        "distribution (same low/mid/high bands as the altitude "
        "color legend)\n"
        "- New: all four watchlists (aircraft, squawk, type, route) "
        "now support up to 12 entries each (up from 5)\n"
        "- Fix: all four watchlist screens now scroll properly "
        "instead of pushing the Add/Back buttons off-screen once "
        "many entries are present";

    const char* const CHANGELOG_DE =
        "- Neu: Der Live-Traffic-Screen zeigt jetzt eine "
        "Verkehrsrichtungs-Aufschluesselung (8-Sektoren-"
        "Kompassverteilung) und die Hauptflussrichtung\n"
        "- Neu: Der Live-Traffic-Screen zeigt jetzt eine "
        "Hoehenverteilung (dieselben Niedrig-/Mittel-/Hoch-Bereiche "
        "wie die Hoehen-Farblegende)\n"
        "- Neu: Alle vier Wachlisten (Rufzeichen, Squawk, Typ, "
        "Route) unterstuetzen jetzt bis zu 12 Eintraege (vorher 5)\n"
        "- Fix: Alle vier Wachlisten-Screens scrollen jetzt "
        "richtig, statt die Hinzufuegen-/Zurueck-Buttons bei vielen "
        "Eintraegen vom Bildschirm zu verdraengen";

    const char* const CHANGELOG_FR =
        "- Nouveau : l'écran Live Traffic affiche désormais une "
        "répartition de la direction du trafic (8 secteurs de "
        "boussole) et la direction de flux dominante\n"
        "- Nouveau : l'écran Live Traffic affiche désormais une "
        "répartition des altitudes (mêmes tranches basse/moyenne/"
        "haute que la légende de couleur d'altitude)\n"
        "- Nouveau : les quatre listes de surveillance (indicatif, "
        "squawk, type, route) prennent désormais en charge jusqu'à "
        "12 entrées chacune (au lieu de 5)\n"
        "- Correction : les quatre écrans de liste de surveillance "
        "défilent désormais correctement au lieu de repousser les "
        "boutons Ajouter/Retour hors de l'écran lorsque de "
        "nombreuses entrées sont présentes";

    const char* const CHANGELOG_TR =
        "- Yeni: Canlı Trafik ekranı artık bir trafik yönü dağılımı "
        "(8 sektörlü pusula dağılımı) ve baskın akış yönünü "
        "gösteriyor\n"
        "- Yeni: Canlı Trafik ekranı artık bir irtifa dağılımı "
        "gösteriyor (irtifa renk lejantındaki aynı düşük/orta/"
        "yüksek bantlar)\n"
        "- Yeni: dört izleme listesinin tümü (çağrı işareti, "
        "squawk, tip, rota) artık her biri için 12'ye kadar giriş "
        "destekliyor (öncesinde 5)\n"
        "- Düzeltme: dört izleme listesi ekranının tümü artık çok "
        "fazla giriş olduğunda Ekle/Geri düğmelerini ekran dışına "
        "itmek yerine düzgün şekilde kaydırılıyor";

    const char* const CHANGELOG_ES =
        "- Novedad: la pantalla Live Traffic ahora muestra un "
        "desglose de la dirección del tráfico (distribución de "
        "brújula de 8 sectores) y la dirección de flujo dominante\n"
        "- Novedad: la pantalla Live Traffic ahora muestra una "
        "distribución de altitud (las mismas franjas baja/media/"
        "alta que la leyenda de color de altitud)\n"
        "- Novedad: las cuatro listas de vigilancia (indicativo, "
        "squawk, tipo, ruta) ahora admiten hasta 12 entradas cada "
        "una (antes 5)\n"
        "- Corrección: las cuatro pantallas de listas de vigilancia "
        "ahora se desplazan correctamente en lugar de empujar los "
        "botones Añadir/Volver fuera de la pantalla cuando hay "
        "muchas entradas";

    const char* const CHANGELOG_IT =
        "- Novità: la schermata Live Traffic ora mostra una "
        "suddivisione della direzione del traffico (distribuzione a "
        "8 settori della bussola) e la direzione di flusso "
        "dominante\n"
        "- Novità: la schermata Live Traffic ora mostra una "
        "distribuzione delle altitudini (le stesse fasce bassa/"
        "media/alta della legenda colori altitudine)\n"
        "- Novità: tutte le quattro liste di controllo (nominativo, "
        "squawk, tipo, rotta) ora supportano fino a 12 voci "
        "ciascuna (prima 5)\n"
        "- Correzione: tutte le quattro schermate delle liste di "
        "controllo ora scorrono correttamente invece di spingere i "
        "pulsanti Aggiungi/Indietro fuori dallo schermo quando sono "
        "presenti molte voci";

    const char* const CHANGELOG_PT =
        "- Novo: a tela Live Traffic agora mostra uma distribuição "
        "da direção do tráfego (distribuição de bússola em 8 "
        "setores) e a direção de fluxo dominante\n"
        "- Novo: a tela Live Traffic agora mostra uma distribuição "
        "de altitude (as mesmas faixas baixa/média/alta da legenda "
        "de cores de altitude)\n"
        "- Novo: todas as quatro listas de observação (indicativo, "
        "squawk, tipo, rota) agora suportam até 12 itens cada "
        "(antes 5)\n"
        "- Correção: todas as quatro telas de listas de observação "
        "agora rolam corretamente em vez de empurrar os botões "
        "Adicionar/Voltar para fora da tela quando há muitos itens";

    const char* const CHANGELOG_NL =
        "- Nieuw: het Live Traffic-scherm toont nu een "
        "verkeersrichting-verdeling (8-sectoren kompasverdeling) en "
        "de dominante stroomrichting\n"
        "- Nieuw: het Live Traffic-scherm toont nu een "
        "hoogteverdeling (dezelfde laag/midden/hoog-banden als de "
        "hoogte-kleurenlegenda)\n"
        "- Nieuw: alle vier volglijsten (roepnaam, squawk, type, "
        "route) ondersteunen nu elk tot 12 items (voorheen 5)\n"
        "- Fix: alle vier volglijst-schermen scrollen nu correct in "
        "plaats van de Toevoegen/Terug-knoppen van het scherm te "
        "duwen bij veel items";

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
