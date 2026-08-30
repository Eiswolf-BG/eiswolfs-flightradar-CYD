#include "mqtt_client.h"
#include "settings_store.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace MqttClient {

namespace {
    WiFiClient wifiClient;
    PubSubClient mqtt(wifiClient);

    // Wie zuletzt an PubSubClient::setServer() uebergeben - nur bei
    // tatsaechlicher Aenderung (Broker-Feld im Menue bearbeitet) neu
    // konfigurieren, siehe ensureConfigured().
    String configuredBroker;

    // Mindestabstand zwischen zwei Verbindungsversuchen - verhindert, dass
    // ein unerreichbarer/falscher Broker die NetTask-Schleife mit staendigen
    // (blockierenden) connect()-Versuchen flutet. PubSubClient::connect()
    // blockiert bis zu ein paar Sekunden bei Nichterreichbarkeit - das ist
    // hinnehmbar, wenn es nur alle 15s statt bei jedem 50ms-Durchlauf
    // passiert.
    uint32_t lastConnectAttemptMs = 0;
    constexpr uint32_t RECONNECT_INTERVAL_MS = 15000;

    // Verfuegbarkeits-Topic - dient GLEICHZEITIG als MQTT Last-Will-Topic
    // (siehe tryConnect(): der Broker veroeffentlicht "offline" automatisch,
    // sobald die Verbindung abbricht, OHNE dass das Geraet selbst noch etwas
    // senden muesste) UND als "availability_topic" in jeder Home-Assistant-
    // Discovery-Konfiguration (siehe publishDiscovery()) - beides zeigt auf
    // dieselbe Zeichenkette, damit Home Assistant den Verbindungsstatus
    // korrekt aus derselben Quelle ableitet, die auch das Last-Will bedient.
    const String AVAILABILITY_TOPIC = String(Config::MQTT_TOPIC_PREFIX) + "/status";

    // Stabile, pro Geraet eindeutige ID (Chip-MAC-Suffix, [a-zA-Z0-9_-] -
    // erfuellt die Home-Assistant-Vorgabe fuer node_id-Zeichen) - dient
    // gleichzeitig als MQTT-Client-ID UND als Home-Assistant-"device"-
    // Identifier/node_id-Segment im Discovery-Topic-Pfad (siehe
    // publishDiscovery()), damit alle drei Verwendungen konsistent
    // zueinander bleiben.
    String nodeId() {
        char buf[40];
        snprintf(buf, sizeof(buf), "eiswolfs-flightradar-%06X",
                  (unsigned)(ESP.getEfuseMac() & 0xFFFFFFUL));
        return String(buf);
    }

    // Zerlegt "host:port" (ein einzelnes Eingabefeld im Menue, siehe
    // mqtt_screen.cpp) am LETZTEN Doppelpunkt - nicht am ersten, damit eine
    // IPv6-Adresse (theoretisch moeglich, auch wenn im Menue-Infotext nicht
    // beworben) nicht faelschlich in der Mitte zerschnitten wird. Ohne
    // Doppelpunkt gilt der komplette String als Host, Port faellt auf
    // Config::MQTT_DEFAULT_PORT zurueck.
    void splitHostPort(const String& hostPort, String& outHost, uint16_t& outPort) {
        int colon = hostPort.lastIndexOf(':');
        if (colon < 0) {
            outHost = hostPort;
            outPort = Config::MQTT_DEFAULT_PORT;
            return;
        }
        outHost = hostPort.substring(0, colon);
        long port = hostPort.substring(colon + 1).toInt();
        outPort = (port > 0 && port <= 65535) ? (uint16_t)port : Config::MQTT_DEFAULT_PORT;
    }

    // Konfiguriert PubSubClient neu, falls sich das Broker-Feld seit dem
    // letzten Aufruf geaendert hat (z.B. gerade im Menue bearbeitet) - ohne
    // diesen Vergleich wuerde setServer() bei jedem loop()-Durchlauf neu
    // aufgerufen, was PubSubClient laut eigener Doku dazu bringt, eine
    // bestehende Verbindung zu verwerfen.
    bool ensureConfigured() {
        String hostPort = SettingsStore::mqttBroker();
        if (hostPort.length() == 0) return false;
        if (hostPort == configuredBroker) return true;

        String host;
        uint16_t port;
        splitHostPort(hostPort, host, port);
        if (host.length() == 0) return false;

        mqtt.setServer(host.c_str(), port);
        configuredBroker = hostPort;
        // Ein Broker-/Port-Wechsel macht eine evtl. bestehende Verbindung
        // zum ALTEN Broker ungueltig - PubSubClient haelt intern aber noch
        // den alten Socket offen, bis der naechste connected()-Check
        // fehlschlaegt. Sauberer sofort trennen, statt auf den naechsten
        // fehlgeschlagenen Keep-Alive zu warten.
        if (mqtt.connected()) mqtt.disconnect();
        return true;
    }

