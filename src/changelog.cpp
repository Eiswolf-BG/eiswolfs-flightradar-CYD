#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the SPK speaker alarm now also reacts to watchlist hits "
        "(a short single beep), in addition to real emergency squawks "
        "(continuous siren)\n"
        "- New: weather icons in the 3h forecast screen are now "
        "colorful (yellow sun, white clouds) instead of a single "
        "color\n"
        "- Fix: the live radar website's weather icon overlapped with "
        "the range/mode row below it\n"
        "- Improvement: the altitude color legend on the live radar "
        "website now sits below the radar/map instead of cramped above "
        "it";

    const char* const CHANGELOG_DE =
        "- Neu: Der SPK-Lautsprecher-Alarm reagiert jetzt zusaetzlich "
        "zu echten Notfall-Squawks (Dauer-Sirene) auch auf Wachlisten-"
        "Treffer (kurzer Einzelton)\n"
        "- Neu: Die Wetter-Symbole im 3h-Vorschau-Screen sind jetzt "
        "farbig (gelbe Sonne, weisse Wolken) statt einfarbig\n"
        "- Fix: Das Wetter-Icon auf der Live-Radar-Webseite "
        "ueberschnitt sich mit der Range-/Mode-Zeile darunter\n"
        "- Verbesserung: Die Hoehenfarben-Legende auf der Live-Radar-"
        "Webseite steht jetzt unterhalb von Radar/Karte statt "
        "gedraengt darueber";

    const char* const CHANGELOG_FR =
        "- Nouveau : l'alarme du haut-parleur SPK réagit désormais "
        "aussi aux correspondances de liste de surveillance (un bref "
        "bip unique), en plus des véritables codes squawk d'urgence "
        "(sirène continue)\n"
        "- Nouveau : les icônes météo de l'écran de prévision sur 3h "
        "sont désormais en couleur (soleil jaune, nuages blancs) au "
        "lieu d'une seule couleur\n"
        "- Correction : l'icône météo du site du radar en direct se "
        "superposait à la ligne de portée/mode juste en dessous\n"
        "- Amélioration : la légende des couleurs d'altitude du site "
        "du radar en direct se trouve désormais sous le radar/la carte "
        "au lieu d'être compressée au-dessus";

    const char* const CHANGELOG_TR =
        "- Yeni: SPK hoparlör alarmı artık gerçek acil durum squawk "
        "kodlarına (sürekli siren) ek olarak takip listesi "
        "eşleşmelerinde de (kısa tek bir bip) tepki veriyor\n"
        "- Yeni: 3 saatlik tahmin ekranındaki hava durumu simgeleri "
        "artık tek renk yerine renkli (sarı güneş, beyaz bulutlar)\n"
        "- Düzeltme: Canlı radar web sitesindeki hava durumu simgesi "
        "altındaki menzil/mod satırıyla çakışıyordu\n"
        "- İyileştirme: Canlı radar web sitesindeki irtifa renk "
        "lejantı artık radar/haritanın üstünde sıkışık değil, altında "
        "yer alıyor";

    const char* const CHANGELOG_ES =
        "- Novedad: la alarma del altavoz SPK ahora también reacciona "
        "a las coincidencias de la lista de vigilancia (un breve "
        "pitido único), además de a los códigos squawk de emergencia "
        "reales (sirena continua)\n"
        "- Novedad: los iconos meteorológicos de la pantalla de "
        "previsión a 3h ahora son a color (sol amarillo, nubes "
        "blancas) en vez de un solo color\n"
        "- Corrección: el icono del tiempo en el sitio web de radar en "
        "vivo se superponía con la fila de alcance/modo de debajo\n"
        "- Mejora: la leyenda de colores de altitud del sitio web de "
        "radar en vivo ahora aparece debajo del radar/mapa en lugar de "
        "apretada arriba";

    const char* const CHANGELOG_IT =
        "- Novità: l'allarme dell'altoparlante SPK ora reagisce anche "
        "alle corrispondenze nella lista di controllo (un breve "
        "segnale singolo), oltre ai veri codici squawk di emergenza "
        "(sirena continua)\n"
        "- Novità: le icone meteo nella schermata di previsione a 3h "
        "sono ora a colori (sole giallo, nuvole bianche) invece che di "
        "un solo colore\n"
        "- Correzione: l'icona meteo sul sito del radar live si "
        "sovrapponeva alla riga di raggio/modalità sottostante\n"
        "- Miglioramento: la legenda dei colori di altitudine sul sito "
        "del radar live ora si trova sotto radar/mappa invece che "
        "compressa sopra";

    const char* const CHANGELOG_PT =
        "- Novo: o alarme do alto-falante SPK agora também reage a "
        "correspondências na lista de observação (um breve bipe "
        "único), além dos códigos squawk de emergência reais (sirene "
        "contínua)\n"
        "- Novo: os ícones de clima na tela de previsão de 3h agora "
        "são coloridos (sol amarelo, nuvens brancas) em vez de uma "
        "única cor\n"
        "- Correção: o ícone do clima no site de radar ao vivo se "
        "sobrepunha à linha de alcance/modo abaixo\n"
        "- Melhoria: a legenda de cores de altitude no site de radar "
        "ao vivo agora fica abaixo do radar/mapa em vez de apertada "
        "acima";

    const char* const CHANGELOG_NL =
        "- Nieuw: het SPK-luidsprekeralarm reageert nu ook op "
        "volglijst-treffers (een korte, enkele toon), naast echte "
        "noodsquawks (doorlopende sirene)\n"
        "- Nieuw: de weersymbolen in het 3-uurs voorspellingsscherm "
        "zijn nu in kleur (gele zon, witte wolken) in plaats van één "
        "kleur\n"
        "- Fix: het weerpictogram op de live-radarwebsite overlapte "
        "met de bereik-/modusregel daaronder\n"
        "- Verbetering: de hoogtekleurenlegenda op de "
        "live-radarwebsite staat nu onder de radar/kaart in plaats van "
        "gedrongen erboven";

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
