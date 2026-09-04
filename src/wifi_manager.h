#pragma once
#include <Arduino.h>
#include "config.h"

namespace WifiMgr {
    enum class State {
        Idle, Connecting, Connected, Failed, NoCredentials
    };

    void init();
    void beginConnect();
    void connectTo(const char* ssid, const char* password);

    void update();
    State getState();
    const char* getIP();

    uint8_t networkCount();
    String networkSsid(uint8_t index);
    // hidden: true, wenn die SSID ueber den "Andere/versteckte SSID"-
    // Eingabepfad hinzugefuegt wurde (nicht aus einer Scan-Auswahl) -
    // siehe wifi_setup_screen.cpp. Steuert, ob beginConnect()/
    // tryReconnectAsync() beim (Wieder-)Verbinden einen Treffer im
    // normalen Netzwerk-Scan voraussetzen (versteckte SSIDs erscheinen
    // dort nie) oder direkt per WiFi.begin() mit explizitem SSID-
    // Parameter verbinden. Default false, damit bestehende Aufrufer
    // unveraendert funktionieren.
    bool addNetwork(const char* ssid, const char* password, bool hidden = false);
    void removeNetwork(uint8_t index);

    void beginScan();
    bool isScanComplete();
    int  getScanResultCount();
    String getScanResultSSID(int index);
    int32_t getScanResultRSSI(int index);
}