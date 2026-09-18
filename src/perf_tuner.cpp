#include "perf_tuner.h"
#include "settings_store.h"
#include <Arduino.h>

namespace PerfTuner {

namespace {
    // Geglaetteter Mittelwert der tick()-Frame-Zeit (siehe recordFrameMs()) -
    // einfacher IIR-Tiefpass statt eines echten Ring-Puffers, genau wie beim
    // Follow-Me-Kurs in radar_screen.cpp - dieselbe Begruendung: kein neues
    // Array noetig, ein einzelner float genuegt.
    float emaFrameMs = 80.0f;
    constexpr float FRAME_MS_SMOOTHING_ALPHA = 0.15f;

    uint8_t level = 0;
    uint32_t lastEvalMs = 0;
    constexpr uint32_t EVAL_INTERVAL_MS = 2000;

    // Schwellwerte je Stufenuebergang - bewusst grobe, plausible Werte
    // (Alex' Vorgabe: "muss nicht perfekt sein"), kein Anspruch auf exakte
    // Kalibrierung. Je "escalate"-Wert wird bei UEBER-/UNTERschreiten (Zeit
    // hoeher, Speicher niedriger) eine Stufe HOCH-, der "recover"-Wert mit
    // spuerbarem Abstand dazu eine Stufe RUNTERgeschaltet - dieser Abstand
    // IST die geforderte Hysterese, verhindert beim staendigen Umherpendeln
    // um einen einzigen Schwellwert ein staendiges Hin-und-Her.
    struct LevelThresholds {
        float escalateFrameMs;
        uint32_t escalateFreeHeap;
        uint32_t escalateMaxAlloc;
        float recoverFrameMs;
        uint32_t recoverFreeHeap;
        uint32_t recoverMaxAlloc;
    };
    // Index 0 = Uebergang Stufe 0<->1, Index 1 = Stufe 1<->2, Index 2 = Stufe 2<->3.
    constexpr LevelThresholds LEVEL_THRESHOLDS[3] = {
        {200.0f, 40000, 20000,   140.0f, 55000, 30000},
        {350.0f, 25000, 12000,   250.0f, 35000, 18000},
        {600.0f, 15000, 8000,    450.0f, 22000, 11000},
    };
}

void recordFrameMs(uint32_t ms) {
    emaFrameMs += ((float)ms - emaFrameMs) * FRAME_MS_SMOOTHING_ALPHA;
}

void update() {
    if (!SettingsStore::perfAutoTuningEnabled()) {
        level = 0;
        return;
    }

    uint32_t now = millis();
    if (now - lastEvalMs < EVAL_INTERVAL_MS) return;
    lastEvalMs = now;

    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxAlloc = ESP.getMaxAllocHeap();

    // Pro Auswertung hoechstens EIN Stufenwechsel (hoch ODER runter) - siehe
    // Doku oben, sorgt fuer ein graduelles statt sprunghaftes Verhalten.
    if (level < 3) {
        const LevelThresholds& t = LEVEL_THRESHOLDS[level];
        bool overloaded = emaFrameMs > t.escalateFrameMs ||
                           freeHeap < t.escalateFreeHeap ||
                           maxAlloc < t.escalateMaxAlloc;
        if (overloaded) {
            level++;
            return;
        }
    }
    if (level > 0) {
        const LevelThresholds& t = LEVEL_THRESHOLDS[level - 1];
        bool recovered = emaFrameMs < t.recoverFrameMs &&
                          freeHeap > t.recoverFreeHeap &&
                          maxAlloc > t.recoverMaxAlloc;
        if (recovered) level--;
    }
}

uint8_t currentLevel() { return level; }

bool weatherEffectsSuppressed() { return level >= 1; }
bool sweepSimplified() { return level >= 2; }
bool silhouetteDetailReduced() { return level >= 3; }

float lastAvgFrameMs() { return emaFrameMs; }

}
