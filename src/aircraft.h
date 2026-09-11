#pragma once
#include <Arduino.h>
struct Aircraft {
    char     hex[7]      = {0};
    char     callsign[9] = {0};
    char     reg[9]      = {0};
    char     typeCode[5] = {0};
    char     squawk[5]   = {0};
    char     category[3] = {0};

    float    lat            = 0;
    float    lon            = 0;
    int32_t  altBaroFt      = 0;
    int16_t  vertRateFtMin  = 0;
    float    groundSpeedKt  = 0;
    float    headingDeg     = 0;

    float    distanceKm     = 0;
    float    bearingDeg     = 0;

    uint32_t lastSeenMs     = 0;
    bool     alerted        = false;
    uint32_t alertedAtMs    = 0;

    // Zuletzt bekannte Distanz-Zone fuer den "Intelligenten" Naeherungs-
    // alarm (SettingsStore::proximityAlertSmartMode(), siehe
    // radar_screen.cpp::updateProximityAlert()) - 0=ausserhalb aller Zonen,
    // 1=Gelb (<20km), 2=Orange (<10km), 3=Rot (<5km). Wird JEDEN Zyklus rein
    // geometrisch aus der Distanz aktualisiert (unabhaengig vom Hoehen-
    // filter), damit ein spaeteres erneutes Anfliegen nach einem Rueckzug
    // wieder korrekt als neue Annaeherung erkannt wird. Der eigentliche
    // Alarm loest nur aus, wenn die aktuelle Zone GROESSER als dieser
    // gespeicherte Wert ist (= echte Annaeherung, nicht nur "noch drin").
    uint8_t  proximityZone  = 0;

    // Zuletzt bekannter Watchlist-Zustand (Rufzeichen- oder Squawk-
    // Wachliste, siehe radar_screen.cpp::updateProximityAlert()) - JEDEN
    // Zyklus aktualisiert, analog zum proximityZone-Mechanismus oben.
    // Damit laesst sich der Uebergang "war nicht beobachtet -> wird jetzt
    // beobachtet" erkennen (fuer den kurzen Einzelton des SPK-Lautsprecher-
    // Alarms, siehe speaker_alert.h) - ein reiner Zustandsvergleich wie
    // false->true statt einer separaten Hex-Liste wie im JS-Pendant der
    // Web-Seite, da die Aircraft-Tabelle hier bereits die passendere Stelle
    // dafuer bietet.
    bool     wasWatched     = false;

    // Best-Effort-Anflug-Erkennung auf den naechstgelegenen Flughafen
    // (Weather::currentNearestAirport(), dieselbe Referenz wie die
    // "Naechster Flughafen"-Eckanzeige) - rein geometrisch aus bereits
    // vorhandenen Live-Daten abgeleitet, siehe aircraft_table.cpp::
    // postFetchUpdate(). prevAirportDistKm haelt die im VORHERIGEN Zyklus
    // gemessene Distanz zu diesem Flughafen fest (analog zum bestehenden
    // proximityZone-Mechanismus oben) - noetig, um "Distanz sinkt ueber
    // die letzten Zyklen" ueberhaupt pruefen zu koennen. -1 = noch keine
    // vorherige Messung (erster Zyklus bzw. Flughafen-Referenz hat
    // gewechselt), dann gilt "sinkend" noch nicht als erfuellt.
    float    prevAirportDistKm = -1;
    bool     approachLikely    = false;
    // Nur gueltig, wenn approachLikely true ist (siehe drawDetailPanel()) -
    // gerundete ETA in Minuten, nur im plausiblen Bereich (1-60min)
    // ueberhaupt gesetzt, siehe postFetchUpdate().
    uint16_t approachEtaMin    = 0;

    // Naeherungs-/Entfernungs-Trend ("Naehert sich"/"Entfernt sich"/"Fliegt
    // vorbei") fuers Detail-Panel (radar_screen.cpp::drawDetailPanel()) -
    // rein geometrisch aus der Distanz zum eigenen Standort ueber die
    // letzten Zyklen abgeleitet, siehe aircraft_table.cpp::
    // postFetchUpdate(). Gleiches "vorherige Messung merken"-Muster wie
    // prevAirportDistKm oben. prevDistanceKm = -1 bedeutet "noch keine
    // vorherige Messung" (erster Zyklus dieses Flugzeugs).
    enum class DistanceTrend : uint8_t { Unknown = 0, Approaching, Departing, Passing };
    float          prevDistanceKm = -1;
    DistanceTrend  distanceTrend  = DistanceTrend::Unknown;

    // "Ueberflug"-CPA (Closest Point of Approach) fuers Detail-Panel (siehe
    // radar_screen.cpp::drawDetailPanel()) - Standard-Navigationsformel aus
    // Position, Kurs und Geschwindigkeit DIESES einen Zyklus (kein
    // "vorherige Messung merken"-Muster noetig wie bei prevDistanceKm oben,
    // die Formel extrapoliert direkt aus dem aktuellen Bewegungsvektor).
    // Nur gueltig/gesetzt, wenn cpaRelevant true ist (siehe
    // aircraft_table.cpp::postFetchUpdate() fuer alle Bedingungen:
    // Flugzeug naehert sich UEBERHAUPT (distanceTrend == Approaching),
    // errechnete Zeit bis zum naechsten Punkt liegt im Fenster 0 bis
    // Config::CPA_MAX_TIME_MIN, UND die dabei erreichte Minimaldistanz
    // liegt unter Config::CPA_MAX_DISTANCE_KM - sonst waere die Anzeige nur
    // Rauschen fuer Flugbahnen, die ohnehin nie nah vorbeikommen).
    bool     cpaRelevant = false;
    float    cpaEtaMin   = 0;

