#pragma once
#include <Arduino.h>

// Optionale MQTT-Anbindung (SettingsStore::mqttEnabled(), AUS per Default,
// siehe mqtt_screen.cpp) - fuer Nutzer, die ein paar Radar-Kennzahlen in
// ein eigenes Smart-Home-System (z.B. Home Assistant) einspeisen wollen.
// Laeuft komplett auf Core 0 (NetTask, siehe net_task.cpp) - gleiche
// Core-Zuordnung wie alle anderen Netzwerk-Zugriffe im Projekt (WLAN, ADS-B-
// Abruf, OTA-Update-Pruefung), damit Core 1 (Display/Touch) nie durch einen
// haengenden Broker blockiert wird.
//
// Home-Assistant-MQTT-Discovery (offizielles Format, siehe
// https://www.home-assistant.io/integrations/mqtt/#discovery-topic):
// bei jeder erfolgreichen (Wieder-)Verbindung werden automatisch fuenf
// retained Discovery-Konfigurationsnachrichten unter
// "homeassistant/<component>/<node_id>/<object_id>/config" veroeffentlicht
// (Anzahl Flugzeuge, Naeherungs-/Watchlist-Alarm, WLAN-Signalstaerke,
// Firmware-Version), alle unter einem gemeinsamen "device"-Objekt
// gruppiert. Verbindungsstatus laeuft ueber ein MQTT Last-Will-and-
// Testament (siehe tryConnect() in mqtt_client.cpp) - der Broker meldet
// das Geraet automatisch als "offline", sobald die Verbindung abbricht,
// ohne dass das Geraet selbst noch etwas senden muesste.
namespace MqttClient {

    // Einmalig aus net_task.cpp::begin()/taskFunc() aufrufen (aktuell ohne
    // Wirkung ausser einer Log-Zeile - die eigentliche Verbindung wird
    // faul/bei Bedarf in loop() aufgebaut, siehe dort).
    void init();

    // Bei JEDEM Durchlauf der NetTask-Hauptschleife aufrufen (alle ~50ms,
    // siehe net_task.cpp) - kuemmert sich um (Wieder-)Verbinden (mit
    // Mindestabstand zwischen Versuchen, siehe mqtt_client.cpp) und ruft
    // PubSubClient::loop() auf (noetig fuer Keep-Alive-Pings, auch wenn wir
    // selbst nichts abonnieren). Liest SettingsStore::mqttEnabled() selbst
    // bei jedem Aufruf - bei AUS passiert nichts (keine Verbindung, kein
    // Netzwerk-Traffic).
    void loop();

    // Sendet die aktuellen Kennzahlen als retained MQTT-Nachrichten (siehe
    // mqtt_client.cpp fuer die genauen Topic-Namen) - aufgerufen aus
    // net_task.cpp nach jedem erfolgreichen ADS-B-Datenabruf. Ohne Wirkung,
    // wenn MQTT ausgeschaltet oder gerade nicht verbunden ist.
    void publishStatus(uint8_t aircraftCount, bool watchlistAlert, bool proximityAlert);

}
