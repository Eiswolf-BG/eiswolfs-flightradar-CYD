#include "airport_lookup.h"
#include "sd_storage.h"
#include "config.h"
#include "radar_math.h"
#include <SD.h>

namespace AirportLookup {

Nearest findNearest(double lat, double lon) {
    Nearest result;
    if (!SdStorage::isMounted()) return result;

    File f = SD.open(Config::SD_AIRPORTS_CSV);
    if (!f) return result;

    f.readStringUntil('\n');

    float bestDistanceKm = 0;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        int c1 = line.indexOf(',');
        if (c1 < 0) continue;
        int c2 = line.indexOf(',', c1 + 1);
        if (c2 < 0) continue;
        int c3 = line.indexOf(',', c2 + 1);
        if (c3 < 0) continue;

        String icao = line.substring(0, c1);
        String name = line.substring(c1 + 1, c2);
        double aptLat = line.substring(c2 + 1, c3).toDouble();
        double aptLon = line.substring(c3 + 1).toDouble();

        float distanceKm = RadarMath::toPolar(lat, lon, aptLat, aptLon).distanceKm;

        if (!result.found || distanceKm < bestDistanceKm) {
            bestDistanceKm = distanceKm;
            result.found = true;
            result.distanceKm = distanceKm;
            result.lat = aptLat;
            result.lon = aptLon;
            strncpy(result.icao, icao.c_str(), sizeof(result.icao) - 1);
            strncpy(result.name, name.c_str(), sizeof(result.name) - 1);
        }
    }
    f.close();

    return result;
}

}
