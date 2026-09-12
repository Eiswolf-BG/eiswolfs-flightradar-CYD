#pragma once
#include <Arduino.h>

namespace LocationManager {

    enum class Source { GpsFix, IpGeolocation, Manual, Persisted, None };

    void init();
    void update();
    void requestIpLookupIfNeeded();
    void getHomeLocation(double& lat, double& lon);

    Source currentSource();
    void setManualLocation(double lat, double lon);
    void setGpsEnabled(bool enabled);
    bool isGpsEnabled();

    void cycleGpsPinPair();
    const char* currentGpsPinLabel();

    bool hasGpsFix();

    // Rohe GPS-Position/-Hoehe fuer die optionale Radar-Eckanzeige
    // (radar_screen.cpp) - bewusst getrennt von getHomeLocation() oben, das
    // je nach aktivem Standort-Preset/IP-Geolocation/Persistenz eine ganz
    // andere Quelle liefern kann. currentGpsPosition() liefert false, wenn
    // (noch) kein GPS-Fix vorliegt - siehe hasGpsFix() oben, dieselbe
    // Bedingung. Hoehe separat, da TinyGPSPlus dafuer ein eigenes
    // gueltig/ungueltig-Flag fuehrt (manche Empfaenger liefern kurz nach
    // dem ersten Fix schon eine gueltige Position, aber noch keine
    // Hoehenangabe).
    bool currentGpsPosition(double& lat, double& lon);
    bool hasGpsAltitude();
    double gpsAltitudeMeters();

    // UTC-Offset in Sekunden (inkl. evtl. Sommerzeit), ermittelt bei der
    // IP-Geolocation-Abfrage. 0/false, falls noch nicht bekannt.
    bool hasUtcOffset();
    int32_t utcOffsetSeconds();

    // Ob die Region (per IP-Geolocation-Laendercode) metrische Einheiten
    // nutzt (Meter/km) statt Fuss/Meilen. Default true (metrisch), bis die
    // IP-Abfrage etwas anderes ermittelt hat - nur die USA nutzen aktuell
    // eine Ausnahme.
    bool useMetricUnits();
}