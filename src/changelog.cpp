#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the brief restart right before an OTA update download "
        "now shows a proper, easy-to-read status screen (\"Restarting "
        "for Update\") instead of a small one-line message\n"
        "- Fix: OTA updates now do a targeted restart right before "
        "the actual download, guaranteeing a fresh, unfragmented "
        "memory state - fixes rare \"update failed\" crashes at the "
        "very start of a download, and major download slowdowns/"
        "stutters after the device had been running for a while (in "
        "testing: from as much as 15 minutes at ~2 KB/s with stutters "
        "down to a consistent ~29 seconds at ~60 KB/s)";

    const char* const CHANGELOG_DE =
        "- Neu: Der kurze Neustart direkt vor einem OTA-Update-"
        "Download zeigt jetzt einen vollwertigen, gut lesbaren "
        "Status-Bildschirm (\"Neustart fuers Update\") statt einer "
        "kleinen einzeiligen Meldung\n"
        "- Fix: OTA-Updates machen jetzt einen gezielten Neustart "
        "direkt vor dem eigentlichen Download, um einen frischen, "
        "unfragmentierten Speicherzustand zu garantieren - behebt "
        "seltene Abstuerze ('Update fehlgeschlagen') gleich zu Beginn "
        "eines Downloads UND deutliche Download-Verlangsamungen/"
        "Stotterer, nachdem das Geraet schon eine Weile gelaufen war "
        "(im Test: von bis zu 15 Minuten bei ~2 KB/s mit Stotterern "
        "auf durchgehend ~29 Sekunden bei ~60 KB/s)";

    const char* const CHANGELOG_FR =
        "- Nouveau : le bref redémarrage juste avant le téléchargement "
        "d'une mise à jour OTA affiche désormais un véritable écran "
        "d'état bien lisible (« Redémarrage pour la mise à jour ») "
        "au lieu d'un petit message sur une ligne\n"
        "- Correction : les mises à jour OTA effectuent maintenant un "
        "redémarrage ciblé juste avant le téléchargement, garantissant "
        "un état mémoire frais et non fragmenté - corrige de rares "
        "plantages (« échec de la mise à jour ») dès le début d'un "
        "téléchargement, ainsi que des ralentissements/blocages "
        "importants du téléchargement après un fonctionnement "
        "prolongé de l'appareil (en test : jusqu'à 15 minutes à "
        "~2 Ko/s avec blocages, ramenées à ~29 secondes constantes à "
        "~60 Ko/s)";

    const char* const CHANGELOG_TR =
        "- Yeni: Bir OTA güncelleme indirmesinden hemen önceki kısa "
        "yeniden başlatma artık küçük tek satırlık bir mesaj yerine "
        "düzgün, kolay okunur bir durum ekranı (\"Güncelleme İçin "
        "Yeniden Başlatılıyor\") gösteriyor\n"
        "- Düzeltme: OTA güncellemeleri artık asıl indirmeden hemen "
        "önce hedefli bir yeniden başlatma yapıyor, taze ve "
        "parçalanmamış bir bellek durumu garanti ediyor - bir "
        "indirmenin en başında görülen nadir \"güncelleme başarısız\" "
        "çökmelerini VE cihaz bir süredir çalışıyorken ortaya çıkan "
        "ciddi indirme yavaşlamalarını/takılmalarını gideriyor (testte: "
        "takılmalarla birlikte ~2 KB/s hızında 15 dakikaya kadar süren "
        "indirmelerden, düzenli olarak ~60 KB/s hızında ~29 saniyeye)";

    const char* const CHANGELOG_ES =
        "- Novedad: el breve reinicio justo antes de la descarga de "
        "una actualización OTA ahora muestra una pantalla de estado "
        "completa y fácil de leer (\"Reiniciando para la "
        "Actualización\") en lugar de un pequeño mensaje de una línea\n"
        "- Corrección: las actualizaciones OTA ahora realizan un "
        "reinicio específico justo antes de la descarga real, "
        "garantizando un estado de memoria fresco y sin fragmentar - "
        "corrige fallos poco frecuentes (\"actualización fallida\") "
        "justo al inicio de una descarga, y ralentizaciones/"
        "bloqueos importantes de la descarga tras un funcionamiento "
        "prolongado del dispositivo (en pruebas: de hasta 15 minutos "
        "a ~2 KB/s con bloqueos, a unos 29 segundos constantes a "
        "~60 KB/s)";

    const char* const CHANGELOG_IT =
        "- Novità: il breve riavvio subito prima del download di un "
        "aggiornamento OTA ora mostra una vera schermata di stato "
        "ben leggibile (\"Riavvio per l'Aggiornamento\") invece di un "
        "piccolo messaggio su una riga\n"
        "- Correzione: gli aggiornamenti OTA ora eseguono un riavvio "
        "mirato subito prima del download vero e proprio, garantendo "
        "uno stato di memoria fresco e non frammentato - corregge "
        "rari blocchi (\"aggiornamento non riuscito\") proprio "
        "all'inizio di un download, e forti rallentamenti/blocchi del "
        "download dopo che il dispositivo era rimasto acceso a lungo "
        "(nei test: da fino a 15 minuti a ~2 KB/s con blocchi, a "
        "circa 29 secondi costanti a ~60 KB/s)";

    const char* const CHANGELOG_PT =
        "- Novo: a breve reinicialização logo antes do download de "
        "uma atualização OTA agora mostra uma tela de status completa "
        "e fácil de ler (\"Reiniciando para a Atualização\") em vez "
        "de uma pequena mensagem de uma linha\n"
        "- Correção: as atualizações OTA agora fazem uma "
        "reinicialização direcionada logo antes do download "
        "propriamente dito, garantindo um estado de memória limpo e "
        "não fragmentado - corrige falhas raras (\"atualização "
        "falhou\") bem no início de um download, e lentidões/travamentos "
        "importantes do download depois que o dispositivo já estava "
        "ligado há um tempo (em testes: de até 15 minutos a ~2 KB/s "
        "com travamentos, para cerca de 29 segundos constantes a "
        "~60 KB/s)";

    const char* const CHANGELOG_NL =
        "- Nieuw: de korte herstart vlak voor het downloaden van een "
        "OTA-update toont nu een volwaardig, goed leesbaar "
        "statusscherm (\"Herstart voor Update\") in plaats van een "
        "klein eenregelig berichtje\n"
        "- Fix: OTA-updates voeren nu een gerichte herstart uit vlak "
        "voor de eigenlijke download, wat een frisse, niet-"
        "gefragmenteerde geheugenstatus garandeert - lost zeldzame "
        "crashes (\"update mislukt\") aan het begin van een download "
        "op, EN forse download-vertragingen/haperingen nadat het "
        "apparaat al een tijdje draaide (in tests: van soms 15 "
        "minuten bij ~2 KB/s met haperingen naar consistent ~29 "
        "seconden bij ~60 KB/s)";

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
