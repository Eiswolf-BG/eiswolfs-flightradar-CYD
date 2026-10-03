#include "airport_lookup.h"
#include "airports_data.h"
#include <cstring>
#include <math.h>

namespace AirportLookup {

namespace {
    // Rueckbau auf direktes Flash-Lesen (Alex' Wunsch 02.10.) - vorher
    // wurde kAirportsBin nur als Einmal-Seed fuer eine SD-Kartendatei
    // genutzt, die findNearest() dann bei JEDER Abfrage komplett neu
    // einlas (SD-Zugriff + Mutex, siehe Git-Historie). Jetzt direkt aus
    // dem im Flash eingebetteten kAirportsBin gelesen - kein SD-Zugriff,
    // kein Mutex, keine Blockierung mehr durch SD-Karten-I/O. Format
    // unveraendert (4 Byte Magic "APR2" + 2 Byte Datensatz-Anzahl + N x
    // 12-Byte-Datensaetze), siehe airports_data.cpp.
    constexpr uint8_t AIRPORTS_MAGIC[4] = {'A', 'P', 'R', '2'};
    constexpr size_t RECORD_SIZE = 12;
    constexpr size_t HEADER_SIZE = 6;

    constexpr double EARTH_RADIUS_KM = 6371.0088;
    constexpr double DEG2RAD = M_PI / 180.0;

    // Reine Distanz-Haversine ohne Peilung (die findNearest() gar nicht
    // braucht) - RadarMath::toPolar() berechnet zusaetzlich noch die
    // Anfangspeilung (weitere ~5 trigonometrische Aufrufe), was beim
    // Scannen von ~5000 Datensaetzen den groessten Teil der gemessenen
    // Abfragezeit ausmachte (auf dem ESP32 gibt es keine Hardware-FPU fuer
    // doppelte Genauigkeit - jede sin/cos/atan2-Berechnung mit double lief
    // in Software). Bewusst hier lokal dupliziert statt RadarMath::toPolar()
    // selbst zu aendern, da diese Funktion auch fuer die
    // Radarschirm-Darstellung JEDES sichtbaren Flugzeugs genutzt wird - eine
    // Aenderung dort haette eine deutlich groessere Auswirkung als hier
    // gewollt.
    double distanceKmOnly(double lat0, double lon0, double lat1, double lon1) {
        double phi1 = lat0 * DEG2RAD;
        double phi2 = lat1 * DEG2RAD;
        double dPhi = (lat1 - lat0) * DEG2RAD;
        double dLambda = (lon1 - lon0) * DEG2RAD;

        double a = sin(dPhi / 2) * sin(dPhi / 2) +
                   cos(phi1) * cos(phi2) * sin(dLambda / 2) * sin(dLambda / 2);
        double c = 2 * atan2(sqrt(a), sqrt(1 - a));
        return EARTH_RADIUS_KM * c;
    }
}

Nearest findNearest(double lat, double lon) {
    Nearest result;
    if (kAirportsBinLen < HEADER_SIZE ||
        memcmp(kAirportsBin, AIRPORTS_MAGIC, sizeof(AIRPORTS_MAGIC)) != 0) {
        return result;
    }
    uint16_t count = (uint16_t)kAirportsBin[4] | ((uint16_t)kAirportsBin[5] << 8);

    float bestDistanceKm = 0;
    const uint8_t* records = kAirportsBin + HEADER_SIZE;
    size_t available = (kAirportsBinLen - HEADER_SIZE) / RECORD_SIZE;
    size_t n = count < available ? count : available;

    for (size_t i = 0; i < n; i++) {
        const uint8_t* rec = records + i * RECORD_SIZE;

        int32_t latMicro = (int32_t)((uint32_t)rec[4] | ((uint32_t)rec[5] << 8) |
                                      ((uint32_t)rec[6] << 16) | ((uint32_t)rec[7] << 24));
        int32_t lonMicro = (int32_t)((uint32_t)rec[8] | ((uint32_t)rec[9] << 8) |
                                      ((uint32_t)rec[10] << 16) | ((uint32_t)rec[11] << 24));
        double aptLat = latMicro / 1000000.0;
        double aptLon = lonMicro / 1000000.0;

        float distanceKm = (float)distanceKmOnly(lat, lon, aptLat, aptLon);

        if (!result.found || distanceKm < bestDistanceKm) {
            bestDistanceKm = distanceKm;
            result.found = true;
            result.distanceKm = distanceKm;
            result.lat = aptLat;
            result.lon = aptLon;
            memcpy(result.icao, rec, 4);
            result.icao[4] = '\0';
        }
    }

    return result;
}

}
