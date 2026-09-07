#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the bearing line in the selected aircraft's detail panel "
        "now also shows the elevation angle (e.g. \"327° NW · 21° up\"), "
        "calculated from distance and altitude\n"
        "- New: the detail panel's altitude/time pattern line from earlier "
        "sightings now also shows the shortest distance and highest speed "
        "ever recorded for that aircraft - appears once at least one "
        "logbook entry in the new format exists for it, so not "
        "immediately on the next sighting after this update, but the one "
        "after that\n"
        "- Fix: the weather info screen (tap the weather icon) always "
        "showed the airport code as ICAO for the METAR report, regardless "
        "of the Menu > Region & Language > Units (IATA/ICAO) setting - "
        "now correctly shows IATA when set and available, otherwise ICAO, "
        "matching the rest of the app";

    const char* const CHANGELOG_DE =
        "- Neu: Die Peil-Zeile im Detail-Panel des ausgewählten Flugzeugs "
        "zeigt jetzt zusätzlich den Höhenwinkel an (z.B. \"327° NW · 21° "
        "hoch\"), berechnet aus Entfernung und Flughöhe\n"
        "- Neu: Die Zeile mit dem Höhen-/Uhrzeitmuster aus früheren "
        "Sichtungen im Detail-Panel zeigt jetzt zusätzlich die kürzeste je "
        "gemessene Distanz und die höchste je gemessene Geschwindigkeit "
        "dieses Flugzeugs - erscheint erst, sobald mindestens ein "
        "Logbuch-Eintrag im neuen Format für dieses Flugzeug vorliegt, "
        "also nicht sofort beim nächsten Wiedersehen nach diesem Update, "
        "sondern erst beim Mal danach\n"
        "- Fix: Der Wetter-Info-Screen (Antippen des Wetter-Icons) zeigte "
        "den Flughafen-Code beim Flugwetterbericht (METAR) bisher immer "
        "als ICAO, unabhängig von der Einstellung Menü > Land/Region > "
        "Einheiten (IATA/ICAO) - zeigt jetzt korrekt IATA an, wenn "
        "eingestellt und verfügbar, sonst ICAO, wie an den anderen "
        "Stellen im Projekt auch";

    const char* const CHANGELOG_FR =
        "- Nouveau : la ligne de relèvement dans le panneau de détails de "
        "l'avion sélectionné affiche désormais aussi l'angle d'élévation "
        "(par ex. « 327° NW · 21° haut »), calculé à partir de la "
        "distance et de l'altitude\n"
        "- Nouveau : la ligne du panneau de détails indiquant le schéma "
        "d'altitude/horaire des observations précédentes affiche "
        "désormais aussi la distance la plus courte et la vitesse la "
        "plus élevée jamais enregistrées pour cet avion - n'apparaît "
        "qu'une fois qu'au moins une entrée de carnet de vol au nouveau "
        "format existe pour lui, donc pas immédiatement lors de la "
        "prochaine observation après cette mise à jour, mais à la "
        "suivante\n"
        "- Correction : l'écran d'informations météo (en appuyant sur "
        "l'icône météo) affichait toujours le code aéroport en ICAO pour "
        "le bulletin météo aéronautique (METAR), indépendamment du "
        "réglage Menu > Région et langue > Unités (IATA/ICAO) - affiche "
        "désormais correctement l'IATA si réglé et disponible, sinon "
        "l'ICAO, comme partout ailleurs dans l'application";

    const char* const CHANGELOG_TR =
        "- Yeni: Seçili uçağın detay panelindeki yön çizgisi artık "
        "irtifa açısını da gösteriyor (örn. \"327° NW · 21° yukarı\"), "
        "mesafe ve irtifadan hesaplanıyor\n"
        "- Yeni: Detay panelindeki önceki gözlemlerden gelen irtifa/saat "
        "deseni satırı artık bu uçak için şimdiye kadar ölçülen en kısa "
        "mesafeyi ve en yüksek hızı da gösteriyor - bu satır, uçak için "
        "yeni formatta en az bir uçuş defteri kaydı oluşana kadar "
        "görünmüyor, yani bu güncellemeden sonraki bir sonraki görüşte "
        "değil, ondan sonraki görüşte ortaya çıkıyor\n"
        "- Düzeltme: Hava durumu bilgi ekranı (hava durumu simgesine "
        "dokunarak açılır) METAR raporundaki havalimanı kodunu, Menü > "
        "Bölge ve Dil > Birimler (IATA/ICAO) ayarından bağımsız olarak "
        "her zaman ICAO olarak gösteriyordu - artık ayarlanmışsa ve "
        "mevcutsa doğru şekilde IATA, aksi halde ICAO gösteriyor, "
        "uygulamanın diğer yerleriyle tutarlı olarak";

    const char* const CHANGELOG_ES =
        "- Novedad: la línea de rumbo en el panel de detalles del avión "
        "seleccionado ahora también muestra el ángulo de elevación (por "
        "ej. «327° NW · 21° arriba»), calculado a partir de la distancia "
        "y la altitud\n"
        "- Novedad: la línea del panel de detalles con el patrón de "
        "altitud/hora de avistamientos anteriores ahora también muestra "
        "la distancia más corta y la velocidad más alta jamás registradas "
        "para ese avión - aparece solo cuando existe al menos un registro "
        "de bitácora en el nuevo formato para él, por lo que no aparece "
        "de inmediato en el próximo avistamiento tras esta actualización, "
        "sino en el siguiente\n"
        "- Corrección: la pantalla de información meteorológica (al tocar "
        "el icono del tiempo) mostraba siempre el código de aeropuerto en "
        "ICAO para el informe METAR, independientemente del ajuste Menú > "
        "Región e idioma > Unidades (IATA/ICAO) - ahora muestra "
        "correctamente IATA cuando está configurado y disponible, y ICAO "
        "en caso contrario, igual que en el resto de la aplicación";

    const char* const CHANGELOG_IT =
        "- Novità: la riga del rilevamento nel pannello dei dettagli "
        "dell'aereo selezionato ora mostra anche l'angolo di elevazione "
        "(es. \"327° NW · 21° su\"), calcolato da distanza e altitudine\n"
        "- Novità: la riga del pannello dei dettagli con lo schema di "
        "altitudine/orario degli avvistamenti precedenti ora mostra anche "
        "la distanza più breve e la velocità più alta mai registrate per "
        "quell'aereo - compare solo quando esiste almeno una voce di "
        "diario di volo nel nuovo formato per esso, quindi non subito al "
        "prossimo avvistamento dopo questo aggiornamento, ma a quello "
        "successivo\n"
        "- Correzione: la schermata informazioni meteo (toccando l'icona "
        "meteo) mostrava sempre il codice aeroporto in ICAO per il "
        "bollettino METAR, indipendentemente dall'impostazione Menu > "
        "Regione e lingua > Unità (IATA/ICAO) - ora mostra correttamente "
        "lo IATA se impostato e disponibile, altrimenti l'ICAO, come nel "
        "resto dell'app";

    const char* const CHANGELOG_PT =
        "- Novo: a linha de marcação no painel de detalhes da aeronave "
        "selecionada agora também mostra o ângulo de elevação (ex. "
        "\"327° NW · 21° acima\"), calculado a partir da distância e da "
        "altitude\n"
        "- Novo: a linha do painel de detalhes com o padrão de "
        "altitude/horário de avistamentos anteriores agora também mostra "
        "a menor distância e a maior velocidade já registradas para essa "
        "aeronave - só aparece quando existe pelo menos um registro de "
        "diário de bordo no novo formato para ela, portanto não "
        "imediatamente no próximo avistamento após esta atualização, mas "
        "no seguinte\n"
        "- Correção: a tela de informações meteorológicas (ao tocar no "
        "ícone do tempo) sempre mostrava o código do aeroporto em ICAO "
        "para o relatório METAR, independentemente da configuração Menu "
        "> Região e idioma > Unidades (IATA/ICAO) - agora mostra "
        "corretamente IATA quando configurado e disponível, senão ICAO, "
        "igual ao resto do app";

    const char* const CHANGELOG_NL =
        "- Nieuw: de peillijn in het detailpaneel van het geselecteerde "
        "vliegtuig toont nu ook de hoogtehoek (bijv. \"327° NW · 21° "
        "omhoog\"), berekend uit afstand en vlieghoogte\n"
        "- Nieuw: de regel met het hoogte-/tijdpatroon van eerdere "
        "waarnemingen in het detailpaneel toont nu ook de kortste ooit "
        "gemeten afstand en de hoogste ooit gemeten snelheid van dat "
        "vliegtuig - verschijnt pas zodra er minstens één logboek-"
        "vermelding in het nieuwe formaat voor dat vliegtuig bestaat, dus "
        "niet meteen bij de eerstvolgende waarneming na deze update, maar "
        "pas de keer daarna\n"
        "- Fix: het weerinformatiescherm (tik op het weericoon) toonde de "
        "luchthavencode bij het METAR-weerbericht altijd als ICAO, "
        "ongeacht de instelling Menu > Regio en taal > Eenheden "
        "(IATA/ICAO) - toont nu correct IATA wanneer ingesteld en "
        "beschikbaar, anders ICAO, net als op de andere plekken in de app";

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
