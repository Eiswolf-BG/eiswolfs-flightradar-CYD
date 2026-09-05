#pragma once
#include <cstdint>

// Waehlt im "Auto"-Reichweitenmodus (5. Schritt im Reichweiten-Button-
// Zyklus 10->25->50->100->Auto->10->..., siehe SettingsStore::
// autoRangeEnabled() und radar_screen.cpp::handleTap()) automatisch
// zwischen den ersten drei Config::RANGE_STEPS_KM-Stufen (10/25/50km),
// abhaengig von der aktuellen Flugzeuganzahl. 100km bleibt bei Auto
// bewusst IMMER ausgeschlossen (siehe CLAUDE.md, "Bekannte Probleme" -
// IncompleteInput-/Heap-Risiko bei grossen Abfrage-Radien) - nur ueber
// den manuellen Zyklus-Schritt erreichbar.
namespace AutoRange {
    // Muss nach JEDEM erfolgreichen ADS-B-Fetch aufgerufen werden (Core 0,
    // net_task.cpp), mit der Anzahl gueltiger Flugzeuge bei der GERADE
    // aktiv abgefragten effektiven Reichweite. Wendet Hysterese (mehrere
    // aufeinanderfolgende Fetch-Zyklen ueber/unter der Schwelle) UND einen
    // Mindestabstand zwischen zwei tatsaechlichen Wechseln an, bevor die
    // gewaehlte Stufe wirklich springt - siehe auto_range.cpp.
    void onFetchSuccess(uint8_t aircraftCount, uint32_t nowMs);

    // 0-2 (10/25/50km, Index in Config::RANGE_STEPS_KM) - aktuell von
    // Auto-Range gewaehlte Stufe. Nur inhaltlich relevant, wenn
    // SettingsStore::autoRangeEnabled() true ist - siehe effectiveIndex()
    // fuer den ueblichen, bereits auto/manuell unterscheidenden Zugriff.
    // Thread-safe (Core 0 schreibt in onFetchSuccess(), Core 1 liest ueber
    // effectiveIndex()/currentIndex()).
    uint8_t currentIndex();

    // Bequemer Standard-Zugriffspunkt fuer JEDEN Aufrufer, der "die gerade
    // tatsaechlich in Kraft befindliche Reichweiten-Stufe" braucht (Fetch-
    // Radius, Zeichnen, Naeherungsalarm, Web-UI-Default) - kapselt die
    // Fallunterscheidung Auto/manuell, damit das nicht an jeder Aufrufstelle
    // wiederholt werden muss. Liefert SettingsStore::rangeIndex() bei
    // deaktiviertem Auto, sonst currentIndex().
    uint8_t effectiveIndex();

    // Setzt den internen Zustand zurueck (Hysterese-Zaehler, letzter
    // Wechsel-Zeitstempel, aktuell gewaehlte Stufe auf 10km) - beim
    // (Wieder-)Aktivieren von Auto ueber den Reichweiten-Button aufgerufen,
    // damit kein alter Zustand von vor dem letzten Ausschalten nachwirkt.
    void reset();
}
