#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the update installation screen now shows a filling "
        "progress bar and a reminder not to unplug or turn off the "
        "device during installation, in addition to a larger, clearer "
        "heading\n"
        "- New: the rain effect on the screensaver now falls noticeably "
        "denser and stronger at all three intensity levels (about 50% "
        "more than before), so it's actually noticeable during real "
        "rainy weather";

    const char* const CHANGELOG_DE =
        "- Neu: Der Update-Installationsbildschirm zeigt jetzt einen "
        "sich füllenden Fortschrittsbalken und einen Hinweis, das Gerät "
        "während der Installation nicht auszustecken oder auszuschalten, "
        "zusätzlich zur größeren, deutlicheren Überschrift\n"
        "- Neu: Der Regen-Effekt auf dem Ruhebildschirm fällt jetzt in "
        "allen Intensitätsstufen spürbar dichter und kräftiger (rund 50% "
        "mehr als vorher), damit er bei echtem Regenwetter auch wirklich "
        "auffällt";

    const char* const CHANGELOG_FR =
        "- Nouveau : l'écran d'installation de la mise à jour affiche "
        "désormais une barre de progression qui se remplit et un rappel "
        "de ne pas débrancher ni éteindre l'appareil pendant "
        "l'installation, en plus d'un titre plus grand et plus clair\n"
        "- Nouveau : l'effet de pluie sur l'écran de veille tombe "
        "désormais nettement plus dense et plus intense à tous les "
        "niveaux d'intensité (environ 50 % de plus qu'avant), pour qu'il "
        "soit vraiment perceptible par temps de pluie réel";

    const char* const CHANGELOG_TR =
        "- Yeni: Güncelleme kurulum ekranı artık dolan bir ilerleme "
        "çubuğu ve kurulum sırasında cihazın fişini çekmemeniz veya "
        "cihazı kapatmamanız gerektiğini hatırlatan bir not gösteriyor, "
        "ayrıca daha büyük ve net bir başlığa sahip\n"
        "- Yeni: Bekleme ekranındaki yağmur efekti artık her üç yoğunluk "
        "seviyesinde de belirgin şekilde daha yoğun ve güçlü yağıyor "
        "(öncekine göre yaklaşık %50 daha fazla), böylece gerçek "
        "yağmurlu havada gerçekten fark ediliyor";

    const char* const CHANGELOG_ES =
        "- Novedad: la pantalla de instalación de actualizaciones ahora "
        "muestra una barra de progreso que se va llenando y un aviso de "
        "no desconectar ni apagar el dispositivo durante la instalación, "
        "además de un título más grande y claro\n"
        "- Novedad: el efecto de lluvia en el salvapantallas ahora cae "
        "notablemente más denso e intenso en los tres niveles de "
        "intensidad (alrededor de un 50% más que antes), para que "
        "realmente se note con lluvia real";

    const char* const CHANGELOG_IT =
        "- Novità: la schermata di installazione dell'aggiornamento ora "
        "mostra una barra di avanzamento che si riempie e un avviso di "
        "non scollegare né spegnere il dispositivo durante "
        "l'installazione, oltre a un titolo più grande e chiaro\n"
        "- Novità: l'effetto pioggia sullo screensaver ora cade "
        "notevolmente più denso e intenso a tutti e tre i livelli di "
        "intensità (circa il 50% in più rispetto a prima), così da "
        "risultare davvero evidente con la pioggia reale";

    const char* const CHANGELOG_PT =
        "- Novo: a tela de instalação da atualização agora mostra uma "
        "barra de progresso que se preenche e um aviso para não "
        "desconectar nem desligar o dispositivo durante a instalação, "
        "além de um título maior e mais claro\n"
        "- Novo: o efeito de chuva na proteção de tela agora cai "
        "visivelmente mais denso e forte em todos os três níveis de "
        "intensidade (cerca de 50% a mais que antes), para que realmente "
        "se note em dias de chuva de verdade";

    const char* const CHANGELOG_NL =
        "- Nieuw: het update-installatiescherm toont nu een vullende "
        "voortgangsbalk en een herinnering om het apparaat tijdens de "
        "installatie niet los te koppelen of uit te schakelen, naast een "
        "grotere, duidelijkere titel\n"
        "- Nieuw: het regeneffect op de schermbeveiliging valt nu "
        "merkbaar dichter en krachtiger bij alle drie de "
        "intensiteitsniveaus (ongeveer 50% meer dan voorheen), zodat het "
        "bij echt regenachtig weer ook echt opvalt";

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