    // Baut EINE Home-Assistant-Discovery-Konfiguration als JSON-String -
    // gemeinsames "device"-Objekt (identifiers/name/manufacturer/model/
    // sw_version) und "origin"-Objekt (von Home Assistant seit einiger Zeit
    // empfohlen, siehe offizielle MQTT-Doku) werden fuer JEDE Entitaet
    // separat mitgeschickt, da MQTT Discovery keinen gemeinsamen "Header"
    // ueber mehrere Config-Topics hinweg kennt - jede /config-Nachricht
    // steht fuer sich allein, verlinkt sich aber ueber dieselbe
    // "identifiers"-Kennung zum selben Geraet.
    String buildDiscoveryPayload(const char* name, const char* stateTopic, const char* uniqueIdSuffix,
                                  const char* icon, const char* deviceClass, const char* stateClass,
                                  const char* unit, bool isBinarySensor, const char* entityCategory) {
        JsonDocument doc;
        doc["name"] = name;
        doc["unique_id"] = nodeId() + "_" + uniqueIdSuffix;
        doc["state_topic"] = stateTopic;
        doc["availability_topic"] = AVAILABILITY_TOPIC;
        if (icon) doc["icon"] = icon;
        if (deviceClass) doc["device_class"] = deviceClass;
        if (stateClass) doc["state_class"] = stateClass;
        if (unit) doc["unit_of_measurement"] = unit;
        if (entityCategory) doc["entity_category"] = entityCategory;
        if (isBinarySensor) {
            // Payload-Werte matchen 1:1 die bereits bestehenden Kennzahlen-
            // Topics (siehe publishStatus() unten) - keine Umkodierung
            // noetig.
            doc["payload_on"] = "ON";
            doc["payload_off"] = "OFF";
        }

        JsonObject device = doc["device"].to<JsonObject>();
        JsonArray ids = device["identifiers"].to<JsonArray>();
        ids.add(nodeId());
        device["name"] = "Eiswolfs Flightradar";
        device["manufacturer"] = "Eiswolf-BG";
        device["model"] = "CYD (ESP32-2432S028)";
        device["sw_version"] = Config::APP_VERSION;

        JsonObject origin = doc["origin"].to<JsonObject>();
        origin["name"] = "Eiswolfs Flightradar";
        origin["sw_version"] = Config::APP_VERSION;

        String out;
        serializeJson(doc, out);
        return out;
    }

    // Veroeffentlicht alle fuenf Discovery-Konfigurationen - aufgerufen bei
    // JEDER erfolgreichen (Wieder-)Verbindung (siehe tryConnect()). Retained
    // (letztes Argument true), damit Home Assistant die Entitaeten auch
    // dann sofort wieder findet, wenn es selbst gerade erst neu gestartet
    // wurde und den urspruenglichen Discovery-Zeitpunkt verpasst hat.
    // Topic-Format exakt nach offizieller Home-Assistant-Doku:
    // homeassistant/<component>/<node_id>/<object_id>/config.
    void publishDiscovery() {
        auto topicFor = [&](const char* component, const char* objectId) {
            return String("homeassistant/") + component + "/" + nodeId() + "/" + objectId + "/config";
        };
        String prefix = String(Config::MQTT_TOPIC_PREFIX) + "/";

        mqtt.publish(topicFor("sensor", "aircraft_count").c_str(),
                     buildDiscoveryPayload("Aircraft Count", (prefix + "aircraft-count").c_str(),
                                            "aircraft_count", "mdi:airplane", nullptr, "measurement",
                                            nullptr, false, nullptr).c_str(),
                     true);

        mqtt.publish(topicFor("binary_sensor", "proximity_alert").c_str(),
                     buildDiscoveryPayload("Proximity Alert", (prefix + "proximity-alert").c_str(),
                                            "proximity_alert", "mdi:airplane-alert", nullptr, nullptr,
                                            nullptr, true, nullptr).c_str(),
                     true);

        mqtt.publish(topicFor("binary_sensor", "watchlist_alert").c_str(),
                     buildDiscoveryPayload("Watchlist Alert", (prefix + "watchlist-alert").c_str(),
                                            "watchlist_alert", "mdi:eye-check", nullptr, nullptr,
                                            nullptr, true, nullptr).c_str(),
                     true);

        mqtt.publish(topicFor("sensor", "wifi_rssi").c_str(),
                     buildDiscoveryPayload("WiFi Signal", (prefix + "wifi-rssi").c_str(),
                                            "wifi_rssi", nullptr, "signal_strength", "measurement",
                                            "dBm", false, "diagnostic").c_str(),
                     true);

        mqtt.publish(topicFor("sensor", "firmware_version").c_str(),
                     buildDiscoveryPayload("Firmware Version", (prefix + "firmware-version").c_str(),
                                            "firmware_version", "mdi:chip-outline", nullptr, nullptr,
                                            nullptr, false, "diagnostic").c_str(),
                     true);
    }

