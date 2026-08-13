#include "sun_times.h"
#include <math.h>

namespace SunTimes {

namespace {
    constexpr double DEG2RAD = 0.017453292519943295;
    constexpr double RAD2DEG = 57.29577951308232;

    int dayOfYear(int year, int month, int day) {
        static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        int n = cum[month - 1] + day;
        bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        if (leap && month > 2) n += 1;
        return n;
    }

    // Klassischer "Sunrise/Sunset Algorithm" (Almanac for Computers, 1990,
    // weit verbreitete Formel). sunrise=true fuer Sonnenaufgang, false fuer
    // Sonnenuntergang. Gibt false zurueck, wenn die Sonne an diesem Tag/
    // Breitengrad ueberhaupt nicht auf-/untergeht (Polartag/-nacht) - dann
    // ist outHour unveraendert.
    bool calc(double lat, double lon, int N, int32_t utcOffsetSeconds, bool sunrise, float& outHour) {
        double lngHour = lon / 15.0;
        double t = sunrise ? (N + ((6.0 - lngHour) / 24.0)) : (N + ((18.0 - lngHour) / 24.0));

        double M = (0.9856 * t) - 3.289;

        double L = M + (1.916 * sin(M * DEG2RAD)) + (0.020 * sin(2 * M * DEG2RAD)) + 282.634;
        L = fmod(L, 360.0);
        if (L < 0) L += 360.0;

        double RA = RAD2DEG * atan(0.91764 * tan(L * DEG2RAD));
        RA = fmod(RA, 360.0);
        if (RA < 0) RA += 360.0;

        // RA muss im selben 90-Grad-Quadranten wie L liegen (atan() liefert
        // nur Werte im Bereich -90..90 Grad, muss also entsprechend
        // "zurueckgeklappt" werden).
        double lQuadrant = floor(L / 90.0) * 90.0;
        double raQuadrant = floor(RA / 90.0) * 90.0;
        RA = RA + (lQuadrant - raQuadrant);
        RA = RA / 15.0; // Grad -> Stunden

        double sinDec = 0.39782 * sin(L * DEG2RAD);
        double cosDec = cos(asin(sinDec));

        constexpr double ZENITH = 90.833; // offizieller Zenit inkl. atm. Refraktion
        double cosH = (cos(ZENITH * DEG2RAD) - (sinDec * sin(lat * DEG2RAD))) / (cosDec * cos(lat * DEG2RAD));

        if (cosH > 1.0 || cosH < -1.0) return false; // Polartag/-nacht

        double H = sunrise ? (360.0 - RAD2DEG * acos(cosH)) : (RAD2DEG * acos(cosH));
        H = H / 15.0;

        double T = H + RA - (0.06571 * t) - 6.622;

        double UT = fmod(T - lngHour, 24.0);
        if (UT < 0) UT += 24.0;

        double local = fmod(UT + (utcOffsetSeconds / 3600.0), 24.0);
        if (local < 0) local += 24.0;

        outHour = (float)local;
        return true;
    }
}

Result compute(double lat, double lon, int year, int month, int day, int32_t utcOffsetSeconds) {
    Result r;
    if (lat == 0.0 && lon == 0.0) {
        r.valid = false;
        return r;
    }

    int N = dayOfYear(year, month, day);

    float sunrise = 0, sunset = 0;
    bool riseOk = calc(lat, lon, N, utcOffsetSeconds, true, sunrise);
    bool setOk = calc(lat, lon, N, utcOffsetSeconds, false, sunset);

    if (!riseOk || !setOk) {
        // Polartag (Sonne geht nicht unter) vs. Polarnacht (Sonne geht nicht
        // auf): grobe Naeherung ueber "hohe Sonne im Sommerhalbjahr der
        // jeweiligen Hemisphaere = Polartag, sonst Polarnacht" - reicht fuer
        // eine Nachtdimmung an den seltenen Orten/Tagen, an denen das
        // ueberhaupt vorkommt, voellig aus.
        bool northernSummerHalf = (N > 79 && N < 265); // grob Fruehlings-/Herbst-Tagundnachtgleiche
        bool polarDay = (lat > 0) == northernSummerHalf;
        r.valid = true;
        r.alwaysDay = polarDay;
        r.alwaysNight = !polarDay;
        return r;
    }

    r.valid = true;
    r.sunriseHour = sunrise;
    r.sunsetHour = sunset;
    return r;
}

}
