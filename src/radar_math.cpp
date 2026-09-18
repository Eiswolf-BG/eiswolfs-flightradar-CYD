#include "radar_math.h"
#include <math.h>

namespace RadarMath {

namespace {
    constexpr double EARTH_RADIUS_KM = 6371.0088;
    constexpr double DEG2RAD = M_PI / 180.0;
    constexpr double RAD2DEG = 180.0 / M_PI;
}

PolarCoord toPolar(double lat0, double lon0, double lat1, double lon1) {
    double phi1 = lat0 * DEG2RAD;
    double phi2 = lat1 * DEG2RAD;
    double dPhi = (lat1 - lat0) * DEG2RAD;
    double dLambda = (lon1 - lon0) * DEG2RAD;

    // Haversine
    double a = sin(dPhi / 2) * sin(dPhi / 2) +
               cos(phi1) * cos(phi2) * sin(dLambda / 2) * sin(dLambda / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    double distanceKm = EARTH_RADIUS_KM * c;

    // Initial bearing
    double y = sin(dLambda) * cos(phi2);
    double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(dLambda);
    double bearing = atan2(y, x) * RAD2DEG;
    bearing = fmod(bearing + 360.0, 360.0);

    return PolarCoord{ static_cast<float>(distanceKm), static_cast<float>(bearing) };
}

ScreenPoint toScreen(const PolarCoord& polar, int16_t centerX, int16_t centerY,
                     int16_t radiusPx, float rangeKm, float rotationOffsetDeg) {
    float clampedKm = polar.distanceKm > rangeKm ? rangeKm : polar.distanceKm;
    float r = (clampedKm / rangeKm) * radiusPx;

    // bearing 0 = North = "up" on screen = negative Y direction.
    // rotationOffsetDeg wird VOR der Umrechnung abgezogen - bei "Follow-Me
    // Modus" ist das die aktuelle Fahrtrichtung, dadurch landet ein
    // Flugzeug genau in Fahrtrichtung wieder "oben" auf dem Bildschirm statt
    // bei seiner tatsaechlichen Kompass-Peilung.
    double rad = (polar.bearingDeg - rotationOffsetDeg) * DEG2RAD;
    float dx = r * sin(rad);
    float dy = -r * cos(rad);

    return ScreenPoint{
        static_cast<int16_t>(centerX + dx),
        static_cast<int16_t>(centerY + dy)
    };
}

} // namespace RadarMath