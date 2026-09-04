#include "airport_lookup.h"
#include "sd_storage.h"
#include "config.h"
#include "sd_mutex.h"
#include <SD.h>
#include <cstring>
#include <math.h>

namespace AirportLookup {

namespace {
    // Muss zu AIRPORTS_MAGIC in sd_storage.cpp passen - siehe dortiger
    // Kommentar fuer das komplette Binaerformat (4 Byte Magic + 2 Byte
    // Datensatz-Anzahl + N x 12-Byte-Datensaetze).
    constexpr uint8_t AIRPORTS_MAGIC[4] = {'A', 'P', 'R', '2'};
    constexpr size_t RECORD_SIZE = 12;
    // Records werden in Bloecken statt einzeln gelesen - ein File::read()
    // pro 12-Byte-Datensatz hatte bei ~5000 Eintraegen (siehe
    // airports_data.h) auf echter Hardware ueber 2 Sekunden pro Abfrage
    // gebraucht (SD-Treiber-/SPI-Overhead pro Aufruf dominierte klar
    // gegenueber der eigentlichen Transferzeit). 256 Datensaetze
    // (3072 Byte) pro Block halten den Puffer klein genug, um nicht in die
    // gleiche Heap-Fragmentierungs-Falle wie beim ADS-B-JSON-Parsing zu
    // laufen (siehe CLAUDE.md), reduzieren die Anzahl der read()-Aufrufe
    // aber um den Faktor 256.
    constexpr size_t RECORDS_PER_CHUNK = 256;
    constexpr size_t CHUNK_SIZE = RECORDS_PER_CHUNK * RECORD_SIZE;

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
    if (!SdStorage::isMounted()) return result;

    // Lock hinzugefuegt, weil findNearest() jetzt nicht mehr nur vom
    // Haupt-Loop (Core 1, z.B. Standort-Presets-Screen) aufgerufen wird,
    // sondern auch periodisch aus Weather::update() auf NetTask (Core 0,
    // siehe weather.cpp) - ohne dieses Lock waeren dann zeitgleiche
    // SD-Zugriffe von beiden Cores moeglich (siehe sd_mutex.h).
    SdMutex::Guard guard;

    File f = SD.open(Config::SD_AIRPORTS_CSV);
    if (!f) return result;

    uint8_t header[6];
    if (f.read(header, sizeof(header)) != sizeof(header) ||
        memcmp(header, AIRPORTS_MAGIC, sizeof(AIRPORTS_MAGIC)) != 0) {
        f.close();
        return result;
    }
    uint16_t count = (uint16_t)header[4] | ((uint16_t)header[5] << 8);

    float bestDistanceKm = 0;
    uint8_t chunk[CHUNK_SIZE];
    uint16_t remaining = count;
    while (remaining > 0) {
        size_t recordsThisChunk = remaining < RECORDS_PER_CHUNK ? remaining : RECORDS_PER_CHUNK;
        size_t bytesToRead = recordsThisChunk * RECORD_SIZE;
        size_t bytesRead = f.read(chunk, bytesToRead);
        size_t recordsRead = bytesRead / RECORD_SIZE;
        if (recordsRead == 0) break;

        for (size_t r = 0; r < recordsRead; r++) {
            const uint8_t* rec = chunk + r * RECORD_SIZE;

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

        if (recordsRead < recordsThisChunk) break;
        remaining -= (uint16_t)recordsRead;
    }
    f.close();

    return result;
}

}
