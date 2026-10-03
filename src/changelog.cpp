#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: Watchlist callsigns now support wildcards (e.g. "
        "\"DLH*\" matches any Lufthansa flight)\n"
        "- New: Detail panel shows an audibility estimate "
        "(likely/maybe/barely audible) based on aircraft type and "
        "distance\n"
        "- New: Expanded MQTT/Home Assistant integration with more "
        "sensors, plus new ntfy push events (daily traffic record, "
        "first military/heavy flight of the day, logbook auto-off "
        "warning)\n"
        "- New: Session Highlights - daily stats on the Statistics "
        "screen (longest tracked, fastest climb/descent, first/last "
        "seen today, most returns)\n"
        "- New: Tapping the aircraft row on the screensaver recalls "
        "the last tracked aircraft (\"What was that?\")\n"
        "- New: Detail panel now shows overhead/passing-left/"
        "passing-right indicators and a radar-exit ETA\n"
        "- Removed: ISS marker\n"
        "- Fix: internal stability and reliability improvements";

    const char* const CHANGELOG_DE =
        "- Neu: Rufzeichen in den Wachlisten unterstützen jetzt "
        "Platzhalter (z.B. \"DLH*\" passt auf jeden Lufthansa-Flug)\n"
        "- Neu: Das Detail-Panel zeigt jetzt eine Hörbarkeits-"
        "Einschätzung (wahrscheinlich/eventuell/kaum hörbar) je "
        "nach Flugzeugtyp und Distanz\n"
        "- Neu: Erweiterte MQTT/Home-Assistant-Anbindung mit mehr "
        "Sensoren, plus neue ntfy-Push-Events (Tages-Verkehrsrekord, "
        "erster Militär-/Heavy-Flug des Tages, Flugbuch-Auto-Off-"
        "Warnung)\n"
        "- Neu: Session-Highlights - tägliche Statistiken im "
        "Statistik-Screen (längste Verfolgung, schnellster Steig-/"
        "Sinkflug, zuerst/zuletzt heute gesehen, meiste Rückkehrer)\n"
        "- Neu: Ein Tap auf die Flugzeugzeile im Ruhebildschirm zeigt "
        "das zuletzt verfolgte Flugzeug (\"Was war das gerade?\")\n"
        "- Neu: Das Detail-Panel zeigt jetzt Overhead-/Vorbeiflug-"
        "links/rechts-Hinweise und eine Reichweiten-Austritts-ETA\n"
        "- Entfernt: ISS-Marker\n"
        "- Fix: interne Stabilitäts- und Zuverlässigkeits-"
        "Verbesserungen";

    const char* const CHANGELOG_FR =
        "- Nouveau : les indicatifs des listes de surveillance "
        "prennent désormais en charge les caractères génériques "
        "(ex. « DLH* » correspond à tout vol Lufthansa)\n"
        "- Nouveau : le panneau de détail affiche désormais une "
        "estimation d'audibilité (probablement/peut-être/à peine "
        "audible) selon le type d'avion et la distance\n"
        "- Nouveau : intégration MQTT/Home Assistant étendue avec "
        "plus de capteurs, ainsi que de nouveaux événements push "
        "ntfy (record de trafic du jour, premier vol militaire/Heavy "
        "du jour, avertissement d'arrêt automatique du carnet de "
        "vol)\n"
        "- Nouveau : Points forts de session - nouvelles statistiques "
        "quotidiennes sur l'écran Statistiques (suivi le plus long, "
        "montée/descente la plus rapide, premier/dernier vu "
        "aujourd'hui, plus de retours)\n"
        "- Nouveau : un appui sur la ligne de l'avion sur l'écran de "
        "veille rappelle le dernier avion suivi (« Qu'est-ce que "
        "c'était ? »)\n"
        "- Nouveau : le panneau de détail affiche désormais des "
        "indicateurs à la verticale/passe à gauche/passe à droite et "
        "une ETA de sortie de portée\n"
        "- Supprimé : marqueur ISS\n"
        "- Correction : améliorations internes de stabilité et de "
        "fiabilité";

    const char* const CHANGELOG_TR =
        "- Yeni: İzleme listesi çağrı işaretleri artık joker "
        "karakterleri destekliyor (örn. \"DLH*\" herhangi bir "
        "Lufthansa uçuşuyla eşleşir)\n"
        "- Yeni: Detay paneli artık uçak tipine ve mesafeye göre bir "
        "işitilebilirlik tahmini gösteriyor (muhtemelen/belki/zar "
        "zor işitilir)\n"
        "- Yeni: Daha fazla sensörle genişletilmiş MQTT/Home "
        "Assistant entegrasyonu, ayrıca yeni ntfy push olayları "
        "(günlük trafik rekoru, günün ilk askeri/Heavy uçuşu, uçuş "
        "defteri otomatik kapanma uyarısı)\n"
        "- Yeni: Oturum Öne Çıkanları - İstatistik ekranında yeni "
        "günlük istatistikler (en uzun takip, en hızlı tırmanış/iniş, "
        "bugün ilk/son görülen, en çok dönen)\n"
        "- Yeni: Bekleme ekranındaki uçak satırına dokunmak son "
        "takip edilen uçağı geri çağırır (\"Az önce ne oldu?\")\n"
        "- Yeni: Detay paneli artık tam üstünde/soldan geçiyor/"
        "sağdan geçiyor göstergelerini ve menzil çıkış ETA'sını "
        "gösteriyor\n"
        "- Kaldırıldı: ISS işareti\n"
        "- Düzeltme: dahili kararlılık ve güvenilirlik "
        "iyileştirmeleri";

    const char* const CHANGELOG_ES =
        "- Novedad: los indicativos de las listas de vigilancia "
        "ahora admiten comodines (p. ej. «DLH*» coincide con "
        "cualquier vuelo de Lufthansa)\n"
        "- Novedad: el panel de detalles ahora muestra una "
        "estimación de audibilidad (probablemente/quizás/apenas "
        "audible) según el tipo de aeronave y la distancia\n"
        "- Novedad: integración MQTT/Home Assistant ampliada con más "
        "sensores, además de nuevos eventos push de ntfy (récord de "
        "tráfico diario, primer vuelo militar/Heavy del día, aviso "
        "de desactivación automática del diario de vuelo)\n"
        "- Novedad: Momentos destacados de la sesión - nuevas "
        "estadísticas diarias en la pantalla de Estadísticas "
        "(seguimiento más largo, ascenso/descenso más rápido, "
        "primero/último visto hoy, más regresos)\n"
        "- Novedad: tocar la fila de la aeronave en el salvapantallas "
        "recupera la última aeronave seguida (¿Qué fue eso?)\n"
        "- Novedad: el panel de detalles ahora muestra indicadores de "
        "vertical/pasa por la izquierda/pasa por la derecha y una "
        "ETA de salida de alcance\n"
        "- Eliminado: marcador de la ISS\n"
        "- Corrección: mejoras internas de estabilidad y fiabilidad";

    const char* const CHANGELOG_IT =
        "- Novità: i nominativi nelle liste di controllo ora "
        "supportano i caratteri jolly (es. \"DLH*\" corrisponde a "
        "qualsiasi volo Lufthansa)\n"
        "- Novità: il pannello dei dettagli ora mostra una stima di "
        "udibilità (probabilmente/forse/appena udibile) in base al "
        "tipo di aereo e alla distanza\n"
        "- Novità: integrazione MQTT/Home Assistant estesa con più "
        "sensori, oltre a nuovi eventi push ntfy (record di traffico "
        "giornaliero, primo volo militare/Heavy del giorno, avviso "
        "di disattivazione automatica del diario di volo)\n"
        "- Novità: Momenti salienti della sessione - nuove "
        "statistiche giornaliere nella schermata Statistiche "
        "(tracciamento più lungo, salita/discesa più rapida, primo/"
        "ultimo visto oggi, più ritorni)\n"
        "- Novità: toccare la riga dell'aereo sul salvaschermo "
        "richiama l'ultimo aereo tracciato (\"Cos'era?\")\n"
        "- Novità: il pannello dei dettagli ora mostra indicatori "
        "verticale/passa a sinistra/passa a destra e un ETA di "
        "uscita dalla portata\n"
        "- Rimosso: indicatore ISS\n"
        "- Correzione: miglioramenti interni di stabilità e "
        "affidabilità";

    const char* const CHANGELOG_PT =
        "- Novo: os indicativos das listas de observação agora "
        "suportam caracteres universais (ex.: \"DLH*\" corresponde a "
        "qualquer voo da Lufthansa)\n"
        "- Novo: o painel de detalhes agora mostra uma estimativa de "
        "audibilidade (provavelmente/talvez/pouco audível) de "
        "acordo com o tipo de aeronave e a distância\n"
        "- Novo: integração MQTT/Home Assistant expandida com mais "
        "sensores, além de novos eventos push do ntfy (recorde de "
        "tráfego diário, primeiro voo militar/Heavy do dia, aviso "
        "de desativação automática do diário de voo)\n"
        "- Novo: Destaques da Sessão - novas estatísticas diárias na "
        "tela de Estatísticas (rastreamento mais longo, subida/"
        "descida mais rápida, primeiro/último visto hoje, mais "
        "retornos)\n"
        "- Novo: tocar na linha da aeronave na proteção de tela "
        "recupera a última aeronave rastreada (\"O que foi isso?\")\n"
        "- Novo: o painel de detalhes agora mostra indicadores de "
        "vertical/passando pela esquerda/direita e uma ETA de saída "
        "de alcance\n"
        "- Removido: marcador da ISS\n"
        "- Correção: melhorias internas de estabilidade e "
        "confiabilidade";

    const char* const CHANGELOG_NL =
        "- Nieuw: roepnamen in de volglijsten ondersteunen nu "
        "jokertekens (bijv. \"DLH*\" komt overeen met elke Lufthansa-"
        "vlucht)\n"
        "- Nieuw: het detailpaneel toont nu een "
        "hoorbaarheidsinschatting (waarschijnlijk/misschien/"
        "nauwelijks hoorbaar) op basis van vliegtuigtype en afstand\n"
        "- Nieuw: uitgebreide MQTT/Home Assistant-integratie met "
        "meer sensoren, plus nieuwe ntfy-pushmeldingen (dagelijks "
        "verkeersrecord, eerste militaire/Heavy-vlucht van de dag, "
        "waarschuwing voor automatisch uitschakelen van het "
        "vluchtlogboek)\n"
        "- Nieuw: Sessiehighlights - nieuwe dagelijkse statistieken "
        "op het statistiekenscherm (langste tracking, snelste klim/"
        "daling, eerst/laatst vandaag gezien, meeste terugkeerders)\n"
        "- Nieuw: tikken op de vliegtuigregel op de "
        "schermbeveiliging roept het laatst gevolgde vliegtuig weer "
        "op (\"Wat was dat net?\")\n"
        "- Nieuw: het detailpaneel toont nu indicatoren voor recht "
        "boven je/passeert links/passeert rechts en een bereik-"
        "uitgang-ETA\n"
        "- Verwijderd: ISS-marker\n"
        "- Fix: interne stabiliteits- en betrouwbaarheids-"
        "verbeteringen";

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
