#include "auto_range.h"
#include "config.h"
#include "settings_store.h"
#include <atomic>

namespace AutoRange {

namespace {
    // Nur die ersten drei Config::RANGE_STEPS_KM-Stufen (10/25/50km) sind
    // im Auto-Modus erreichbar - siehe auto_range.h.
    std::atomic<uint8_t> currentTier{0};
    std::atomic<uint32_t> lastSwitchMs{0};

    // Nur von Core 0 (onFetchSuccess()) angefasst - kein Atomic noetig, im
    // Gegensatz zu currentTier/lastSwitchMs oben, die auch von Core 1
    // (effectiveIndex()/currentIndex()) gelesen werden.
    uint8_t consecutiveAbove = 0;
    uint8_t consecutiveBelow = 0;

    constexpr uint8_t HYSTERESIS_CYCLES = 3;
    constexpr uint32_t MIN_SWITCH_INTERVAL_MS = 120000; // 2 Minuten

    // Schwellenwerte fuer die Ziel-Stufe je nach aktueller Flugzeuganzahl -
    // kalibriert an Alex' realen Live-Messungen (10km=1, 25km=8, 50km=23
    // Flugzeuge, siehe Aufgabe/Analyse). Grob konsistent mit einer
    // flaechenproportionalen Verkehrsdichte (25km/10km-Flaechenverhaeltnis
    // 6,25x vs. gemessenes Verhaeltnis 8x; 50km/25km-Flaechenverhaeltnis 4x
    // vs. gemessenes Verhaeltnis ~2,9x - angesichts nur einer Messreihe
    // erwartungsgemaess nicht exakt, aber in der richtigen Groessenordnung).
    uint8_t targetTierFor(uint8_t count) {
        if (count <= 3) return 0;  // 10km
        if (count <= 12) return 1; // 25km
        return 2;                  // 50km
    }
}

void reset() {
    currentTier.store(0, std::memory_order_relaxed);
    lastSwitchMs.store(0, std::memory_order_relaxed);
    consecutiveAbove = 0;
    consecutiveBelow = 0;
}

uint8_t currentIndex() {
    return currentTier.load(std::memory_order_relaxed);
}

uint8_t effectiveIndex() {
    return SettingsStore::autoRangeEnabled() ? currentIndex() : SettingsStore::rangeIndex();
}

void onFetchSuccess(uint8_t aircraftCount, uint32_t nowMs) {
    uint8_t tier = currentTier.load(std::memory_order_relaxed);
    uint8_t target = targetTierFor(aircraftCount);

    if (target == tier) {
        consecutiveAbove = 0;
        consecutiveBelow = 0;
        return;
    }

    // Hysterese: ein einzelner Ausreisser-Zyklus (z.B. kurzzeitig ein
    // zusaetzliches Flugzeug genau an der Schwelle) darf noch keinen
    // Wechsel ausloesen - erst wenn HYSTERESIS_CYCLES aufeinanderfolgende
    // Fetch-Zyklen konsistent in dieselbe Richtung zeigen.
    if (target > tier) {
        consecutiveAbove++;
        consecutiveBelow = 0;
    } else {
        consecutiveBelow++;
        consecutiveAbove = 0;
    }

    bool stable = (target > tier ? consecutiveAbove : consecutiveBelow) >= HYSTERESIS_CYCLES;
    if (!stable) return;

    // Mindestabstand zwischen zwei ECHTEN Wechseln - verhindert, dass Auto-
    // Range bei schwankender Flugzeuganzahl nahe einer Schwelle wiederholt
    // in kurzer Folge auf einen groesseren (teureren, siehe CLAUDE.md
    // IncompleteInput-Risiko) Radius wechselt.
    uint32_t last = lastSwitchMs.load(std::memory_order_relaxed);
    if (last != 0 && nowMs - last < MIN_SWITCH_INTERVAL_MS) return;

    currentTier.store(target, std::memory_order_relaxed);
    lastSwitchMs.store(nowMs, std::memory_order_relaxed);
    consecutiveAbove = 0;
    consecutiveBelow = 0;
}

}
