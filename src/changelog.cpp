#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: the radar display's world map background can now be "
        "switched on/off (Radar Display menu), on by default as "
        "before\n"
        "- New: the airline filter can now also work as a whitelist "
        "(\"Show only\") in addition to the existing \"Hide\" mode, both "
        "sharing the same airline list - the mode switch (with "
        "explanation) is available on the device and on the web Lists "
        "page, kept in sync both ways\n"
        "- New: adding an airline to the filter now accepts both IATA "
        "and ICAO codes, converted to ICAO internally";

    const char* const CHANGELOG_DE =
        "- Neu: Der Weltkarten-Hintergrund im Radar-Darstellung-Screen "
        "laesst sich jetzt ein-/ausschalten (Default: an wie bisher)\n"
        "- Neu: Der Airline-Filter kann jetzt zusaetzlich als \"Nur "
        "anzeigen\" (Whitelist) genutzt werden, beide Modi teilen sich "
        "dieselbe Liste - der Umschalter samt Erklaerung ist am Geraet "
        "UND auf der Web-Listenseite verfuegbar, beidseitig synchron\n"
        "- Neu: Beim Hinzufuegen einer Airline zum Filter koennen jetzt "
        "sowohl IATA- als auch ICAO-Codes eingegeben werden (intern "
        "automatisch auf ICAO umgewandelt)";

    const char* const CHANGELOG_FR =
        "- Nouveau : le fond de carte du monde sur l'écran radar peut "
        "désormais être activé/désactivé (activé par défaut, comme "
        "avant)\n"
        "- Nouveau : le filtre compagnies peut désormais aussi "
        "fonctionner comme liste blanche (« Afficher seulement ») en "
        "plus du mode « Masquer » existant, les deux modes partageant "
        "la même liste - le sélecteur (avec explication) est "
        "disponible sur l'appareil ET sur la page web Listes, "
        "synchronisé dans les deux sens\n"
        "- Nouveau : l'ajout d'une compagnie au filtre accepte "
        "désormais les codes IATA et OACI, convertis en OACI en "
        "interne";

    const char* const CHANGELOG_TR =
        "- Yeni: Radar ekranındaki dünya haritası arka planı artık "
        "açılıp kapatılabiliyor (varsayılan: önceki gibi açık)\n"
        "- Yeni: Havayolu filtresi artık mevcut \"Gizle\" moduna ek "
        "olarak beyaz liste olarak da (\"Sadece göster\") "
        "kullanılabiliyor, her iki mod aynı listeyi paylaşıyor - mod "
        "anahtarı (açıklamasıyla birlikte) hem cihazda hem de web "
        "Listeler sayfasında mevcut ve her iki yönde senkronize\n"
        "- Yeni: Filtreye havayolu eklerken artık hem IATA hem de ICAO "
        "kodları girilebiliyor, dahili olarak otomatik ICAO'ya "
        "dönüştürülüyor";

    const char* const CHANGELOG_ES =
        "- Novedad: el fondo de mapa mundial de la pantalla del radar "
        "ahora se puede activar/desactivar (activado por defecto, "
        "como antes)\n"
        "- Novedad: el filtro de aerolíneas ahora también puede "
        "funcionar como lista blanca (\"Mostrar solo\") además del "
        "modo \"Ocultar\" existente, ambos modos comparten la misma "
        "lista - el selector de modo (con explicación) está "
        "disponible en el dispositivo Y en la página web de listas, "
        "sincronizado en ambos sentidos\n"
        "- Novedad: al añadir una aerolínea al filtro ahora se aceptan "
        "tanto códigos IATA como OACI, convertidos internamente a "
        "OACI";

    const char* const CHANGELOG_IT =
        "- Novità: lo sfondo a mappa del mondo nella schermata radar "
        "ora può essere attivato/disattivato (attivo per impostazione "
        "predefinita, come prima)\n"
        "- Novità: il filtro compagnie ora può funzionare anche come "
        "lista bianca (\"Mostra solo\") oltre alla modalità \"Nascondi\" "
        "esistente, entrambe le modalità condividono lo stesso elenco "
        "- il selettore di modalità (con spiegazione) è disponibile "
        "sul dispositivo E sulla pagina web Elenchi, sincronizzato in "
        "entrambe le direzioni\n"
        "- Novità: aggiungendo una compagnia al filtro ora si "
        "accettano sia codici IATA che ICAO, convertiti internamente "
        "in ICAO";

    const char* const CHANGELOG_PT =
        "- Novo: o fundo de mapa-múndi na tela do radar agora pode "
        "ser ativado/desativado (ativado por padrão, como antes)\n"
        "- Novo: o filtro de companhias aéreas agora também pode "
        "funcionar como lista branca (\"Mostrar apenas\") além do modo "
        "\"Ocultar\" existente, ambos os modos compartilham a mesma "
        "lista - o alternador de modo (com explicação) está "
        "disponível no dispositivo E na página web de Listas, "
        "sincronizado nos dois sentidos\n"
        "- Novo: ao adicionar uma companhia aérea ao filtro agora são "
        "aceitos códigos IATA e ICAO, convertidos internamente para "
        "ICAO";

    const char* const CHANGELOG_NL =
        "- Nieuw: de wereldkaartachtergrond op het radarscherm kan nu "
        "in-/uitgeschakeld worden (standaard aan, zoals voorheen)\n"
        "- Nieuw: de luchtvaartmaatschappijenfilter kan nu ook als "
        "whitelist (\"Alleen tonen\") werken naast de bestaande "
        "\"Verbergen\"-modus, beide modi delen dezelfde lijst - de "
        "modusschakelaar (met uitleg) is beschikbaar op het apparaat "
        "EN op de webpagina Lijsten, in beide richtingen "
        "gesynchroniseerd\n"
        "- Nieuw: bij het toevoegen van een maatschappij aan de "
        "filter worden nu zowel IATA- als ICAO-codes geaccepteerd, "
        "intern automatisch omgezet naar ICAO";

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
