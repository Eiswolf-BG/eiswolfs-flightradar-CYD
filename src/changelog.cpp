#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: Flight Stories - automatic ntfy push notifications "
        "when a military/government aircraft, a helicopter, or an "
        "unusually low-flying aircraft is spotted nearby (respects "
        "range and filters); shares the emergency/watchlist push "
        "slot (those always take priority) and suppresses repeats "
        "per aircraft for 10 minutes (Menu -> System -> ntfy.sh "
        "Push)\n"
        "- New: The QR code on the About screen is now much larger "
        "and centered vertically\n"
        "- New: The update screens (installing/installed) now show "
        "the same GitHub QR code at the bottom instead of the small "
        "logo";

    const char* const CHANGELOG_DE =
        "- Neu: Flight Stories - automatische ntfy-Push-"
        "Benachrichtigungen, wenn ein Militaer-/Behoerdenflugzeug, "
        "ein Hubschrauber oder ein ungewoehnlich tief fliegendes "
        "Flugzeug in der Naehe entdeckt wird (beruecksichtigt "
        "Reichweite und Filter); teilt sich den Notfall-/Wachlisten-"
        "Sendeplatz (die haben immer Vorrang) und unterdrueckt "
        "Wiederholungen pro Flugzeug fuer 10 Minuten (Menue -> "
        "System -> ntfy.sh Push)\n"
        "- Neu: Der QR-Code im Ueber-Bildschirm ist jetzt deutlich "
        "groesser und vertikal zentriert\n"
        "- Neu: Die Update-Bildschirme (Installation/Installiert) "
        "zeigen unten jetzt denselben GitHub-QR-Code statt des "
        "kleinen Logos";

    const char* const CHANGELOG_FR =
        "- Nouveau : Flight Stories - notifications push ntfy "
        "automatiques lorsqu'un avion militaire/gouvernemental, un "
        "hélicoptère ou un vol à basse altitude inhabituel est "
        "repéré à proximité (respecte la portée et les filtres) ; "
        "partage l'emplacement d'envoi urgence/liste de surveillance "
        "(qui ont toujours la priorité) et supprime les répétitions "
        "par avion pendant 10 minutes (Menu -> Système -> Push "
        "ntfy.sh)\n"
        "- Nouveau : Le code QR de l'écran À propos est désormais "
        "beaucoup plus grand et centré verticalement\n"
        "- Nouveau : Les écrans de mise à jour (installation/"
        "installée) affichent désormais le même code QR GitHub en "
        "bas au lieu du petit logo";

    const char* const CHANGELOG_TR =
        "- Yeni: Flight Stories - yakında bir askeri/resmi uçak, bir "
        "helikopter veya alışılmadık derecede alçak uçan bir uçak "
        "tespit edildiğinde otomatik ntfy push bildirimleri (menzil "
        "ve filtreleri dikkate alır); acil durum/izleme listesi "
        "gönderim yuvasını paylaşır (bunlar her zaman önceliklidir) "
        "ve uçak başına tekrarları 10 dakika boyunca bastırır (Menü "
        "-> Sistem -> ntfy.sh Push)\n"
        "- Yeni: Hakkında ekranındaki QR kodu artık çok daha büyük "
        "ve dikey olarak ortalanmış\n"
        "- Yeni: Güncelleme ekranları (yükleniyor/yüklendi) artık "
        "altta küçük logo yerine aynı GitHub QR kodunu gösteriyor";

    const char* const CHANGELOG_ES =
        "- Novedad: Flight Stories - notificaciones push de ntfy "
        "automáticas cuando se detecta cerca una aeronave militar/"
        "gubernamental, un helicóptero o un vuelo a baja altitud "
        "inusual (respeta el alcance y los filtros); comparte la "
        "ranura de envío de emergencia/lista de vigilancia (que "
        "siempre tienen prioridad) y suprime las repeticiones por "
        "aeronave durante 10 minutos (Menú -> Sistema -> Push "
        "ntfy.sh)\n"
        "- Novedad: El código QR de la pantalla Acerca de ahora es "
        "mucho más grande y está centrado verticalmente\n"
        "- Novedad: Las pantallas de actualización (instalando/"
        "instalada) ahora muestran el mismo código QR de GitHub en "
        "la parte inferior en lugar del pequeño logotipo";

    const char* const CHANGELOG_IT =
        "- Novità: Flight Stories - notifiche push ntfy automatiche "
        "quando viene avvistato nelle vicinanze un aereo militare/"
        "governativo, un elicottero o un volo a bassa quota "
        "insolito (rispetta portata e filtri); condivide lo slot di "
        "invio emergenza/lista di controllo (che hanno sempre la "
        "priorità) e sopprime le ripetizioni per aereo per 10 minuti "
        "(Menu -> Sistema -> Push ntfy.sh)\n"
        "- Novità: Il codice QR nella schermata Informazioni è ora "
        "molto più grande e centrato verticalmente\n"
        "- Novità: Le schermate di aggiornamento (installazione/"
        "installato) mostrano ora lo stesso codice QR di GitHub in "
        "basso al posto del piccolo logo";

    const char* const CHANGELOG_PT =
        "- Novo: Flight Stories - notificações push ntfy "
        "automáticas quando uma aeronave militar/governamental, um "
        "helicóptero ou um voo a baixa altitude incomum é avistado "
        "nas proximidades (respeita o alcance e os filtros); "
        "compartilha o slot de envio de emergência/lista de "
        "observação (que sempre têm prioridade) e suprime "
        "repetições por aeronave durante 10 minutos (Menu -> "
        "Sistema -> Push ntfy.sh)\n"
        "- Novo: O código QR na tela Sobre agora é muito maior e "
        "está centralizado verticalmente\n"
        "- Novo: As telas de atualização (instalando/instalado) "
        "agora mostram o mesmo código QR do GitHub na parte "
        "inferior em vez do pequeno logotipo";

    const char* const CHANGELOG_NL =
        "- Nieuw: Flight Stories - automatische ntfy-pushmeldingen "
        "wanneer een militair/overheidsvliegtuig, een helikopter of "
        "een ongewoon laag vliegend vliegtuig in de buurt wordt "
        "gespot (houdt rekening met bereik en filters); deelt de "
        "nood-/volglijst-verzendplek (die altijd voorrang heeft) en "
        "onderdrukt herhalingen per vliegtuig gedurende 10 minuten "
        "(Menu -> Systeem -> ntfy.sh Push)\n"
        "- Nieuw: De QR-code op het Over-scherm is nu veel groter en "
        "verticaal gecentreerd\n"
        "- Nieuw: De update-schermen (installeren/geïnstalleerd) "
        "tonen nu onderaan dezelfde GitHub QR-code in plaats van het "
        "kleine logo";

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
