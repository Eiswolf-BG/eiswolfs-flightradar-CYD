#pragma once
#include <Arduino.h>

// Bonus-Feature: zeigt die Internationale Raumstation (ISS) auf dem Radar,
// falls sie sich gerade innerhalb des eingestellten Radius befindet - rein
// visuell, kein Alarm/Ton, keine Beeinflussung der normalen Flugzeug-
// Anzeige/-Filterung. Nutzt die kostenlose Open-Notify-API
// (http://api.open-notify.org/iss-now.json, kein API-Key noetig, nur
// HTTP - kein TLS-Handshake noetig, siehe iss_tracker.cpp). Da die ISS mit
// ~7,66 km/s extrem schnell ist, ist der Marker vermutlich nur fuer wenige
// Sekunden sichtbar, wenn ueberhaupt - das ist so beabsichtigt.
namespace IssTracker {

    struct Position {
        bool available = false;   // noch keine erfolgreiche Abfrage
        double lat = 0;
        double lon = 0;
        uint32_t fetchedAtMs = 0;
    };

    // Muss regelmaessig aus dem NetTask (Core 0) aufgerufen werden -
    // kuemmert sich intern um das Abfrageintervall
    // (Config::ISS_FETCH_INTERVAL_MS). Schlaegt die Abfrage fehl (kein
    // WLAN, API nicht erreichbar, unerwartete Antwort), bleibt einfach die
    // zuletzt bekannte Position stehen bzw. available=false, falls es noch
    // nie eine gab - kein Fehlerzustand, kein Einfluss auf den restlichen
    // Radar-Betrieb.
    void update();

    // Letzte bekannte Position - threadsicher genug fuer diesen Zweck
    // (nur vom NetTask geschrieben, vom UI-Thread gelesen, analog zu
    // Weather::current()).
    Position current();
}
