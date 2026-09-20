#pragma once
#include "aircraft.h"

// Gemeinsamer Treffer-Check fuer alle VIER nutzerdefinierten Wachlisten
// (Rufzeichen/aircraft_watchlist.h, Squawk/squawk_watchlist.h, Flugzeugtyp/
// type_watchlist.h, Route/route_watchlist.h) - ein Treffer auf IRGENDEINER
// der vier loest denselben Alarm aus (LED WatchlistBlue, optionaler
// SPK-Einzelton, Browser-Watchlist-Sound/-Badge im Web-Export, ntfy.sh-
// Push), siehe radar_screen.cpp::updateProximityAlert() und
// web_export_server.cpp. Zentral hier gebuendelt (Alex' Wunsch), damit alle
// Aufrufstellen synchron bleiben und die "||"-Verknuepfung nicht an
// mehreren Stellen im Projekt separat gepflegt werden muss.
namespace WatchlistAlert {
    bool isHit(const Aircraft& a);
}
