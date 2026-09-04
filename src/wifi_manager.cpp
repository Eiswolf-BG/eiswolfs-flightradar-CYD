#include "wifi_manager.h"
#include "sd_storage.h"
#include "sd_mutex.h"
#include <WiFi.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>

namespace WifiMgr {

namespace {
    struct NetworkEntry {
        char ssid[33] = {0};
        char pass[64] = {0};
        // Siehe addNetwork()-Kommentar in wifi_manager.h - true fuer
        // manuell (nicht per Scan-Auswahl) hinzugefuegte SSIDs.
        bool hidden = false;
    };

    NetworkEntry networks[Config::MAX_WIFI_NETWORKS];
    uint8_t networkCountVal = 0;

    // Kopfzeile des NEUEN, hidden-flag-faehigen SD-Dateiformats (3 Zeilen
    // pro Netzwerk: SSID/Passwort/"0"-oder-"1"). Dateien von VOR diesem
    // Fix haben diese Kopfzeile nicht (altes Format: nur 2 Zeilen pro
    // Netzwerk, kein Hidden-Flag) - loadFromSd() erkennt das am Fehlen
    // dieser Zeile und faellt automatisch auf das alte 2-Zeilen-Format
    // zurueck (dort ist hidden zwangslaeufig immer false, da das alte
    // Format es nie gespeichert hat). saveToSd() schreibt ab jetzt immer
    // im neuen Format - nach dem naechsten Speichern (z.B. Hinzufuegen/
    // Entfernen eines beliebigen Netzwerks) liegt die Datei dauerhaft im
    // neuen Format vor. WICHTIG: ein VOR diesem Fix manuell gespeichertes
    // verstecktes Netzwerk wird dadurch NICHT rueckwirkend als "hidden"
    // erkannt (das alte Format kann das schlicht nicht ausdruecken) - so
    // ein Eintrag muss einmalig ueber "Andere/versteckte SSID" neu
    // eingegeben werden, danach ist er korrekt markiert und uebersteht
    // jeden weiteren Neustart.
    constexpr const char* WIFI_CRED_FILE_HEADER = "WIFI2";

    State state = State::Idle;
    uint32_t connectStartMs = 0;
    constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
    char ipStr[16] = {0};

    uint32_t lastReconnectAttemptMs = 0;
    constexpr uint32_t RECONNECT_RETRY_MS = 10000;

    bool reconnectScanPending = false;

    SemaphoreHandle_t mutex = nullptr;

    void setState(State s) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        state = s;
        xSemaphoreGive(mutex);
    }

    void loadFromSd() {
        networkCountVal = 0;
        if (!SdStorage::isMounted()) return;

        SdMutex::Guard guard;

        if (!SD.exists(Config::SD_WIFI_CREDENTIALS_FILE)) return;

        File f = SD.open(Config::SD_WIFI_CREDENTIALS_FILE, FILE_READ);
        if (!f) return;

        // Format-Erkennung: neues Format beginnt mit der WIFI_CRED_FILE_HEADER-
        // Kopfzeile (siehe dortiger Kommentar), danach 3 Zeilen pro Netzwerk
        // (SSID/Passwort/Hidden-Flag). Fehlt die Kopfzeile, ist es eine
        // Datei von VOR diesem Fix (altes 2-Zeilen-Format, kein Hidden-
        // Flag) - die bereits gelesene erste Zeile ist in diesem Fall
        // bereits die erste SSID und wird entsprechend weiterverarbeitet.
        String firstLine = f.readStringUntil('\n');
        firstLine.trim();
        bool newFormat = (firstLine == WIFI_CRED_FILE_HEADER);

        String pendingSsid;
        bool havePendingSsid = false;
        if (!newFormat) {
            pendingSsid = firstLine;
            havePendingSsid = true;
        }

        while (networkCountVal < Config::MAX_WIFI_NETWORKS) {
            String ssid;
            if (havePendingSsid) {
                ssid = pendingSsid;
                havePendingSsid = false;
            } else {
                if (!f.available()) break;
                ssid = f.readStringUntil('\n');
            }
            ssid.trim();
            if (ssid.length() == 0) break;
            if (!f.available()) break;
            String pass = f.readStringUntil('\n');
            pass.trim();

            bool hidden = false;
            if (newFormat) {
                if (!f.available()) break;
                String hiddenFlag = f.readStringUntil('\n');
                hiddenFlag.trim();
                hidden = (hiddenFlag == "1");
            }

            strncpy(networks[networkCountVal].ssid, ssid.c_str(), sizeof(networks[networkCountVal].ssid) - 1);
            strncpy(networks[networkCountVal].pass, pass.c_str(), sizeof(networks[networkCountVal].pass) - 1);
            networks[networkCountVal].hidden = hidden;
            networkCountVal++;
        }
        f.close();
    }

    void saveToSd() {
        if (!SdStorage::isMounted()) return;

        SdMutex::Guard guard;

        File f = SD.open(Config::SD_WIFI_CREDENTIALS_FILE, FILE_WRITE);
        if (!f) return;
        // Immer im neuen, hidden-flag-faehigen Format schreiben (siehe
        // WIFI_CRED_FILE_HEADER-Kommentar oben) - auch wenn die Datei
        // urspruenglich im alten Format geladen wurde, liegt sie ab dem
        // ersten Speichern danach dauerhaft im neuen Format vor.
        f.println(WIFI_CRED_FILE_HEADER);
        for (uint8_t i = 0; i < networkCountVal; i++) {
            f.println(networks[i].ssid);
            f.println(networks[i].pass);
            f.println(networks[i].hidden ? "1" : "0");
        }
        f.close();
    }

