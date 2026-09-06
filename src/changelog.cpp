#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: countdown to the automatic 24-hour shutoff shown "
        "right on the flight logbook line, as long as the logbook "
        "is switched on\n"
        "- New: the aircraft detail panel now also shows a typical "
        "time window and altitude range once an aircraft has been "
        "logged several times before\n"
        "- New: \"Live Traffic\" screen with a compact overview of "
        "current traffic - total count, breakdown by aircraft type, "
        "plus the nearest, highest, lowest, and fastest aircraft\n"
        "- New: \"Connection Status\" screen shows the number of "
        "tracked aircraft, time since the last successful data "
        "fetch, the result of the last fetch attempt, and a simple "
        "traffic-light status";

    const char* const CHANGELOG_DE =
        "- Neu: Countdown bis zur automatischen 24h-Abschaltung "
        "direkt an der Flugbuch-Zeile, solange das Flugbuch "
        "eingeschaltet ist\n"
        "- Neu: Flugzeug-Detail-Panel zeigt jetzt zusätzlich ein "
        "typisches Zeitfenster und einen typischen Höhenbereich, "
        "sobald ein Flugzeug bereits mehrfach zuvor geloggt wurde\n"
        "- Neu: Neuer \"Live-Verkehr\"-Screen mit einer kompakten "
        "Übersicht des aktuellen Verkehrs - Gesamtanzahl, "
        "Aufschlüsselung nach Flugzeugtyp, sowie das nächste, "
        "höchste, niedrigste und schnellste Flugzeug\n"
        "- Neu: Neuer \"Verbindungsstatus\"-Screen zeigt Anzahl "
        "getrackter Flugzeuge, Zeit seit dem letzten erfolgreichen "
        "Datenabruf, das Ergebnis des letzten Abrufversuchs sowie "
        "einen einfachen Ampel-Status";

    const char* const CHANGELOG_FR =
        "- Nouveau : compte à rebours jusqu'à l'arrêt automatique "
        "de 24h affiché directement sur la ligne du journal de vol, "
        "tant que celui-ci est activé\n"
        "- Nouveau : le panneau de détails de l'avion indique "
        "désormais aussi une plage horaire et une plage d'altitude "
        "typiques dès qu'un avion a déjà été enregistré plusieurs "
        "fois\n"
        "- Nouveau : nouvel écran \"Trafic en direct\" avec un "
        "aperçu compact du trafic actuel - nombre total, répartition "
        "par type d'avion, ainsi que l'avion le plus proche, le "
        "plus haut, le plus bas et le plus rapide\n"
        "- Nouveau : nouvel écran \"État de la connexion\" indique "
        "le nombre d'avions suivis, le temps écoulé depuis la "
        "dernière récupération de données réussie, le résultat de "
        "la dernière tentative et un simple statut à code couleur";

    const char* const CHANGELOG_TR =
        "- Yeni: uçuş defteri açık olduğu sürece, otomatik 24 "
        "saatlik kapanmaya kalan süre doğrudan uçuş defteri "
        "satırında gösteriliyor\n"
        "- Yeni: bir uçak daha önce birkaç kez kaydedilmişse, uçak "
        "detay paneli artık tipik bir zaman aralığı ve irtifa "
        "aralığı da gösteriyor\n"
        "- Yeni: güncel trafiğin kompakt bir özetini sunan yeni "
        "\"Canlı Trafik\" ekranı - toplam sayı, uçak tipine göre "
        "dağılım, ayrıca en yakın, en yüksek, en alçak ve en hızlı "
        "uçak\n"
        "- Yeni: yeni \"Bağlantı Durumu\" ekranı, takip edilen uçak "
        "sayısını, son başarılı veri alımından bu yana geçen "
        "süreyi, son deneme sonucunu ve basit bir trafik ışığı "
        "durumunu gösteriyor";

    const char* const CHANGELOG_ES =
        "- Novedad: cuenta atrás hasta el apagado automático de 24h "
        "mostrada directamente en la línea del cuaderno de vuelo, "
        "mientras esté activado\n"
        "- Novedad: el panel de detalles del avión ahora también "
        "muestra una franja horaria y un rango de altitud típicos "
        "en cuanto un avión ha sido registrado varias veces antes\n"
        "- Novedad: nueva pantalla \"Tráfico en vivo\" con un "
        "resumen compacto del tráfico actual - número total, "
        "desglose por tipo de avión, además del avión más cercano, "
        "más alto, más bajo y más rápido\n"
        "- Novedad: nueva pantalla \"Estado de la conexión\" muestra "
        "el número de aviones rastreados, el tiempo desde la última "
        "recepción de datos exitosa, el resultado del último "
        "intento y un sencillo estado tipo semáforo";

    const char* const CHANGELOG_IT =
        "- Novità: conto alla rovescia fino allo spegnimento "
        "automatico delle 24 ore mostrato direttamente sulla riga "
        "del diario di volo, finché è attivo\n"
        "- Novità: il pannello dei dettagli dell'aereo ora mostra "
        "anche una fascia oraria e un intervallo di quota tipici "
        "non appena un aereo è già stato registrato più volte in "
        "precedenza\n"
        "- Novità: nuova schermata \"Traffico in diretta\" con una "
        "panoramica compatta del traffico attuale - numero totale, "
        "suddivisione per tipo di aereo, oltre all'aereo più "
        "vicino, più alto, più basso e più veloce\n"
        "- Novità: nuova schermata \"Stato della connessione\" "
        "mostra il numero di aerei tracciati, il tempo trascorso "
        "dall'ultimo recupero dati riuscito, l'esito dell'ultimo "
        "tentativo e un semplice stato a semaforo";

    const char* const CHANGELOG_PT =
        "- Novo: contagem regressiva até o desligamento automático "
        "de 24h exibida diretamente na linha do diário de bordo, "
        "enquanto ele estiver ativado\n"
        "- Novo: o painel de detalhes da aeronave agora também "
        "mostra uma janela de horário e uma faixa de altitude "
        "típicas assim que uma aeronave já foi registrada várias "
        "vezes antes\n"
        "- Novo: nova tela \"Tráfego ao vivo\" com um resumo "
        "compacto do tráfego atual - contagem total, discriminação "
        "por tipo de aeronave, além da aeronave mais próxima, mais "
        "alta, mais baixa e mais rápida\n"
        "- Novo: nova tela \"Estado da conexão\" mostra o número de "
        "aeronaves rastreadas, o tempo desde a última busca de "
        "dados bem-sucedida, o resultado da última tentativa e um "
        "status simples do tipo semáforo";

    const char* const CHANGELOG_NL =
        "- Nieuw: aftelling tot de automatische 24-uurs "
        "uitschakeling direct op de logboekregel, zolang het "
        "logboek is ingeschakeld\n"
        "- Nieuw: het vliegtuig-detailpaneel toont nu ook een "
        "typisch tijdvenster en een typisch hoogtebereik zodra een "
        "vliegtuig al meerdere keren eerder is gelogd\n"
        "- Nieuw: nieuw \"Live verkeer\"-scherm met een compact "
        "overzicht van het huidige verkeer - totaal aantal, "
        "onderverdeling naar vliegtuigtype, plus het dichtstbijzijnde, "
        "hoogste, laagste en snelste vliegtuig\n"
        "- Nieuw: nieuw \"Verbindingsstatus\"-scherm toont het "
        "aantal gevolgde vliegtuigen, tijd sinds de laatste "
        "geslaagde gegevensophaling, het resultaat van de laatste "
        "poging en een eenvoudige stoplichtstatus";

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
