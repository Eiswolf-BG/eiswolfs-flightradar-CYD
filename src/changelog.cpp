#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: You can now manually pick a fallback ADS-B data source "
        "under System > Tools > Connection status - airplanes.live and "
        "adsb.fi are available in addition to adsb.lol, in case one of "
        "the sources has trouble\n"
        "- Fix: The retry delay after repeated 429 (rate limit) "
        "responses from the ADS-B API grew without limit and was no "
        "longer reset after a successful response - the device could "
        "then go days without fetching new data, even though the "
        "server had long since returned to normal. Now capped and "
        "guaranteed to reset after a success";

    const char* const CHANGELOG_DE =
        "- Neu: Unter System > Werkzeuge > Verbindungsstatus lässt sich "
        "jetzt manuell eine alternative ADS-B-Datenquelle wählen - "
        "airplanes.live und adsb.fi stehen zusätzlich zu adsb.lol zur "
        "Verfügung, falls eine der Quellen mal Probleme macht\n"
        "- Fix: Die Wartezeit nach wiederholten 429-Antworten "
        "(Rate-Limit) der ADS-B-API wuchs unbegrenzt weiter und wurde "
        "nach einer erfolgreichen Antwort nicht mehr zurückgesetzt - "
        "das Gerät konnte dadurch tagelang keine neuen Daten mehr "
        "abrufen, obwohl der Server längst wieder normal antwortete. "
        "Jetzt mit fester Obergrenze und garantiertem Reset bei Erfolg";

    const char* const CHANGELOG_FR =
        "- Nouveau : il est désormais possible de choisir manuellement "
        "une source de données ADS-B alternative sous Système > Outils "
        "> État de la connexion - airplanes.live et adsb.fi sont "
        "disponibles en plus d'adsb.lol, au cas où l'une des sources "
        "poserait problème\n"
        "- Correction : le délai d'attente après des réponses 429 "
        "(limite de débit) répétées de l'API ADS-B augmentait sans "
        "limite et n'était plus réinitialisé après une réponse "
        "réussie - l'appareil pouvait ainsi rester plusieurs jours "
        "sans récupérer de nouvelles données, alors que le serveur "
        "répondait de nouveau normalement depuis longtemps. Désormais "
        "plafonné, avec réinitialisation garantie après un succès";

    const char* const CHANGELOG_TR =
        "- Yeni: Sistem > Araçlar > Bağlantı durumu altında artık "
        "manuel olarak alternatif bir ADS-B veri kaynağı seçilebiliyor "
        "- adsb.lol'a ek olarak airplanes.live ve adsb.fi de "
        "kullanılabilir, kaynaklardan biri sorun çıkarırsa diye\n"
        "- Düzeltme: ADS-B API'sinden art arda gelen 429 (hız sınırı) "
        "yanıtlarından sonraki bekleme süresi sınırsız büyüyordu ve "
        "başarılı bir yanıttan sonra artık sıfırlanmıyordu - bu "
        "nedenle cihaz, sunucu çoktan normale dönmüş olsa bile "
        "günlerce yeni veri alamıyordu. Artık sabit bir üst sınırı "
        "var ve başarıdan sonra kesin olarak sıfırlanıyor";

    const char* const CHANGELOG_ES =
        "- Novedad: ahora se puede elegir manualmente una fuente de "
        "datos ADS-B alternativa en Sistema > Herramientas > Estado "
        "de la conexión - airplanes.live y adsb.fi están disponibles "
        "además de adsb.lol, por si alguna de las fuentes da "
        "problemas\n"
        "- Corrección: el tiempo de espera tras respuestas 429 "
        "(límite de solicitudes) repetidas de la API ADS-B crecía sin "
        "límite y ya no se restablecía tras una respuesta exitosa - "
        "el dispositivo podía así pasar días sin obtener datos "
        "nuevos, aunque el servidor llevara tiempo respondiendo con "
        "normalidad. Ahora tiene un límite máximo fijo y se "
        "restablece de forma garantizada tras un éxito";

    const char* const CHANGELOG_IT =
        "- Novità: ora è possibile scegliere manualmente una fonte "
        "dati ADS-B alternativa in Sistema > Strumenti > Stato della "
        "connessione - airplanes.live e adsb.fi sono disponibili "
        "oltre ad adsb.lol, nel caso una delle fonti dia problemi\n"
        "- Correzione: il tempo di attesa dopo ripetute risposte 429 "
        "(limite di frequenza) dell'API ADS-B cresceva senza limiti e "
        "non veniva più azzerato dopo una risposta riuscita - il "
        "dispositivo poteva così restare giorni senza recuperare "
        "nuovi dati, anche se il server rispondeva normalmente da "
        "tempo. Ora ha un limite massimo fisso e si azzera garantito "
        "dopo un successo";

    const char* const CHANGELOG_PT =
        "- Novo: agora é possível escolher manualmente uma fonte de "
        "dados ADS-B alternativa em Sistema > Ferramentas > Estado da "
        "conexão - airplanes.live e adsb.fi estão disponíveis além do "
        "adsb.lol, caso uma das fontes apresente problemas\n"
        "- Correção: o tempo de espera após respostas 429 (limite de "
        "taxa) repetidas da API ADS-B crescia sem limite e não era "
        "mais redefinido após uma resposta bem-sucedida - o "
        "dispositivo podia assim ficar dias sem buscar novos dados, "
        "mesmo que o servidor já estivesse respondendo normalmente há "
        "muito tempo. Agora com limite máximo fixo e reinício "
        "garantido após um sucesso";

    const char* const CHANGELOG_NL =
        "- Nieuw: onder Systeem > Hulpmiddelen > Verbindingsstatus "
        "kan nu handmatig een alternatieve ADS-B-gegevensbron gekozen "
        "worden - naast adsb.lol zijn nu ook airplanes.live en "
        "adsb.fi beschikbaar, voor het geval een van de bronnen "
        "problemen geeft\n"
        "- Fix: de wachttijd na herhaalde 429-reacties (rate limit) "
        "van de ADS-B-API bleef onbeperkt groeien en werd na een "
        "geslaagde reactie niet meer gereset - het apparaat kon "
        "hierdoor dagenlang geen nieuwe gegevens meer ophalen, "
        "terwijl de server allang weer normaal reageerde. Nu met een "
        "vaste bovengrens en gegarandeerde reset na succes";

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
