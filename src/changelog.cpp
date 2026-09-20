#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: Language tables are now stored on the SD card instead "
        "of built into flash (except English, which stays as a "
        "permanent fallback) - frees about 184KB / 9 percentage "
        "points of flash\n"
        "- New: A language file is only re-downloaded automatically "
        "when its translated text actually changed, not on every "
        "firmware update\n"
        "- New: Robust fallback to English if the SD card is missing, "
        "a download fails, or a language file is invalid or corrupt\n"
        "- Fix: language selection no longer gets stuck on English "
        "after one language's download fails - the other languages "
        "are tried too\n"
        "- Fix: a timing bug in the language download throttle that "
        "could cause downloads to happen in the wrong order";

    const char* const CHANGELOG_DE =
        "- Neu: Sprachtabellen liegen jetzt auf der SD-Karte statt im "
        "Flash (ausser Englisch, das als dauerhafter Fallback bleibt) "
        "- spart rund 184KB / 9 Prozentpunkte Flash\n"
        "- Neu: Eine Sprachdatei wird nur automatisch neu "
        "heruntergeladen, wenn sich ihr uebersetzter Text tatsaechlich "
        "geaendert hat, nicht bei jedem Firmware-Update\n"
        "- Neu: Robuster Fallback auf Englisch, falls die SD-Karte "
        "fehlt, ein Download fehlschlaegt oder eine Sprachdatei "
        "ungueltig/beschaedigt ist\n"
        "- Fix: Die Sprachauswahl blieb nicht mehr bei Englisch "
        "haengen, wenn der Download einer Sprache fehlschlug - die "
        "anderen Sprachen werden jetzt ebenfalls versucht\n"
        "- Fix: ein Timing-Fehler in der Drosselung des Sprachdatei-"
        "Downloads, der Downloads in falscher Reihenfolge ausloesen "
        "konnte";

    const char* const CHANGELOG_FR =
        "- Nouveau : les tables de langue sont désormais stockées sur "
        "la carte SD au lieu d'être intégrées à la mémoire flash "
        "(sauf l'anglais, qui reste une solution de repli "
        "permanente) - libère environ 184 Ko / 9 points de "
        "pourcentage de flash\n"
        "- Nouveau : un fichier de langue n'est retéléchargé "
        "automatiquement que si son texte traduit a réellement "
        "changé, pas à chaque mise à jour du firmware\n"
        "- Nouveau : repli robuste vers l'anglais si la carte SD est "
        "absente, si un téléchargement échoue, ou si un fichier de "
        "langue est invalide ou corrompu\n"
        "- Correction : la sélection de langue ne reste plus bloquée "
        "sur l'anglais après l'échec du téléchargement d'une langue "
        "- les autres langues sont désormais essayées aussi\n"
        "- Correction : un bug de synchronisation dans la limitation "
        "des téléchargements de langue qui pouvait provoquer des "
        "téléchargements dans le mauvais ordre";

    const char* const CHANGELOG_TR =
        "- Yeni: Dil tabloları artık flash yerine SD karta "
        "kaydediliyor (kalıcı yedek olarak kalan İngilizce hariç) - "
        "yaklaşık 184KB / 9 yüzde puanı flash alanı kazandırıyor\n"
        "- Yeni: Bir dil dosyası yalnızca çevrilen metni gerçekten "
        "değiştiğinde otomatik olarak yeniden indiriliyor, her "
        "yazılım güncellemesinde değil\n"
        "- Yeni: SD kart eksikse, bir indirme başarısız olursa veya "
        "bir dil dosyası geçersiz/bozuksa İngilizceye sağlam bir "
        "şekilde geri dönülüyor\n"
        "- Düzeltme: bir dilin indirilmesi başarısız olduğunda dil "
        "seçimi artık İngilizcede takılı kalmıyor - diğer diller de "
        "deneniyor\n"
        "- Düzeltme: dil indirme kısıtlamasındaki, indirmelerin "
        "yanlış sırada gerçekleşmesine neden olabilecek bir zamanlama "
        "hatası";

    const char* const CHANGELOG_ES =
        "- Novedad: las tablas de idioma ahora se guardan en la "
        "tarjeta SD en lugar de estar integradas en la memoria flash "
        "(excepto el inglés, que permanece como respaldo permanente) "
        "- libera unos 184 KB / 9 puntos porcentuales de flash\n"
        "- Novedad: un archivo de idioma solo se vuelve a descargar "
        "automáticamente cuando su texto traducido realmente ha "
        "cambiado, no en cada actualización de firmware\n"
        "- Novedad: retorno automático robusto al inglés si falta la "
        "tarjeta SD, falla una descarga o un archivo de idioma es "
        "inválido o está dañado\n"
        "- Corrección: la selección de idioma ya no se queda atascada "
        "en inglés después de que falle la descarga de un idioma - "
        "ahora también se prueban los demás idiomas\n"
        "- Corrección: un error de sincronización en la limitación de "
        "descargas de idioma que podía provocar descargas en el orden "
        "incorrecto";

    const char* const CHANGELOG_IT =
        "- Novità: le tabelle delle lingue ora vengono salvate sulla "
        "scheda SD invece di essere integrate nella memoria flash "
        "(tranne l'inglese, che rimane come ripiego permanente) - "
        "libera circa 184 KB / 9 punti percentuali di flash\n"
        "- Novità: un file di lingua viene riscaricato automaticamente "
        "solo quando il suo testo tradotto è effettivamente cambiato, "
        "non a ogni aggiornamento del firmware\n"
        "- Novità: ripiego affidabile sull'inglese se la scheda SD "
        "manca, un download fallisce o un file di lingua non è "
        "valido o è danneggiato\n"
        "- Correzione: la selezione della lingua non rimane più "
        "bloccata sull'inglese dopo che il download di una lingua "
        "fallisce - vengono provate anche le altre lingue\n"
        "- Correzione: un bug di temporizzazione nella limitazione "
        "dei download delle lingue che poteva causare download "
        "nell'ordine sbagliato";

    const char* const CHANGELOG_PT =
        "- Novo: as tabelas de idioma agora são armazenadas no cartão "
        "SD em vez de embutidas na memória flash (exceto o inglês, "
        "que permanece como reserva permanente) - libera cerca de "
        "184 KB / 9 pontos percentuais de flash\n"
        "- Novo: um arquivo de idioma só é baixado novamente "
        "automaticamente quando seu texto traduzido realmente mudou, "
        "não a cada atualização de firmware\n"
        "- Novo: retorno automático robusto ao inglês se o cartão SD "
        "estiver ausente, um download falhar ou um arquivo de idioma "
        "for inválido ou estiver corrompido\n"
        "- Correção: a seleção de idioma não fica mais travada em "
        "inglês após a falha no download de um idioma - os outros "
        "idiomas também são tentados agora\n"
        "- Correção: um bug de temporização na limitação de downloads "
        "de idioma que podia causar downloads na ordem errada";

    const char* const CHANGELOG_NL =
        "- Nieuw: taaltabellen worden nu op de SD-kaart opgeslagen in "
        "plaats van ingebouwd in het flashgeheugen (behalve Engels, "
        "dat als permanente terugval blijft) - bespaart ongeveer 184 "
        "KB / 9 procentpunten flash\n"
        "- Nieuw: een taalbestand wordt alleen automatisch opnieuw "
        "gedownload wanneer de vertaalde tekst daadwerkelijk is "
        "gewijzigd, niet bij elke firmware-update\n"
        "- Nieuw: robuuste terugval naar Engels als de SD-kaart "
        "ontbreekt, een download mislukt, of een taalbestand ongeldig "
        "of beschadigd is\n"
        "- Fix: taalselectie blijft niet meer op Engels hangen nadat "
        "het downloaden van een taal is mislukt - de andere talen "
        "worden nu ook geprobeerd\n"
        "- Fix: een timingfout in de drempel voor taaldownloads die "
        "downloads in de verkeerde volgorde kon veroorzaken";

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
