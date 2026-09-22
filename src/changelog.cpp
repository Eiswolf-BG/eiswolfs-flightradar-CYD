#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: ntfy.sh push notifications now support quiet hours - "
        "pick a start/end hour to pause all push types (emergency "
        "squawk, watchlist hits, Flight Stories, approach alerts) "
        "without affecting the on-device display or LED alarms (Menu "
        "-> System -> ntfy.sh Push)\n"
        "- New: Weather screen now shows today's date next to "
        "sunrise/sunset\n"
        "- Fix: color scheme, range, and other settings changed from "
        "the web live radar now apply on the device immediately "
        "instead of waiting up to a few minutes for the next ADS-B "
        "update\n"
        "- Fix: after a failed ADS-B connection, retries now back off "
        "gradually (18s, 36s, 72s...) instead of retrying every 18 "
        "seconds indefinitely";

    const char* const CHANGELOG_DE =
        "- Neu: ntfy.sh-Push-Benachrichtigungen unterstuetzen jetzt "
        "Ruhezeiten - waehle eine Start-/Endstunde, um alle Push-"
        "Arten (Notfall-Squawk, Watchlist-Treffer, Flight Stories, "
        "Anflug-Alarm) zu pausieren, ohne die Anzeige oder LED-"
        "Alarme am Geraet zu beeinflussen (Menue -> System -> "
        "ntfy.sh Push)\n"
        "- Neu: Der Wetter-Screen zeigt jetzt das heutige Datum "
        "neben Sonnenauf-/-untergang\n"
        "- Fix: Farbschema, Reichweite und andere Einstellungen, die "
        "aus der Web-Livekarte geaendert werden, wirken jetzt sofort "
        "am Geraet, statt bis zu ein paar Minuten auf den naechsten "
        "ADS-B-Abruf zu warten\n"
        "- Fix: Nach einem fehlgeschlagenen ADS-B-Verbindungsversuch "
        "wird die Wartezeit jetzt schrittweise laenger (18s, 36s, "
        "72s ...), statt endlos alle 18 Sekunden erneut zu "
        "versuchen";

    const char* const CHANGELOG_FR =
        "- Nouveau : les notifications push ntfy.sh prennent en "
        "charge des heures de silence - choisissez une heure de "
        "début/fin pour suspendre tous les types de push (squawk "
        "d'urgence, alertes de liste de surveillance, Flight "
        "Stories, alertes d'approche) sans affecter l'affichage ou "
        "les alarmes LED de l'appareil (Menu -> Système -> ntfy.sh "
        "Push)\n"
        "- Nouveau : l'écran météo affiche désormais la date du "
        "jour à côté du lever/coucher du soleil\n"
        "- Correction : le thème de couleur, la portée et les "
        "autres réglages modifiés depuis le radar en direct du site "
        "web s'appliquent désormais immédiatement sur l'appareil, "
        "au lieu d'attendre jusqu'à quelques minutes la prochaine "
        "mise à jour ADS-B\n"
        "- Correction : après un échec de connexion ADS-B, les "
        "nouvelles tentatives s'espacent désormais progressivement "
        "(18s, 36s, 72s...) au lieu de réessayer toutes les 18 "
        "secondes indéfiniment";

    const char* const CHANGELOG_TR =
        "- Yeni: ntfy.sh push bildirimleri artık sessiz saatleri "
        "destekliyor - tüm push türlerini (acil durum squawk kodu, "
        "izleme listesi eşleşmeleri, Flight Stories, yaklaşma "
        "uyarıları) cihazdaki ekranı veya LED alarmlarını "
        "etkilemeden duraklatmak için bir başlangıç/bitiş saati "
        "seçin (Menü -> Sistem -> ntfy.sh Push)\n"
        "- Yeni: Hava durumu ekranı artık gün doğumu/batımı yanında "
        "bugünün tarihini gösteriyor\n"
        "- Düzeltme: web canlı radarından değiştirilen renk şeması, "
        "menzil ve diğer ayarlar artık bir sonraki ADS-B "
        "güncellemesini (bazen birkaç dakika) beklemek yerine "
        "cihazda hemen uygulanıyor\n"
        "- Düzeltme: başarısız bir ADS-B bağlantı denemesinden "
        "sonra, yeniden denemeler artık sürekli 18 saniyede bir "
        "değil, kademeli olarak uzuyor (18sn, 36sn, 72sn ...)";

    const char* const CHANGELOG_ES =
        "- Novedad: las notificaciones push de ntfy.sh ahora admiten "
        "horas silenciosas - elige una hora de inicio/fin para "
        "pausar todos los tipos de push (squawk de emergencia, "
        "coincidencias de listas de vigilancia, Flight Stories, "
        "alertas de aproximación) sin afectar la pantalla ni las "
        "alarmas LED del dispositivo (Menú -> Sistema -> ntfy.sh "
        "Push)\n"
        "- Novedad: la pantalla del tiempo ahora muestra la fecha de "
        "hoy junto al amanecer/atardecer\n"
        "- Corrección: el esquema de color, el alcance y otros "
        "ajustes cambiados desde el radar en vivo de la web ahora se "
        "aplican de inmediato en el dispositivo, en lugar de esperar "
        "hasta unos minutos a la siguiente actualización ADS-B\n"
        "- Corrección: tras un fallo de conexión ADS-B, los "
        "reintentos ahora se espacian progresivamente (18s, 36s, "
        "72s...) en lugar de reintentarlo cada 18 segundos "
        "indefinidamente";

    const char* const CHANGELOG_IT =
        "- Novità: le notifiche push ntfy.sh ora supportano le ore "
        "silenziose - scegli un'ora di inizio/fine per sospendere "
        "tutti i tipi di push (squawk di emergenza, corrispondenze "
        "delle liste di controllo, Flight Stories, avvisi di "
        "avvicinamento) senza influire sul display o sugli allarmi "
        "LED del dispositivo (Menu -> Sistema -> ntfy.sh Push)\n"
        "- Novità: la schermata meteo ora mostra la data odierna "
        "accanto ad alba/tramonto\n"
        "- Correzione: lo schema colori, il raggio e altre "
        "impostazioni modificate dal radar live del sito web ora si "
        "applicano immediatamente sul dispositivo, invece di "
        "attendere fino a qualche minuto il prossimo aggiornamento "
        "ADS-B\n"
        "- Correzione: dopo un tentativo di connessione ADS-B "
        "fallito, i nuovi tentativi ora si distanziano "
        "progressivamente (18s, 36s, 72s...) invece di riprovare "
        "ogni 18 secondi all'infinito";

    const char* const CHANGELOG_PT =
        "- Novo: as notificações push do ntfy.sh agora suportam "
        "horário silencioso - escolha uma hora de início/fim para "
        "pausar todos os tipos de push (squawk de emergência, "
        "correspondências de lista de observação, Flight Stories, "
        "alertas de aproximação) sem afetar a tela ou os alarmes de "
        "LED do aparelho (Menu -> Sistema -> ntfy.sh Push)\n"
        "- Novo: a tela de clima agora mostra a data de hoje ao "
        "lado do nascer/pôr do sol\n"
        "- Correção: o esquema de cores, o alcance e outras "
        "configurações alteradas pelo radar ao vivo do site agora "
        "são aplicados imediatamente no aparelho, em vez de esperar "
        "até alguns minutos pela próxima atualização ADS-B\n"
        "- Correção: após uma falha de conexão ADS-B, as novas "
        "tentativas agora aumentam progressivamente (18s, 36s, "
        "72s...) em vez de tentar novamente a cada 18 segundos "
        "indefinidamente";

    const char* const CHANGELOG_NL =
        "- Nieuw: ntfy.sh-pushmeldingen ondersteunen nu stille uren "
        "- kies een begin-/eindtijd om alle pushtypes (noodsquawk, "
        "volglijsttreffers, Flight Stories, naderingswaarschuwingen) "
        "te pauzeren zonder het scherm of de LED-alarmen op het "
        "apparaat te beïnvloeden (Menu -> Systeem -> ntfy.sh Push)\n"
        "- Nieuw: het weerscherm toont nu de datum van vandaag naast "
        "zonsopgang/zonsondergang\n"
        "- Fix: kleurenschema, bereik en andere instellingen die via "
        "de live radar op de website worden gewijzigd, worden nu "
        "direct op het apparaat toegepast in plaats van tot enkele "
        "minuten te wachten op de volgende ADS-B-update\n"
        "- Fix: na een mislukte ADS-B-verbindingspoging wordt de "
        "wachttijd nu geleidelijk langer (18s, 36s, 72s...) in "
        "plaats van eindeloos elke 18 seconden opnieuw te proberen";

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