    // "First Seen"/"Seen For" fuers Detail-Panel (radar_screen.cpp::
    // drawDetailPanel()) - rein session-lokal (nur RAM, keine SD-Karte/kein
    // Logbuch), setzt sich bei jedem Geraete-Neustart zurueck. Merkt sich
    // millis() beim allerersten Sichten dieses Flugzeugs (per Hex-Code) in
    // der laufenden Sitzung, siehe adsb_client.cpp::fetch(). GENAU wie
    // prevDistanceKm oben MUSS dieser Wert ueber den Fetch-Zyklus-
    // Schnappschuss in adsb_client.cpp hinweg erhalten bleiben (siehe
    // PrevFirstSeen dort) - sonst wuerde "a = Aircraft{}" ihn dort bei
    // JEDEM Zyklus auf 0 zuruecksetzen, genau derselbe Bug-Mechanismus wie
    // beim urspruenglichen prevDistanceKm-Fehler (siehe dessen Kommentar).
    // 0 bedeutet "noch nicht gesetzt" (wird beim ersten Sichten in
    // adsb_client.cpp auf den aktuellen millis()-Wert gesetzt). Bleibt
    // weiterhin die Grundlage fuer "Sichtbar seit" (reine Dauer, immer aus
    // millis() ableitbar, egal ob die Uhrzeit schon NTP-synchronisiert ist)
    // UND dient als "wurde dieses Flugzeug schon einmal gesehen"-Sentinel
    // fuer firstSeenEpoch unten.
    uint32_t firstSeenMs = 0;

    // Echte Wanduhrzeit (Unix-Epoch, Sekunden) zum Zeitpunkt des ersten
    // Sichtens - fuer die "Erstmals gesehen: HH:MM:SS"-Anzeige (siehe
    // radar_screen.cpp::drawDetailPanel()), auf Alex' Wunsch eine
    // tatsaechliche Tageszeit statt der Boot-relativen firstSeenMs oben.
    // Wird GENAU EINMAL zusammen mit firstSeenMs gesetzt (siehe
    // adsb_client.cpp::fetch()) - NUR wenn die Systemzeit in genau diesem
    // Moment bereits NTP-synchronisiert ist (time(nullptr) > 8*3600*2,
    // gleiche Pruefung wie ueberall sonst im Projekt, z.B. flight_logbook.
    // cpp::checkAutoOff()). War sie es zu diesem Zeitpunkt noch nicht,
    // bleibt firstSeenEpoch bewusst dauerhaft 0 fuer dieses Flugzeug (KEIN
    // nachtraegliches "Aufholen" mit einem spaeteren, dann zwar gueltigen,
    // aber nicht mehr zum tatsaechlichen Erstsichten passenden Zeitstempel)
    // - die Anzeige laesst "Erstmals gesehen" in diesem Fall einfach weg,
    // statt eine falsche Uhrzeit zu zeigen. Wie firstSeenMs MUSS auch
    // dieser Wert ueber den Fetch-Zyklus-Schnappschuss in adsb_client.cpp
    // hinweg erhalten bleiben (siehe PrevFirstSeen dort).
    uint32_t firstSeenEpoch = 0;

    // Circle-Crossing-Puls auf dem Radar (radar_screen.cpp, rein visuell,
    // kein Ton/keine LED) - millis()-Zeitstempel des letzten tatsaechlichen
    // Durchquerens eines der drei angezeigten Entfernungsringe (1/3, 2/3,
    // Aussenrand der AKTUELLEN Anzeige-Reichweite), in BEIDE Richtungen.
    // Anders als firstSeenMs/prevDistanceKm braucht dieses Feld KEINE
    // Erhaltung ueber den adsb_client.cpp-Schnappschuss hinweg - es wird in
    // JEDEM aircraft_table.cpp::postFetchUpdate()-Durchlauf entweder neu
    // gesetzt (falls in diesem Zyklus tatsaechlich eine Ringgrenze
    // ueberquert wurde) oder bleibt bei 0 (kein Aussage ueber "kein
    // Puls mehr noetig" - die 1-2s-Pulsdauer selbst wird rein zeitbasiert
    // in radar_screen.cpp anhand dieses Zeitstempels ausgewertet, ein
    // Zuruecksetzen durch den naechsten Fetch-Zyklus 10s spaeter kommt
    // dafuer ohnehin viel zu spaet, um relevant zu sein). 0 = kein
    // (kuerzlicher) Ringdurchgang.
    uint32_t ringCrossedAtMs = 0;

    // Flugzeug-Steckbrief fuers Flugbuch (siehe flight_logbook.cpp::
    // update()/writeLogLine()) - kuerzeste je gemessene Distanz bzw.
    // hoechste je gemessene Geschwindigkeit SEIT dem ersten Sichten dieses
    // Flugzeugs in der laufenden Sitzung, rein session-lokal (kein SD-
    // Zugriff hier). Wird JEDEN Zyklus in aircraft_table.cpp::
    // postFetchUpdate() aktualisiert, analog zum bestehenden
    // prevDistanceKm-Muster. Muss GENAU wie prevDistanceKm/firstSeenMs
    // ueber den Fetch-Zyklus-Schnappschuss in adsb_client.cpp hinweg
    // erhalten bleiben (siehe dortiges PrevProfile), sonst wuerde "a =
    // Aircraft{}" die bisherigen Extremwerte bei JEDEM Zyklus verwerfen.
    // -1 = noch keine Messung (erster Zyklus dieses Flugzeugs).
    float    sessionMinDistanceKm = -1;
    float    sessionMaxSpeedKt    = -1;

    bool     valid          = false;

    char     airlineName[24] = {0};
    uint16_t estSeats         = 0;
};