#pragma once
#include <cstdint>

namespace RadarMath {

    struct PolarCoord {
        float distanceKm;
        float bearingDeg; // 0-360, 0 = North, clockwise
    };

    struct ScreenPoint {
        int16_t x;
        int16_t y;
    };
    PolarCoord toPolar(double lat0, double lon0, double lat1, double lon1);

    // rotationOffsetDeg dreht die Bildschirm-Darstellung zusaetzlich um den
    // angegebenen Winkel (im Uhrzeigersinn) - fuer den "Follow-Me Modus"
    // (radar_screen.cpp), der das Radar statt fest nach Norden an der
    // aktuellen Fahrtrichtung ausrichtet. Default 0 = unveraendertes
    // bisheriges Verhalten (Norden = oben), bestehende Aufrufer bleiben
    // dadurch unangetastet.
    ScreenPoint toScreen(const PolarCoord& polar, int16_t centerX, int16_t centerY,
                         int16_t radiusPx, float rangeKm, float rotationOffsetDeg = 0.0f);

}