    void tryReconnectAsync() {
        if (!reconnectScanPending) {
            WiFi.scanNetworks(/*async=*/true);
            reconnectScanPending = true;
            return;
        }

        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) return;

        reconnectScanPending = false;

        // Versteckte Netzwerke brauchen KEINEN Scan-Treffer (siehe
        // NetworkEntry::hidden-Kommentar in wifi_manager.h - sie erscheinen
        // in einem normalen Scan nie) - deshalb hier zuerst geprueft, in
        // Speicher-Reihenfolge (= Prioritaet, wie bisher), unabhaengig
        // davon, ob der Scan ueberhaupt Ergebnisse lieferte.
        int8_t chosen = -1;
        for (uint8_t i = 0; i < networkCountVal && chosen < 0; i++) {
            if (networks[i].hidden) {
                chosen = (int8_t)i;
                break;
            }
            if (n <= 0) continue;
            for (int j = 0; j < n; j++) {
                if (WiFi.SSID(j) == networks[i].ssid) {
                    chosen = (int8_t)i;
                    break;
                }
            }
        }
        WiFi.scanDelete();

        if (chosen < 0) return;

        connectTo(networks[chosen].ssid, networks[chosen].pass);
    }
}

void init() {
    if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    loadFromSd();
    setState(networkCountVal > 0 ? State::Idle : State::NoCredentials);
}

uint8_t networkCount() { return networkCountVal; }

String networkSsid(uint8_t index) {
    if (index >= networkCountVal) return String();
    return String(networks[index].ssid);
}

bool addNetwork(const char* ssid, const char* password, bool hidden) {
    if (networkCountVal >= Config::MAX_WIFI_NETWORKS) return false;

    strncpy(networks[networkCountVal].ssid, ssid, sizeof(networks[networkCountVal].ssid) - 1);
    strncpy(networks[networkCountVal].pass, password, sizeof(networks[networkCountVal].pass) - 1);
    networks[networkCountVal].hidden = hidden;
    networkCountVal++;
    saveToSd();
    return true;
}

void removeNetwork(uint8_t index) {
    if (index >= networkCountVal) return;
    for (uint8_t i = index; i < networkCountVal - 1; i++) {
        networks[i] = networks[i + 1];
    }
    networkCountVal--;
    networks[networkCountVal] = NetworkEntry{};
    saveToSd();
}

void connectTo(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    connectStartMs = millis();
    setState(State::Connecting);
}

void beginConnect() {
    if (networkCountVal == 0) {
        setState(State::NoCredentials);
        return;
    }

    int visibleCount = WiFi.scanNetworks(/*async=*/false);

    // Versteckte Netzwerke brauchen KEINEN Scan-Treffer (siehe
    // NetworkEntry::hidden-Kommentar in wifi_manager.h - sie erscheinen in
    // einem normalen Scan nie, das war die eigentliche Ursache des
    // "verstecktes WLAN verbindet nach Neustart nicht"-Bugs: WiFi.begin()
    // wurde fuer diese Eintraege bisher gar nicht erst erreicht, weil der
    // vorherige Scan-Match-Schritt sie immer aussortierte). Reihenfolge
    // (= Prioritaet) bleibt wie bisher die Speicher-Reihenfolge.
    int8_t chosen = -1;
    for (uint8_t i = 0; i < networkCountVal && chosen < 0; i++) {
        if (networks[i].hidden) {
            chosen = (int8_t)i;
            break;
        }
        for (int j = 0; j < visibleCount; j++) {
            if (WiFi.SSID(j) == networks[i].ssid) {
                chosen = (int8_t)i;
                break;
            }
        }
    }

    if (chosen < 0) {
        setState(State::Failed);
        return;
    }

    connectTo(networks[chosen].ssid, networks[chosen].pass);
}

void update() {
    State s = getState();

    if (s == State::Connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            strncpy(ipStr, WiFi.localIP().toString().c_str(), sizeof(ipStr) - 1);
            setState(State::Connected);
            return;
        }
        if (millis() - connectStartMs > CONNECT_TIMEOUT_MS) {
            WiFi.disconnect(true);
            setState(State::Failed);
        }
        return;
    }

    if (s == State::Connected && WiFi.status() != WL_CONNECTED) {
        Serial.println("[WifiMgr] Verbindung verloren, versuche automatisch neu zu verbinden...");
        setState(State::Idle);
        lastReconnectAttemptMs = millis() - RECONNECT_RETRY_MS;
        return;
    }

    if ((s == State::Idle || s == State::Failed) && networkCountVal > 0) {
        if (reconnectScanPending) {
            tryReconnectAsync();
        } else if (millis() - lastReconnectAttemptMs >= RECONNECT_RETRY_MS) {
            lastReconnectAttemptMs = millis();
            tryReconnectAsync();
        }
    }
}

State getState() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    State s = state;
    xSemaphoreGive(mutex);
    return s;
}

void beginScan() {
    WiFi.scanNetworks(/*async=*/true);
}

bool isScanComplete() {
    return WiFi.scanComplete() >= 0;
}

int getScanResultCount() {
    int n = WiFi.scanComplete();
    return n > 0 ? n : 0;
}

String getScanResultSSID(int index) {
    return WiFi.SSID(index);
}

int32_t getScanResultRSSI(int index) {
    return WiFi.RSSI(index);
}

const char* getIP() { return ipStr; }

}