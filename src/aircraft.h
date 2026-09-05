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

    bool     valid          = false;

    char     airlineName[24] = {0};
    uint16_t estSeats         = 0;
};