#pragma once
#include <Arduino.h>

// Optionale Push-Benachrichtigung ueber den kostenlosen, anmeldefreien
// Dienst ntfy.sh (SettingsStore::ntfyPushEnabled()/ntfyPushTopic(), siehe
// ntfy_push_screen.cpp) bei Notfall-Squawk oder Watchlist-Treffer -
// wiederverwendet dieselbe Erkennung wie die bestehenden LED-Alerts
// (radar_screen.cpp::updateProximityAlert()), keine eigene Logik dafuer.
// Gleiches Cross-Core-Muster wie AircraftDetails (aircraft_details.h):
// request() wird von Core 1 aufgerufen und merkt nur eine Nachricht vor,
// update() wird periodisch von NetTask (Core 0) aufgerufen und fuehrt dort
// die eigentliche, blockierende HTTPS-POST-Anfrage aus - laeuft im
// Hintergrund wie jeder andere NetTask-Netzwerkaufruf, blockiert also nie
// die UI auf Core 1.
namespace NtfyPush {
    // Merkt eine zu sendende Nachricht vor (ueberschreibt eine evtl. noch
    // nicht versendete vorherige - es soll ohnehin immer nur die neueste
    // zaehlen). Macht selbst KEINE Netzwerkanfrage, nur ein Cross-Core-
    // Uebergabepuffer - sicher von Core 1 aufzurufen. enabled()/Topic-
    // Pruefung passiert bewusst NICHT hier, sondern beim Aufrufer (siehe
    // radar_screen.cpp) bzw. in update() unten - so kann z.B. der Test-
    // Button im Einstellungs-Screen den Haupt-Schalter bewusst umgehen.
    void request(const char* message);

    // Von NetTask (Core 0) periodisch aufgerufen - sendet eine
    // vorgemerkte Nachricht, falls vorhanden (blockierender HTTPS-POST-
    // Request an https://ntfy.sh/<Topic>, laeuft im Hintergrund wie
    // Weather::update() & Co.). Fehler (kein WLAN, kein Topic konfiguriert,
    // Server nicht erreichbar) werden bewusst still ignoriert - eine
    // fehlgeschlagene Zustellung darf nie die App zum Haengen bringen oder
    // abstuerzen lassen.
    void update();

    // Ruhezeiten (SettingsStore::ntfyQuietHoursEnabled()/-StartHour()/
    // -EndHour(), Alex' Wunsch) - true, wenn die aktuelle lokale Uhrzeit
    // gerade im eingestellten Ruhezeit-Fenster liegt UND die Ruhezeit
    // ueberhaupt eingeschaltet ist. Bewusst NICHT hier in request()/
    // update() geprueft (gleiches Prinzip wie ntfyPushEnabled(), siehe
    // Kommentar bei request() oben) - der Aufrufer (radar_screen.cpp)
    // entscheidet, ob eine konkrete Meldung ueberhaupt erst angestossen
    // wird. Der Test-Button (ntfy_push_screen.cpp) ruft das bewusst NICHT
    // auf, damit ein Testversand auch waehrend der Ruhezeit funktioniert
    // (gleiche Ausnahme wie beim Haupt-Ein/Aus-Schalter).
    //
    // Kein NTP-Zeitabgleich vorhanden -> false (kein "false positive"
    // Unterdruecken, solange die Uhrzeit noch unbekannt ist). Handhabt ein
    // ueber Mitternacht laufendes Fenster (z.B. 22 bis 7 Uhr) korrekt: bei
    // startHour > endHour gilt die Ruhezeit als aktiv, wenn die aktuelle
    // Stunde >= startHour ODER < endHour ist. startHour == endHour wird als
    // durchgehendes 24h-Fenster gewertet (kein sinnvoller 0-Stunden-Fall).
    bool isQuietHoursActive();
}
