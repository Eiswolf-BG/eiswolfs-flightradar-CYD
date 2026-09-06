#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- New: MQTT/Home Assistant integration extended with eight "
        "more sensors - nearest/highest/lowest/fastest aircraft, "
        "helicopter/heavy aircraft count, and military/emergency "
        "detection\n"
        "- New: the web interface can now be installed on your "
        "phone as its own app, with its own icon and a full-screen "
        "mode";

    const char* const CHANGELOG_DE =
        "- Neu: MQTT/Home-Assistant-Anbindung um acht weitere "
        "Sensoren erweitert - nächstes/höchstes/niedrigstes/"
        "schnellstes Flugzeug, Anzahl Hubschrauber/Heavy-Flugzeuge, "
        "sowie Militär- und Notfall-Erkennung\n"
        "- Neu: die Web-Oberfläche lässt sich jetzt auf dem "
        "Smartphone als eigene App installieren, mit eigenem Icon "
        "und Vollbildmodus";

    const char* const CHANGELOG_FR =
        "- Nouveau : intégration MQTT/Home Assistant étendue de "
        "huit capteurs supplémentaires - avion le plus proche/haut/"
        "bas/rapide, nombre d'hélicoptères/d'avions Heavy, ainsi que "
        "la détection militaire/d'urgence\n"
        "- Nouveau : l'interface web peut désormais être installée "
        "sur le téléphone comme une application à part entière, avec "
        "sa propre icône et un mode plein écran";

    const char* const CHANGELOG_TR =
        "- Yeni: MQTT/Home Assistant entegrasyonu sekiz yeni "
        "sensörle genişletildi - en yakın/en yüksek/en alçak/en "
        "hızlı uçak, helikopter/Heavy uçak sayısı ve askeri/acil "
        "durum tespiti\n"
        "- Yeni: web arayüzü artık telefona kendi simgesi ve tam "
        "ekran moduyla ayrı bir uygulama olarak yüklenebiliyor";

    const char* const CHANGELOG_ES =
        "- Novedad: integración MQTT/Home Assistant ampliada con "
        "ocho sensores más - avión más cercano/alto/bajo/rápido, "
        "número de helicópteros/aviones Heavy, y detección militar/"
        "de emergencia\n"
        "- Novedad: la interfaz web ahora se puede instalar en el "
        "teléfono como una app propia, con su propio icono y modo "
        "de pantalla completa";

    const char* const CHANGELOG_IT =
        "- Novità: integrazione MQTT/Home Assistant estesa con otto "
        "sensori aggiuntivi - aereo più vicino/alto/basso/veloce, "
        "numero di elicotteri/aerei Heavy, e rilevamento militare/"
        "di emergenza\n"
        "- Novità: l'interfaccia web ora può essere installata sul "
        "telefono come app a sé stante, con icona propria e modalità "
        "a schermo intero";

    const char* const CHANGELOG_PT =
        "- Novo: integração MQTT/Home Assistant ampliada com oito "
        "novos sensores - aeronave mais próxima/alta/baixa/rápida, "
        "número de helicópteros/aeronaves Heavy, e detecção militar/"
        "de emergência\n"
        "- Novo: a interface web agora pode ser instalada no celular "
        "como um app próprio, com ícone próprio e modo de tela cheia";

    const char* const CHANGELOG_NL =
        "- Nieuw: MQTT/Home Assistant-integratie uitgebreid met acht "
        "extra sensoren - dichtstbijzijnde/hoogste/laagste/snelste "
        "vliegtuig, aantal helikopters/Heavy-vliegtuigen, en "
        "militaire/nood-detectie\n"
        "- Nieuw: de webinterface kan nu op de telefoon als eigen "
        "app worden geïnstalleerd, met eigen icoon en volledig-"
        "scherm-modus";

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
