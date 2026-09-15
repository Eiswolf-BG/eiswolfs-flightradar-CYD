#include "watchlist_alert.h"
#include "aircraft_watchlist.h"
#include "squawk_watchlist.h"
#include "type_watchlist.h"

namespace WatchlistAlert {

bool isHit(const Aircraft& a) {
    return AircraftWatchlist::isWatched(a.callsign) ||
           SquawkWatchlist::isWatched(a.squawk) ||
           TypeWatchlist::isWatched(a.typeCode);
}

}
