#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 6;

    const char* const CHANGELOG_EN =
        "- New: choose IATA or ICAO airport codes for the route display "
        "in Menu > Location/Region > Units\n"
        "- Fix: switched the ADS-B data source from adsb.fi to adsb.lol "
        "- adsb.fi had started silently answering the device's requests "
        "with empty results without any error";

    const char* const CHANGELOG_DE =
        "- Neu: Flughafencodes für die Routenanzeige wahlweise als IATA "
        "oder ICAO im Menü > Land/Region > Einheiten\n"
        "- Fix: ADS-B-Datenquelle von adsb.fi auf adsb.lol umgestellt - "
        "adsb.fi hatte begonnen, Anfragen vom Gerät serverseitig ohne "
        "Fehlermeldung leer zu beantworten";

    const char* const CHANGELOG_FR =
        "- Nouveau : choisir les codes aéroport IATA ou OACI pour "
        "l'affichage de la route dans Menu > Lieu/Région > Unités\n"
        "- Correction : changement de la source de données ADS-B, "
        "d'adsb.fi vers adsb.lol - adsb.fi avait commencé à répondre aux "
        "requêtes de l'appareil avec des résultats vides, sans aucune "
        "erreur";

    const char* const CHANGELOG_TR =
        "- Yeni: Menü > Konum/Bölge > Birimler'de rota gösterimi için "
        "IATA veya ICAO havalimanı kodları seçilebilir\n"
        "- Düzeltme: ADS-B veri kaynağı adsb.fi'den adsb.lol'e "
        "değiştirildi - adsb.fi, cihazın isteklerine herhangi bir hata "
        "vermeden sessizce boş sonuçlar döndürmeye başlamıştı";

    const char* const CHANGELOG_ES =
        "- Novedad: elegir códigos de aeropuerto IATA o ICAO para la "
        "ruta mostrada en Menú > Ubicación/Región > Unidades\n"
        "- Corrección: se cambió la fuente de datos ADS-B de adsb.fi a "
        "adsb.lol - adsb.fi había empezado a responder a las peticiones "
        "del dispositivo con resultados vacíos, sin ningún error";

    const char* const CHANGELOG_IT =
        "- Novità: scegliere i codici aeroportuali IATA o ICAO per la "
        "rotta mostrata in Menu > Posizione/Regione > Unità\n"
        "- Correzione: la fonte dati ADS-B è stata cambiata da adsb.fi "
        "ad adsb.lol - adsb.fi aveva iniziato a rispondere alle "
        "richieste del dispositivo con risultati vuoti, senza alcun "
        "errore";

    const char* const TABLE[CHANGELOG_LANG_COUNT] = {
        CHANGELOG_EN, CHANGELOG_DE, CHANGELOG_FR, CHANGELOG_TR, CHANGELOG_ES, CHANGELOG_IT
    };
}

const char* changelogLatest() {
    uint8_t lang = SettingsStore::language();
    if (lang >= CHANGELOG_LANG_COUNT) lang = 0;
    return TABLE[lang];
}

}