    bool tryConnect() {
        if (WiFi.status() != WL_CONNECTED) return false;

        // Eindeutige Client-ID (Chip-ID-Suffix) - wichtig auf oeffentlichen
        // Test-Brokern (z.B. broker.hivemq.com), wo viele fremde Geraete
        // gleichzeitig verbunden sind: zwei Clients mit identischer ID
        // wuerden sich gegenseitig staendig vom Broker werfen.
        String clientId = nodeId();

        String user = SettingsStore::mqttUsername();
        String pass = SettingsStore::mqttPassword();
        // Last-Will-and-Testament (LWT): der Broker veroeffentlicht diese
        // Nachricht SELBSTAENDIG auf AVAILABILITY_TOPIC (retained), sobald
        // er erkennt, dass die Verbindung ungewollt abgebrochen ist (Keep-
        // Alive-Timeout ueberschritten) - genau das macht Home Assistants
        // Verfuegbarkeitsanzeige zuverlaessig, auch bei Stromausfall/
        // Absturz des Geraets, wo es selbst keine "offline"-Nachricht mehr
        // senden koennte. QoS 1 + retain=true, wie fuer LWT/Availability
        // ueblich.
        bool ok = (user.length() > 0)
            ? mqtt.connect(clientId.c_str(), user.c_str(), pass.c_str(),
                           AVAILABILITY_TOPIC.c_str(), 1, true, "offline")
            : mqtt.connect(clientId.c_str(), nullptr, nullptr,
                           AVAILABILITY_TOPIC.c_str(), 1, true, "offline");

        if (ok) {
            Serial.printf("[MQTT] Verbunden mit %s (Client-ID %s)\n",
                          configuredBroker.c_str(), clientId.c_str());
            // "online" MUSS explizit nachgesendet werden - das LWT allein
            // deckt nur den Abbruchfall ab, der positive "ich bin da"-Status
            // wird nie automatisch vom Broker gesetzt.
            mqtt.publish(AVAILABILITY_TOPIC.c_str(), "online", true);
            publishDiscovery();
        } else {
            Serial.printf("[MQTT] Verbindung fehlgeschlagen (state=%d)\n", mqtt.state());
        }
        return ok;
    }
}

void init() {
    // Grosszuegiger als das PubSubClient-Standardlimit (256 Byte) - die
    // Discovery-Konfigurationsnachrichten (verschachteltes "device"- und
    // "origin"-Objekt, siehe buildDiscoveryPayload()) sind je nach Entitaet
    // 400-500 Byte gross und wuerden mit dem Standardpuffer stillschweigend
    // abgeschnitten/verworfen. 768 Byte lassen reichlich Marge, RAM ist auf
    // dem ESP32 hierfuer kein knapper Faktor.
    mqtt.setBufferSize(768);
    Serial.println("[MQTT] Modul bereit (verbindet nur, wenn in den Einstellungen aktiviert).");
}

void loop() {
    if (!SettingsStore::mqttEnabled()) {
        // Sauber trennen, sobald der Schalter waehrend einer bestehenden
        // Verbindung ausgeschaltet wird - "offline" wird hier bewusst noch
        // explizit VOR dem Trennen gesendet: ein regulaeres disconnect()
        // schickt ein sauberes MQTT-DISCONNECT-Paket, wodurch der Broker das
        // Last-Will NICHT ausloest (das greift nur bei einem UNerwarteten
        // Abbruch) - ohne diese Zeile wuerde Home Assistant faelschlich
        // weiterhin "online" anzeigen, obwohl der Nutzer MQTT bewusst
        // ausgeschaltet hat.
        if (mqtt.connected()) {
            mqtt.publish(AVAILABILITY_TOPIC.c_str(), "offline", true);
            mqtt.disconnect();
        }
        return;
    }

    if (!ensureConfigured()) return;

    if (!mqtt.connected()) {
        uint32_t now = millis();
        if (now - lastConnectAttemptMs < RECONNECT_INTERVAL_MS) return;
        lastConnectAttemptMs = now;
        tryConnect();
        return;
    }

    mqtt.loop();
}

void publishStatus(uint8_t aircraftCount, bool watchlistAlert, bool proximityAlert) {
    if (!SettingsStore::mqttEnabled()) return;
    if (!mqtt.connected()) return;

    String prefix = String(Config::MQTT_TOPIC_PREFIX) + "/";

    char countBuf[8];
    snprintf(countBuf, sizeof(countBuf), "%u", (unsigned)aircraftCount);
    mqtt.publish((prefix + "aircraft-count").c_str(), countBuf, true);
    mqtt.publish((prefix + "watchlist-alert").c_str(), watchlistAlert ? "ON" : "OFF", true);
    mqtt.publish((prefix + "proximity-alert").c_str(), proximityAlert ? "ON" : "OFF", true);

    // WLAN-Signalstaerke und Firmware-Version - neu fuer die Home-Assistant-
    // Discovery-Sensoren (siehe publishDiscovery()), werden bei jedem
    // erfolgreichen ADS-B-Zyklus mitgeschickt statt eines eigenen Timers -
    // gleiche Herleitung wie die drei bestehenden Kennzahlen oben, minimaler
    // Zusatzaufwand.
    char rssiBuf[8];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d", (int)WiFi.RSSI());
    mqtt.publish((prefix + "wifi-rssi").c_str(), rssiBuf, true);
    mqtt.publish((prefix + "firmware-version").c_str(), Config::APP_VERSION, true);
}

}
