#pragma once
#include <cstdint>

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
    // Quere kommen koennen (siehe Kommentar bei netTaskIdle in
    // net_task.cpp).
    //
    // pause() suspendiert NICHT sofort - ein simples vTaskSuspend() greift
    // erst am naechsten Yield-/Blockierpunkt des Tasks, nicht mitten in
    // einer laufenden ADS-B-Netzwerkoperation (bis zu
    // Config::ADSB_HTTP_TIMEOUT_MS = 15s). Stattdessen wartet pause() aktiv
    // (kurzes Polling-Intervall), bis NetTask sich selbst als idle meldet
    // (also NICHT mitten in einem Fetch steckt), und suspendiert erst dann
    // wirklich - das schliesst das Zeitfenster, in dem ein gleichzeitiger
    // OTA-Download und eine laufende ADS-B-TLS-Verbindung um Heap-/TLS-
    // Ressourcen konkurrieren koennten (siehe Analyse mit Karl,
    // RSA/BIGNUM-Speicherproblem). Gibt false zurueck, wenn NetTask
    // innerhalb von timeoutMs nicht idle wurde (Task haengt fest) - der
    // Aufrufer sollte in diesem Fall NICHT mit dem OTA-Vorgang fortfahren,
    // sondern einen Fehler anzeigen, statt endlos zu haengen.
    bool pause(uint32_t timeoutMs = 16000);
    void resume();
}