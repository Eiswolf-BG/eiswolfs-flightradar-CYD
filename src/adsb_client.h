#pragma once
#include <Arduino.h>
#include "aircraft.h"
#include "config.h"

namespace AdsbClient {

    struct FetchResult {
        bool     ok = false;
        uint16_t aircraftCount = 0;
        int      httpCode = 0;
        // Nur gesetzt (>=0), wenn httpCode==429 UND der Server einen
        // "Retry-After"-Header mitgeschickt hat (siehe net_task.cpp fuer
        // die Backoff-Logik, die diesen Wert bevorzugt gegenueber der
        // eigenen Schaetzung verwendet).
        int      retryAfterSec = -1;
    };
    FetchResult fetch(double homeLat, double homeLon, float radiusKm,
                       Aircraft* table, uint8_t tableCapacity);
    void primeTime();

}