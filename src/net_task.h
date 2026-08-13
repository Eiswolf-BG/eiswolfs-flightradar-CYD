#pragma once

// Hintergrund-Task fuer WLAN-Status, Standortbestimmung und ADS-B-Abfragen.
// Laeuft auf Core 0, damit Core 1 (Rendering + Touch im main-Loop) nie durch
// Netzwerk-Wartezeiten (WLAN, HTTPS) blockiert wird.
namespace NetTask {
    // Startet den Hintergrund-Task. Muss erst NACH WifiMgr::init() und
    // LocationManager::init() aufgerufen werden.
    void begin();

    // Haelt den Hintergrund-Task an bzw. setzt ihn fort - z.B. waehrend
    // eines OTA-Updates, da der ESP32 nur eine WLAN-Funkeinheit/einen
    // gemeinsamen Netzwerk-Stack fuer beide Cores hat und gleichzeitige
    // Anfragen (ADS-B-Polling im Hintergrund vs. OTA-Download) sich in die
    // Quere kommen koennen. Beide sicher mehrfach aufrufbar bzw. ohne
    // Wirkung, falls der Task noch nicht gestartet ist.
    void pause();
    void resume();
}