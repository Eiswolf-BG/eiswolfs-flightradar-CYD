#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: notification when the flight logbook gets disabled "
        "by the automatic 24-hour safety shutoff - even if the "
        "device was powered off in the meantime\n"
        "- New: the aircraft detail panel now shows when an "
        "aircraft was first seen in this session and how long it "
        "has been continuously visible\n"
        "- New: aircraft markers briefly pulse when they cross one "
        "of the distance rings on the radar\n"
        "- New: the aircraft detail panel now shows how many times "
        "an aircraft has been logged before and when it was last "
        "seen\n"
        "- New: new statistic - today's peak number of "
        "simultaneously visible aircraft, with the time it "
        "happened";

    const char* const CHANGELOG_DE =
        "- Neu: Hinweis, wenn das Flugbuch durch die automatische "
        "24h-Abschaltung deaktiviert wurde - auch wenn das Gerät "
        "währenddessen ausgeschaltet war\n"
        "- Neu: Flugzeug-Detail-Panel zeigt jetzt, wann ein "
        "Flugzeug erstmals in dieser Sitzung gesehen wurde und wie "
        "lange es schon durchgehend sichtbar ist\n"
        "- Neu: Flugzeug-Marker pulsieren kurz, wenn sie einen der "
        "Entfernungsringe auf dem Radar durchqueren\n"
        "- Neu: Flugzeug-Detail-Panel zeigt jetzt, wie oft ein "
        "Flugzeug bereits zuvor geloggt wurde und wann zuletzt\n"
        "- Neu: Neue Statistik - heutiger Höchstwert gleichzeitig "
        "sichtbarer Flugzeuge, mit Uhrzeit";

    const char* const CHANGELOG_FR =
        "- Nouveau : notification lorsque le journal de vol est "
        "désactivé par l'arrêt de sécurité automatique de 24h - "
        "même si l'appareil était éteint entre-temps\n"
        "- Nouveau : le panneau de détails de l'avion indique "
        "désormais quand un avion a été vu pour la première fois "
        "dans cette session et depuis combien de temps il est "
        "visible en continu\n"
        "- Nouveau : les marqueurs d'avion pulsent brièvement "
        "lorsqu'ils traversent l'un des cercles de distance du "
        "radar\n"
        "- Nouveau : le panneau de détails de l'avion indique "
        "désormais combien de fois un avion a déjà été enregistré "
        "et quand pour la dernière fois\n"
        "- Nouveau : nouvelle statistique - pic du jour du nombre "
        "d'avions visibles simultanément, avec l'heure";

    const char* const CHANGELOG_TR =
        "- Yeni: uçuş defteri otomatik 24 saatlik güvenlik "
        "kapatması nedeniyle devre dışı bırakıldığında bildirim - "
        "cihaz bu süre zarfında kapalı olsa bile\n"
        "- Yeni: uçak detay paneli artık bir uçağın bu oturumda "
        "ilk ne zaman görüldüğünü ve ne kadar süredir kesintisiz "
        "görünür olduğunu gösteriyor\n"
        "- Yeni: uçak işaretleri, radardaki mesafe halkalarından "
        "birini geçtiklerinde kısa süreliğine yanıp sönüyor\n"
        "- Yeni: uçak detay paneli artık bir uçağın daha önce kaç "
        "kez kaydedildiğini ve en son ne zaman görüldüğünü "
        "gösteriyor\n"
        "- Yeni: yeni istatistik - bugün aynı anda görülen en "
        "fazla uçak sayısı, saatiyle birlikte";

    const char* const CHANGELOG_ES =
        "- Novedad: aviso cuando el cuaderno de vuelo se desactiva "
        "por el apagado de seguridad automático de 24 horas - "
        "incluso si el dispositivo estuvo apagado mientras tanto\n"
        "- Novedad: el panel de detalles del avión ahora muestra "
        "cuándo se vio un avión por primera vez en esta sesión y "
        "cuánto tiempo lleva visible de forma continua\n"
        "- Novedad: los marcadores de avión parpadean brevemente "
        "al cruzar uno de los círculos de distancia del radar\n"
        "- Novedad: el panel de detalles del avión ahora muestra "
        "cuántas veces se ha registrado antes un avión y cuándo "
        "fue la última vez\n"
        "- Novedad: nueva estadística - pico de hoy de aviones "
        "visibles simultáneamente, con la hora";

    const char* const CHANGELOG_IT =
        "- Novità: avviso quando il diario di volo viene "
        "disattivato dallo spegnimento di sicurezza automatico "
        "delle 24 ore - anche se il dispositivo era spento nel "
        "frattempo\n"
        "- Novità: il pannello dei dettagli dell'aereo ora mostra "
        "quando un aereo è stato visto per la prima volta in "
        "questa sessione e da quanto tempo è visibile "
        "ininterrottamente\n"
        "- Novità: i marcatori degli aerei pulsano brevemente "
        "quando attraversano uno degli anelli di distanza sul "
        "radar\n"
        "- Novità: il pannello dei dettagli dell'aereo ora mostra "
        "quante volte un aereo è già stato registrato in "
        "precedenza e quando l'ultima volta\n"
        "- Novità: nuova statistica - picco odierno di aerei "
        "visibili contemporaneamente, con l'orario";

    const char* const CHANGELOG_PT =
        "- Novo: aviso quando o diário de bordo é desativado pelo "
        "desligamento de segurança automático de 24 horas - mesmo "
        "que o aparelho tenha ficado desligado nesse período\n"
        "- Novo: o painel de detalhes da aeronave agora mostra "
        "quando uma aeronave foi vista pela primeira vez nesta "
        "sessão e há quanto tempo está visível continuamente\n"
        "- Novo: os marcadores de aeronaves piscam brevemente ao "
        "cruzar um dos círculos de distância no radar\n"
        "- Novo: o painel de detalhes da aeronave agora mostra "
        "quantas vezes uma aeronave já foi registrada antes e "
        "quando foi a última vez\n"
        "- Novo: nova estatística - pico de hoje de aeronaves "
        "visíveis simultaneamente, com o horário";

    const char* const CHANGELOG_NL =
        "- Nieuw: melding wanneer het logboek wordt uitgeschakeld "
        "door de automatische 24-uurs veiligheidsuitschakeling - "
        "zelfs als het apparaat ondertussen uit stond\n"
        "- Nieuw: het vliegtuig-detailpaneel toont nu wanneer een "
        "vliegtuig voor het eerst in deze sessie is gezien en hoe "
        "lang het al ononderbroken zichtbaar is\n"
        "- Nieuw: vliegtuigmarkeringen knipperen kort wanneer ze "
        "een van de afstandsringen op de radar kruisen\n"
        "- Nieuw: het vliegtuig-detailpaneel toont nu hoe vaak een "
        "vliegtuig al eerder is gelogd en wanneer voor het laatst\n"
        "- Nieuw: nieuwe statistiek - piek van vandaag van "
        "gelijktijdig zichtbare vliegtuigen, met tijdstip";

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
