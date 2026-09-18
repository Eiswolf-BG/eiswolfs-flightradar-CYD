#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: Follow-Me Mode - the radar now centers live on your "
        "GPS position, rotates with your direction of travel, and "
        "automatically zooms out at higher speed (Menu -> System -> "
        "Display -> Radar Display)\n"
        "- New: Performance Auto-Tuning - automatically detects "
        "sluggishness or tight memory and gradually reduces render "
        "load (weather effects, sweep animation, aircraft silhouette "
        "detail), stepping back down once things recover; current "
        "level shown on the System Status screen";

    const char* const CHANGELOG_DE =
        "- Neu: Follow-Me Modus - das Radar zentriert sich jetzt live "
        "auf die GPS-Position, dreht sich mit der Fahrtrichtung und "
        "zoomt bei hoeherer Geschwindigkeit automatisch weiter heraus "
        "(Menue -> System -> Anzeige -> Radar-Darstellung)\n"
        "- Neu: Performance-Auto-Tuning - erkennt automatisch "
        "Traegheit oder knappen Speicher und reduziert schrittweise "
        "die Render-Last (Wetter-Effekte, Sweep-Animation, Detailgrad "
        "der Flugzeug-Silhouetten), stuft automatisch wieder zurueck, "
        "sobald sich die Werte erholen; aktuelle Stufe im System-"
        "Status-Screen sichtbar";

    const char* const CHANGELOG_FR =
        "- Nouveau : Mode Suivi - le radar se centre désormais en "
        "direct sur votre position GPS, s'oriente selon votre "
        "direction de déplacement et effectue un zoom arrière "
        "automatique à vitesse plus élevée (Menu -> Système -> "
        "Affichage -> Affichage Radar)\n"
        "- Nouveau : Réglage Auto Performance - détecte "
        "automatiquement les ralentissements ou la mémoire "
        "insuffisante et réduit progressivement la charge de rendu "
        "(effets météo, animation du balayage, détail des silhouettes "
        "d'avions), revenant en arrière dès que la situation "
        "s'améliore ; le niveau actuel est visible sur l'écran d'état "
        "du système";

    const char* const CHANGELOG_TR =
        "- Yeni: Takip Modu - radar artık GPS konumunuza canlı olarak "
        "ortalanıyor, hareket yönünüze göre dönüyor ve daha yüksek "
        "hızlarda otomatik olarak uzaklaşıyor (Menü -> Sistem -> "
        "Ekran -> Radar Görünümü)\n"
        "- Yeni: Performans Otomatik Ayarı - yavaşlamayı veya bellek "
        "darlığını otomatik olarak tespit eder ve render yükünü "
        "(hava efektleri, tarama animasyonu, uçak silüeti detayı) "
        "kademeli olarak azaltır, değerler iyileştiğinde geri adım "
        "atar; mevcut seviye Sistem Durumu ekranında görünür";

    const char* const CHANGELOG_ES =
        "- Novedad: Modo Seguimiento - el radar ahora se centra en "
        "directo en tu posición GPS, gira según tu dirección de "
        "desplazamiento y aleja el zoom automáticamente a mayor "
        "velocidad (Menú -> Sistema -> Pantalla -> Visualización del "
        "Radar)\n"
        "- Novedad: Ajuste Automático de Rendimiento - detecta "
        "automáticamente lentitud o memoria escasa y reduce "
        "progresivamente la carga de renderizado (efectos "
        "meteorológicos, animación de barrido, detalle de las "
        "siluetas de aeronaves), retrocediendo en cuanto los valores "
        "se recuperan; el nivel actual se muestra en la pantalla de "
        "estado del sistema";

    const char* const CHANGELOG_IT =
        "- Novità: Modalità Follow-Me - il radar ora si centra in "
        "tempo reale sulla tua posizione GPS, ruota in base alla "
        "direzione di marcia e riduce automaticamente lo zoom alle "
        "velocità più elevate (Menu -> Sistema -> Schermo -> "
        "Visualizzazione Radar)\n"
        "- Novità: Ottimizzazione Automatica Prestazioni - rileva "
        "automaticamente rallentamenti o memoria scarsa e riduce "
        "gradualmente il carico di rendering (effetti meteo, "
        "animazione della scansione, dettaglio delle sagome degli "
        "aerei), tornando indietro non appena i valori migliorano; "
        "il livello attuale è visibile nella schermata di stato del "
        "sistema";

    const char* const CHANGELOG_PT =
        "- Novo: Modo Follow-Me - o radar agora se centraliza ao vivo "
        "na sua posição GPS, gira de acordo com a direção de "
        "deslocamento e reduz o zoom automaticamente em velocidades "
        "mais altas (Menu -> Sistema -> Tela -> Exibição do Radar)\n"
        "- Novo: Ajuste Automático de Desempenho - detecta "
        "automaticamente lentidão ou memória escassa e reduz "
        "gradualmente a carga de renderização (efeitos climáticos, "
        "animação de varredura, detalhe das silhuetas das aeronaves), "
        "retrocedendo assim que os valores se recuperam; o nível "
        "atual é exibido na tela de status do sistema";

    const char* const CHANGELOG_NL =
        "- Nieuw: Follow-Me Modus - de radar centreert zich nu live "
        "op je GPS-positie, draait mee met je rijrichting en zoomt "
        "bij hogere snelheid automatisch verder uit (Menu -> Systeem "
        "-> Scherm -> Radarweergave)\n"
        "- Nieuw: Prestatie Auto-Tuning - detecteert automatisch "
        "traagheid of krap geheugen en vermindert geleidelijk de "
        "renderbelasting (weer-effecten, sweep-animatie, detailniveau "
        "van vliegtuigsilhouetten), schakelt automatisch weer terug "
        "zodra de waarden zich herstellen; huidig niveau zichtbaar op "
        "het systeemstatusscherm";

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
