#pragma once
#include <cstdint>

// "Performance-Auto-Tuning" (Alex' Wunsch) - reduziert automatisch und
// stufenweise die Render-Last, wenn Frame-Zeit/freeHeap/maxAlloc auf eine
// Ueberlastung hindeuten (z.B. viele gleichzeitig sichtbare Flugzeuge oder
// zunehmende Heap-Fragmentierung nach Stunden Laufzeit), und normalisiert
// sich mit etwas Hysterese automatisch wieder. Bewusst extrem leichtgewichtig
// (nur ein paar Namensraum-Member, keine neuen Arrays/Tabellen) - Flash lag
// bei Auftragsstart schon bei 93,3%.
namespace PerfTuner {

    // Von main.cpp bei JEDEM RadarScreen::tick()-Aufruf mit dem tatsaechlich
    // vergangenen deltaMs gefuettert (bereits vorhanden, keine neue Messung
    // noetig) - gleitender Mittelwert dient als einfacher "Framerate"-Proxy,
    // ohne eine eigene FPS-Zaehl-Infrastruktur aufzubauen.
    void recordFrameMs(uint32_t ms);

    // Wertet freeHeap/maxAlloc (ESP.getFreeHeap()/ESP.getMaxAllocHeap(),
    // dieselben Werte wie im System-Status-Screen) zusammen mit der
    // geglaetteten Frame-Zeit aus und passt ggf. die aktuelle Stufe an -
    // intern selbst auf ca. alle 2s gedrosselt, darf also beliebig oft
    // (z.B. bei jedem tick()) aufgerufen werden.
    void update();

    // 0 = normal, 1-3 = zunehmend reduziert (siehe Stufen-Doku in perf_tuner.cpp).
    uint8_t currentLevel();

    bool weatherEffectsSuppressed(); // Stufe >= 1
    bool sweepSimplified();          // Stufe >= 2
    bool silhouetteDetailReduced();  // Stufe >= 3

    // Fuer den System-Status-Screen (Alex' Wunsch: unauffaelliger
    // Hinweis/Wert dort statt eines eigenen Panels).
    float lastAvgFrameMs();
}
