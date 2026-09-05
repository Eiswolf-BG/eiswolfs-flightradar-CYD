#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: Auto-Range option when cycling through the radar "
        "range - automatically picks between 10/25/50 km depending "
        "on current traffic density, 100 km remains manual-only "
        "for stability reasons\n"
        "- New: overflight ETA estimate in the aircraft detail "
        "panel, based on heading and speed of an approaching "
        "aircraft\n"
        "- Improved: weather data is now fetched every 5 minutes "
        "instead of 10, for a faster reaction of the rain/snow "
        "effect to real weather changes\n"
        "- Fix: an aircraft almost directly overhead could "
        "temporarily become invisible while the CRT-Phosphor effect "
        "was active\n"
        "- Fix: the approaching/departing/passing indicator barely "
        "ever worked due to an internal bug\n"
        "- Fix: the proximity alert (simple and smart) reacted "
        "regardless of the configured radar range and active "
        "display filters\n"
        "- Fix: aircraft no longer aged out internally during an "
        "ADS-B connection loss, which could leave a proximity alert "
        "stuck active";

    const char* const CHANGELOG_DE =
        "- Neu: Auto-Range-Option beim Durchschalten der "
        "Radar-Reichweite - wählt automatisch zwischen 10/25/50km "
        "je nach aktueller Verkehrsdichte, 100km bleibt aus "
        "Stabilitätsgründen nur manuell wählbar\n"
        "- Neu: ETA-Schätzung für einen möglichen Überflug im "
        "Flugzeug-Detail-Panel, basierend auf Kurs und "
        "Geschwindigkeit eines sich annähernden Flugzeugs\n"
        "- Verbessert: Wetterdaten werden jetzt alle 5 statt 10 "
        "Minuten abgerufen, für eine schnellere Reaktion des "
        "Regen-/Schnee-Effekts auf echte Wetteränderungen\n"
        "- Fix: Ein Flugzeug fast genau über dem eigenen Standort "
        "konnte bei aktivem CRT-Phosphor-Effekt vorübergehend "
        "unsichtbar werden\n"
        "- Fix: Die Anzeige, ob sich ein Flugzeug annähert, entfernt "
        "oder vorbeifliegt, funktionierte durch einen internen "
        "Fehler fast nie\n"
        "- Fix: Der Näherungsalarm (einfach und intelligent) "
        "reagierte unabhängig von der eingestellten Radar-Reichweite "
        "und aktiven Anzeigefiltern\n"
        "- Fix: Bei Verbindungsverlust zum ADS-B-Server alterten "
        "Flugzeuge intern nicht mehr, wodurch ein Näherungsalarm "
        "dauerhaft aktiv bleiben konnte";

    const char* const CHANGELOG_FR =
        "- Nouveau : option Auto-Range lors du changement de "
        "portée du radar - choisit automatiquement entre 10/25/50 "
        "km selon la densité de trafic actuelle, le palier 100 km "
        "reste accessible uniquement manuellement pour des raisons "
        "de stabilité\n"
        "- Nouveau : estimation de l'heure de survol dans le "
        "panneau de détails de l'avion, basée sur le cap et la "
        "vitesse d'un avion qui s'approche\n"
        "- Amélioré : les données météo sont désormais récupérées "
        "toutes les 5 minutes au lieu de 10, pour une réaction plus "
        "rapide de l'effet pluie/neige aux changements météo réels\n"
        "- Correction : un avion presque exactement au-dessus de "
        "votre position pouvait temporairement devenir invisible "
        "lorsque l'effet CRT-Phosphor était actif\n"
        "- Correction : l'indicateur d'approche/éloignement/passage "
        "ne fonctionnait presque jamais à cause d'un bug interne\n"
        "- Correction : l'alarme de proximité (simple et "
        "intelligente) réagissait indépendamment de la portée radar "
        "configurée et des filtres d'affichage actifs\n"
        "- Correction : les avions ne vieillissaient plus en "
        "interne lors d'une perte de connexion au serveur ADS-B, ce "
        "qui pouvait laisser une alarme de proximité activée en "
        "permanence";

    const char* const CHANGELOG_TR =
        "- Yeni: radar menzilini değiştirirken Otomatik Menzil "
        "seçeneği - mevcut trafik yoğunluğuna göre otomatik olarak "
        "10/25/50 km arasında seçim yapar, 100 km kararlılık "
        "nedeniyle yalnızca manuel olarak seçilebilir kalır\n"
        "- Yeni: yaklaşan bir uçağın rotası ve hızına dayalı "
        "olarak, uçak detay panelinde olası bir üstten geçiş için "
        "tahmini süre\n"
        "- İyileştirildi: hava durumu verileri artık 10 dakika "
        "yerine her 5 dakikada bir alınıyor, böylece yağmur/kar "
        "efekti gerçek hava değişikliklerine daha hızlı tepki "
        "veriyor\n"
        "- Düzeltme: CRT-Fosfor efekti etkinken, neredeyse tam "
        "olarak konumunuzun üzerindeki bir uçak geçici olarak "
        "görünmez hale gelebiliyordu\n"
        "- Düzeltme: bir uçağın yaklaşıp yaklaşmadığını, uzaklaşıp "
        "uzaklaşmadığını veya yanından geçip geçmediğini gösteren "
        "gösterge, dahili bir hata nedeniyle neredeyse hiç "
        "çalışmıyordu\n"
        "- Düzeltme: yakınlık alarmı (basit ve akıllı) ayarlanan "
        "radar menzilinden ve etkin görüntüleme filtrelerinden "
        "bağımsız olarak tepki veriyordu\n"
        "- Düzeltme: ADS-B sunucusuyla bağlantı kaybı sırasında "
        "uçaklar dahili olarak artık eskimiyordu, bu da bir "
        "yakınlık alarmının sürekli etkin kalmasına neden "
        "olabiliyordu";

    const char* const CHANGELOG_ES =
        "- Novedad: opción de Alcance automático al cambiar el "
        "alcance del radar - elige automáticamente entre 10/25/50 "
        "km según la densidad de tráfico actual, el nivel de 100 km "
        "sigue siendo solo manual por motivos de estabilidad\n"
        "- Novedad: estimación de la hora de sobrevuelo en el "
        "panel de detalles del avión, basada en el rumbo y la "
        "velocidad de un avión que se aproxima\n"
        "- Mejorado: los datos meteorológicos ahora se obtienen "
        "cada 5 minutos en lugar de 10, para una reacción más "
        "rápida del efecto de lluvia/nieve a los cambios climáticos "
        "reales\n"
        "- Corrección: un avión casi exactamente sobre tu ubicación "
        "podía volverse temporalmente invisible con el efecto "
        "CRT-Phosphor activo\n"
        "- Corrección: el indicador de acercamiento/alejamiento/"
        "paso casi nunca funcionaba debido a un error interno\n"
        "- Corrección: la alarma de proximidad (simple e "
        "inteligente) reaccionaba independientemente del alcance de "
        "radar configurado y de los filtros de visualización "
        "activos\n"
        "- Corrección: los aviones ya no se descartaban internamente "
        "durante una pérdida de conexión con el servidor ADS-B, lo "
        "que podía dejar una alarma de proximidad activada "
        "permanentemente";

    const char* const CHANGELOG_IT =
        "- Novità: opzione Auto-Range durante il cambio della "
        "portata del radar - sceglie automaticamente tra 10/25/50 "
        "km in base all'attuale densità di traffico, il livello 100 "
        "km resta selezionabile solo manualmente per motivi di "
        "stabilità\n"
        "- Novità: stima dell'orario di sorvolo nel pannello dei "
        "dettagli dell'aereo, basata su rotta e velocità di un "
        "aereo in avvicinamento\n"
        "- Migliorato: i dati meteo vengono ora recuperati ogni 5 "
        "minuti invece di 10, per una reazione più rapida "
        "dell'effetto pioggia/neve ai cambiamenti meteo reali\n"
        "- Correzione: un aereo quasi esattamente sopra la propria "
        "posizione poteva diventare temporaneamente invisibile con "
        "l'effetto CRT-Phosphor attivo\n"
        "- Correzione: l'indicatore di avvicinamento/allontanamento/"
        "transito quasi non funzionava mai a causa di un bug "
        "interno\n"
        "- Correzione: l'allarme di prossimità (semplice e "
        "intelligente) reagiva indipendentemente dalla portata "
        "radar impostata e dai filtri di visualizzazione attivi\n"
        "- Correzione: gli aerei non venivano più fatti invecchiare "
        "internamente durante una perdita di connessione al server "
        "ADS-B, il che poteva lasciare un allarme di prossimità "
        "bloccato attivo";

    const char* const CHANGELOG_PT =
        "- Novo: opção de Alcance automático ao alternar o alcance "
        "do radar - escolhe automaticamente entre 10/25/50 km de "
        "acordo com a densidade de tráfego atual, o nível de 100 km "
        "permanece apenas manual por motivos de estabilidade\n"
        "- Novo: estimativa de horário de sobrevoo no painel de "
        "detalhes da aeronave, com base na direção e na velocidade "
        "de uma aeronave que se aproxima\n"
        "- Melhorias: os dados meteorológicos agora são obtidos a "
        "cada 5 minutos em vez de 10, para uma reação mais rápida "
        "do efeito de chuva/neve a mudanças climáticas reais\n"
        "- Correção: uma aeronave quase exatamente sobre a sua "
        "localização podia ficar temporariamente invisível com o "
        "efeito CRT-Phosphor ativo\n"
        "- Correção: o indicador de aproximação/afastamento/"
        "passagem quase nunca funcionava devido a um erro interno\n"
        "- Correção: o alarme de proximidade (simples e "
        "inteligente) reagia independentemente do alcance de radar "
        "configurado e dos filtros de exibição ativos\n"
        "- Correção: as aeronaves deixaram de \"envelhecer\" "
        "internamente durante uma perda de conexão com o servidor "
        "ADS-B, o que podia deixar um alarme de proximidade preso "
        "ativo";

    const char* const CHANGELOG_NL =
        "- Nieuw: Auto-Range-optie bij het doorschakelen van het "
        "radarbereik - kiest automatisch tussen 10/25/50 km "
        "afhankelijk van de huidige verkeersdichtheid, 100 km "
        "blijft om stabiliteitsredenen alleen handmatig "
        "selecteerbaar\n"
        "- Nieuw: ETA-schatting voor een mogelijke overvlucht in "
        "het vliegtuig-detailpaneel, gebaseerd op koers en snelheid "
        "van een naderend vliegtuig\n"
        "- Verbeterd: weergegevens worden nu elke 5 in plaats van "
        "10 minuten opgehaald, voor een snellere reactie van het "
        "regen-/sneeuweffect op echte weersveranderingen\n"
        "- Fix: een vliegtuig bijna precies boven je eigen locatie "
        "kon tijdelijk onzichtbaar worden terwijl het CRT-Phosphor-"
        "effect actief was\n"
        "- Fix: de indicator of een vliegtuig nadert, zich "
        "verwijdert of voorbijvliegt werkte door een interne fout "
        "bijna nooit\n"
        "- Fix: het nabijheidsalarm (eenvoudig en slim) reageerde "
        "onafhankelijk van het ingestelde radarbereik en actieve "
        "weergavefilters\n"
        "- Fix: vliegtuigen verouderden intern niet meer bij een "
        "verbindingsverlies met de ADS-B-server, waardoor een "
        "nabijheidsalarm permanent actief kon blijven";

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
