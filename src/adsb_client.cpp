#include "adsb_client.h"
#include "radar_math.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

namespace AdsbClient {

namespace {
    const char* kNoValidate = nullptr;

    WiFiClientSecure persistentClient;
    bool clientConfigured = false;

    bool isJsonSpace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    // Speicherschonendes Parsen grosser Antworten (siehe Root-Cause-
    // Untersuchung/Design-Absprache im Chat): liest byte-weise vom Stream,
    // bis die Zeichenfolge keyPattern (z.B. "\"ac\":") vollstaendig
    // gelesen wurde, dann ueberspringt sie Leerzeichen bis zum
    // oeffnenden '[' des Arrays. Reines Byte-Scannen ohne JSON-
    // Verstaendnis - deshalb bewusst KEIN vollstaendiger Tokenizer.
    // maxScan verhindert eine Endlosschleife, falls das Muster (z.B. bei
    // einer unerwarteten Fehlerantwort) nie auftaucht.
    bool skipToArrayStart(Stream& stream, const char* keyPattern, size_t maxScan) {
        size_t patLen = strlen(keyPattern);
        size_t matched = 0;
        for (size_t i = 0; i < maxScan; i++) {
            int c = stream.read();
            if (c < 0) return false;
            if ((char)c == keyPattern[matched]) {
                matched++;
                if (matched == patLen) {
                    for (size_t j = 0; j < maxScan; j++) {
                        int c2 = stream.read();
                        if (c2 < 0) return false;
                        if ((char)c2 == '[') return true;
                        if (!isJsonSpace((char)c2)) return false;
                    }
                    return false;
                }
            } else {
                // Kein vollstaendiges KMP noetig - das Muster ("ac":) hat
                // keine problematische Selbstueberlappung, ein einfacher
                // Neustart bei Fehltreffer reicht.
                matched = ((char)c == keyPattern[0]) ? 1 : 0;
            }
        }
        return false;
    }

    // Liest das naechste Nicht-Leerzeichen-Byte vom Stream, OHNE es zu
    // verbrauchen falls es kein Leerzeichen ist (peek) - fuer die
    // Entscheidung "naechstes Flugzeug-Objekt oder Array-Ende", bevor
    // deserializeJson() fuer das naechste Objekt aufgerufen wird.
    int peekNextNonSpace(Stream& stream, size_t maxScan) {
        for (size_t i = 0; i < maxScan; i++) {
            int p = stream.peek();
            if (p < 0) return -1;
            if (!isJsonSpace((char)p)) return p;
            stream.read();
        }
        return -1;
    }

    // Liest UND verbraucht das naechste Nicht-Leerzeichen-Byte - fuer das
    // Trennzeichen zwischen zwei Array-Elementen (',' oder ']').
    int readNextNonSpace(Stream& stream, size_t maxScan) {
        for (size_t i = 0; i < maxScan; i++) {
            int c = stream.read();
            if (c < 0) return -1;
            if (!isJsonSpace((char)c)) return c;
        }
        return -1;
    }

    // "Naechste 40"-Auswahl (siehe Bugfix-Absprache im Chat): sobald die
    // Tabelle voll ist, muss bei jedem Ersetzen bekannt sein, welcher der
    // aktuell BELEGTEN Plaetze der am weitesten entfernte ist. Einfacher
    // linearer Scan - bei nur tableCapacity (40) Eintraegen voellig
    // unkritisch performant, bewusst nicht ueberoptimiert (siehe Absprache).
    // Wird nur aufgerufen, wenn ALLE tableCapacity Plaetze bereits mit
    // gueltigen Flugzeugen belegt sind.
    void findFarthest(const Aircraft* table, uint8_t tableCapacity,
                       uint8_t& farthestIdx, float& farthestDist) {
        farthestIdx = 0;
        farthestDist = table[0].distanceKm;
        for (uint8_t j = 1; j < tableCapacity; j++) {
            if (table[j].distanceKm > farthestDist) {
                farthestDist = table[j].distanceKm;
                farthestIdx = j;
            }
        }
    }
}

void primeTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    uint32_t start = millis();
    while (now < 8 * 3600 * 2 && millis() - start < 5000) {
        delay(100);
        now = time(nullptr);
    }
}

FetchResult fetch(double homeLat, double homeLon, float radiusKm,
                   Aircraft* table, uint8_t tableCapacity) {
    FetchResult result;

    if (WiFi.status() != WL_CONNECTED) {
        return result;
    }

    if (!clientConfigured) {
        persistentClient.setInsecure();
        persistentClient.setTimeout(Config::ADSB_HTTP_TIMEOUT_MS);
        clientConfigured = true;
    }

    HTTPClient http;
    char url[160];
    snprintf(url, sizeof(url),
             "https://%s/v2/lat/%.5f/lon/%.5f/dist/%.0f",
             Config::ADSB_API_HOST, homeLat, homeLon, radiusKm);

    http.setTimeout(Config::ADSB_HTTP_TIMEOUT_MS);
    if (!http.begin(persistentClient, url)) {
        return result;
    }
    // Kein Connection-Reuse mehr (siehe Design-Absprache): das neue
    // Stream-Parsing liest die Antwort nur bis zum Ende des "ac"-Arrays,
    // NICHT den kompletten Rest (msg/now/total/...) - eine wiederverwendete
    // Verbindung wuerde durch die undrainierten Rest-Bytes fuer den
    // naechsten Abruf korrumpiert. Ein frischer Handshake pro Abruf
    // (~900ms laut Messung, siehe Chat) passt komfortabel in
    // ADSB_HTTP_TIMEOUT_MS und den Fetch-Zyklus.
    http.setReuse(false);
    http.setUserAgent("EiswolfsFlightradarCYD/1.0 (+https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD)");
    http.addHeader("Accept", "application/json");
    // TESTWEISE - Backoff-Logik in net_task.cpp respektiert einen vom
    // Server mitgeschickten "Retry-After"-Header bei HTTP 429, statt nur
    // selbst zu schaetzen (siehe Absprache mit Karl). Muss VOR GET()
    // registriert werden, sonst liefert http.header() dafuer nichts.
    const char* collectedHeaders[] = {"Retry-After"};
    http.collectHeaders(collectedHeaders, 1);

    int code = http.GET();
    result.httpCode = code;

    if (code == 429) {
        String retryAfter = http.header("Retry-After");
        if (retryAfter.length()) {
            result.retryAfterSec = retryAfter.toInt();
        }
    }

    if (code != HTTP_CODE_OK) {
        http.end();
        return result;
    }
    // Speicherschonendes Streaming-Parsen (siehe CLAUDE.md, Abschnitt
    // "Bekannte Probleme" / Root-Cause-Untersuchung + Design-Absprache im
    // Chat): FRUEHER wurde die komplette gefilterte Antwort in EIN grosses
    // JsonDocument geparst - bei ~150 Flugzeugen lagen dadurch bis zu ~1800
    // Nodes UND ~900 einzelne String-Allokationen GLEICHZEITIG im Speicher,
    // bis das gesamte "ac"-Array fertig war. Das sprengte den auf diesem
    // Geraet dauerhaft auf ~43KB begrenzten groessten zusammenhaengenden
    // Heap-Block (live gemessen, siehe Chat) bei 100km praktisch immer
    // (IncompleteInput/NoMemory, deterministisch 0% Erfolg).
    //
    // Jetzt: JEDES Flugzeug-Objekt wird EINZELN vom Stream geparst (ein
    // deserializeJson()-Aufruf pro Objekt endet nachweislich exakt am
    // schliessenden '}' - siehe JsonDeserializer.hpp/Latch.hpp - und
    // hinterlaesst den Stream exakt an dieser Position fuer den naechsten
    // Aufruf), in ein einziges WIEDERVERWENDETES kleines JsonDocument -
    // Spitzenbedarf sinkt dadurch auf hoechstens EIN Flugzeug gleichzeitig
    // (~1KB statt ~35KB). Der Filter ist inhaltlich unveraendert (dieselben
    // 12 Felder), nur nicht mehr in "ac" verschachtelt, da jetzt jedes
    // Objekt einzeln als Root geparst wird.
    JsonDocument filter;
    filter["hex"]       = true;
    filter["flight"]    = true;
    filter["r"]         = true;
    filter["t"]         = true;
    filter["lat"]       = true;
    filter["lon"]       = true;
    filter["alt_baro"]  = true;
    filter["baro_rate"] = true;
    filter["gs"]        = true;
    filter["track"]     = true;
    filter["squawk"]    = true;
    filter["category"]  = true;

    Stream& stream = http.getStream();
    if (!skipToArrayStart(stream, "\"ac\":", 512)) {
        http.end();
        result.ok = false;
        return result;
    }

    // Schnappschuss von prevAirportDistKm (Best-Effort-Anflug-Erkennung,
    // siehe aircraft.h/aircraft_table.cpp::postFetchUpdate()) je Hex-Code,
    // BEVOR die Schleife unten beginnt, einzelne table[]-Eintraege per
    // "a = Aircraft{}" zurueckzusetzen. table[] ist tempTable aus
    // net_task.cpp, bleibt zwischen Aufrufen bestehen und enthaelt zu
    // Beginn dieses Aufrufs noch die Werte vom VORHERIGEN Abfragezyklus -
    // ohne diesen Schnappschuss wuerde "a = Aircraft{}" weiter unten
    // prevAirportDistKm bei JEDEM Zyklus auf den Default (-1) zuruecksetzen,
    // NOCH BEVOR aircraft_table.cpp::postFetchUpdate() (aufgerufen direkt
    // nach diesem fetch()) ueberhaupt pruefen kann, ob die Distanz zum
    // naechstgelegenen Flughafen sinkt - die Anflug-Erkennung wuerde dadurch
    // nie ausloesen (in einer Simulation mit stetig sinkender Distanz
    // bestaetigt: approachLikely blieb ueber alle Zyklen 0). Der Schnapp-
    // schuss wird bewusst VOR der Haupt-Schleife komplett erstellt statt
    // erst waehrend ihres Durchlaufs nachgeschlagen, weil sonst bereits in
    // diesem Zyklus ueberschriebene Slots fuer noch nicht bearbeitete
    // Hex-Codes falsche (schon geloeschte) Werte liefern wuerden.
    struct PrevAirportDist { char hex[7]; float dist; };
    PrevAirportDist prevAirportDistByHex[Config::MAX_TRACKED_AIRCRAFT];
    uint8_t prevAirportDistCount = 0;
    // Gleicher Schnappschuss-Bedarf wie bei prevAirportDistKm oben, nur fuer
    // den Naeherungs-/Entfernungs-Trend im Detail-Panel (aircraft.h::
    // DistanceTrend, aircraft_table.cpp::postFetchUpdate()). Wurde beim
    // urspruenglichen Hinzufuegen dieses Features vergessen - dadurch wurde
    // prevDistanceKm bei JEDEM Zyklus durch "a = Aircraft{}" unten auf den
    // Default (-1) zurueckgesetzt, NOCH BEVOR postFetchUpdate() eine
    // vorherige Distanz zum Vergleichen sehen konnte. Ergebnis: der Trend
    // blieb praktisch immer auf "Unknown" (leerer Text), unabhaengig davon,
    // wie lange ein Flugzeug schon verfolgt wurde (Alex' Bugmeldung).
    struct PrevDistance { char hex[7]; float dist; };
    PrevDistance prevDistanceByHex[Config::MAX_TRACKED_AIRCRAFT];
    uint8_t prevDistanceCount = 0;
    // Gleicher Schnappschuss-Bedarf wie oben, fuer die "First Seen"/"Seen
    // For"-Anzeige im Detail-Panel (aircraft.h::firstSeenMs) - rein
    // session-lokal, MUSS aber trotzdem ueber diesen Reset hinweg erhalten
    // bleiben, sonst wuerde jedes Flugzeug bei JEDEM Zyklus faelschlich als
    // "gerade erst neu gesehen" gelten (identischer Bug-Mechanismus wie der
    // urspruengliche prevDistanceKm-Fehler oben - deshalb von Anfang an
    // gleich mit demselben Muster umgesetzt statt es erst spaeter zu
    // entdecken).
    struct PrevFirstSeen { char hex[7]; uint32_t firstSeenMs; uint32_t firstSeenEpoch; };
    PrevFirstSeen prevFirstSeenByHex[Config::MAX_TRACKED_AIRCRAFT];
    uint8_t prevFirstSeenCount = 0;
    // Gleicher Schnappschuss-Bedarf wie oben, fuer den "Flugzeug-Steckbrief"
    // (aircraft.h::sessionMinDistanceKm/sessionMaxSpeedKt, siehe
    // aircraft_table.cpp::postFetchUpdate()/flight_logbook.cpp) - ohne
    // diesen Schnappschuss wuerden die bisherigen Extremwerte bei JEDEM
    // Zyklus verworfen, identischer Bug-Mechanismus wie bei prevDistanceKm
    // oben.
    struct PrevProfile { char hex[7]; float minDist; float maxSpeed; };
    PrevProfile prevProfileByHex[Config::MAX_TRACKED_AIRCRAFT];
    uint8_t prevProfileCount = 0;
    // Gleicher Schnappschuss-Bedarf wie oben, fuer die Wiederholungssperre
    // des "Flight Stories"-Features (aircraft.h::lastFlightStoryMs, siehe
    // dortiger Kommentar) - ohne diesen Schnappschuss wuerde die Sperre
    // durch "a = Aircraft{}" unten bei JEDEM Fetch-Zyklus (alle ~10s)
    // wirkungslos auf 0 zurueckgesetzt.
    struct PrevFlightStory { char hex[7]; uint32_t lastMs; };
    PrevFlightStory prevFlightStoryByHex[Config::MAX_TRACKED_AIRCRAFT];
    uint8_t prevFlightStoryCount = 0;
    for (uint8_t j = 0; j < tableCapacity && j < Config::MAX_TRACKED_AIRCRAFT; j++) {
        if (table[j].hex[0] != '\0') {
            strncpy(prevAirportDistByHex[prevAirportDistCount].hex, table[j].hex,
                    sizeof(prevAirportDistByHex[0].hex) - 1);
            prevAirportDistByHex[prevAirportDistCount].hex[sizeof(prevAirportDistByHex[0].hex) - 1] = 0;
            prevAirportDistByHex[prevAirportDistCount].dist = table[j].prevAirportDistKm;
            prevAirportDistCount++;

            strncpy(prevDistanceByHex[prevDistanceCount].hex, table[j].hex,
                    sizeof(prevDistanceByHex[0].hex) - 1);
            prevDistanceByHex[prevDistanceCount].hex[sizeof(prevDistanceByHex[0].hex) - 1] = 0;
            prevDistanceByHex[prevDistanceCount].dist = table[j].prevDistanceKm;
            prevDistanceCount++;

            strncpy(prevFirstSeenByHex[prevFirstSeenCount].hex, table[j].hex,
                    sizeof(prevFirstSeenByHex[0].hex) - 1);
            prevFirstSeenByHex[prevFirstSeenCount].hex[sizeof(prevFirstSeenByHex[0].hex) - 1] = 0;
            prevFirstSeenByHex[prevFirstSeenCount].firstSeenMs = table[j].firstSeenMs;
            prevFirstSeenByHex[prevFirstSeenCount].firstSeenEpoch = table[j].firstSeenEpoch;
            prevFirstSeenCount++;

            strncpy(prevProfileByHex[prevProfileCount].hex, table[j].hex,
                    sizeof(prevProfileByHex[0].hex) - 1);
            prevProfileByHex[prevProfileCount].hex[sizeof(prevProfileByHex[0].hex) - 1] = 0;
            prevProfileByHex[prevProfileCount].minDist = table[j].sessionMinDistanceKm;
            prevProfileByHex[prevProfileCount].maxSpeed = table[j].sessionMaxSpeedKt;
            prevProfileCount++;

            strncpy(prevFlightStoryByHex[prevFlightStoryCount].hex, table[j].hex,
                    sizeof(prevFlightStoryByHex[0].hex) - 1);
            prevFlightStoryByHex[prevFlightStoryCount].hex[sizeof(prevFlightStoryByHex[0].hex) - 1] = 0;
            prevFlightStoryByHex[prevFlightStoryCount].lastMs = table[j].lastFlightStoryMs;
            prevFlightStoryCount++;
        }
    }

    // Wiederverwendetes kleines JsonDocument fuer JE EIN Flugzeug-Objekt
    // (siehe Design-Absprache) - .clear() gibt dessen Pool vor jedem
    // Objekt vollstaendig frei, sodass nie mehr als ein Flugzeug
    // gleichzeitig im Speicher steht.
    JsonDocument aircraftDoc;
    uint8_t idx = 0;
    // "Naechste 40"-Auswahl (siehe Bugfix-Absprache im Chat - vorher wurden
    // einfach die ersten tableCapacity Flugzeuge in API-Antwort-Reihenfolge
    // uebernommen, adsb.lol liefert aber NICHT nach Entfernung sortiert).
    // Solange idx < tableCapacity, wird ganz normal angehaengt; sobald die
    // Tabelle voll ist, muessen wir wissen, welcher belegte Platz aktuell
    // am weitesten entfernt ist, um ihn ggf. durch ein naeheres Flugzeug zu
    // ersetzen - deshalb werden Index/Distanz erst dann per findFarthest()
    // bestimmt (nicht laufend waehrend des Auffuellens mitgefuehrt, das
    // waere unnoetig komplex fuer nur 40 Eintraege).
    uint8_t farthestIdx = 0;
    float farthestDist = 0.0f;
    for (;;) {
        int p = peekNextNonSpace(stream, 32);
        if (p < 0) {
            // Stream endete/Timeout, ohne dass das erwartete ']' kam -
            // gleiches Fehlerverhalten wie ein deserializeJson()-
            // Fehlschlag frueher (result.ok bleibt false).
            http.end();
            result.ok = false;
            return result;
        }
        if ((char)p == ']') {
            stream.read();
            break;
        }
        // WICHTIG: hier bewusst KEIN "if (idx >= tableCapacity) break;" mehr
        // (siehe Bugfix-Absprache) - der Stream muss vollstaendig gelesen
        // werden, auch wenn die Tabelle schon voll ist, damit ein spaeter in
        // der Antwort auftauchendes NAEHERES Flugzeug noch beruecksichtigt
        // werden kann. Jedes Objekt bleibt dabei einzeln/klein geparst
        // (aircraftDoc), der Speichervorteil des Streaming-Parsers bleibt
        // also erhalten.

        aircraftDoc.clear();
        DeserializationError objErr = deserializeJson(
            aircraftDoc, stream, DeserializationOption::Filter(filter));
        if (objErr) {
            http.end();
            result.ok = false;
            return result;
        }
        JsonObject ac = aircraftDoc.as<JsonObject>();

        const char* hex = ac["hex"] | "";

        // Manche ADS-B-Aggregatoren (adsb.fi eingeschlossen) melden ein und
        // dasselbe Flugzeug in seltenen Faellen zweimal in derselben Antwort
        // (z.B. ueber unterschiedliche Empfangs-/MLAT-Pfade) - das zeigte
        // sich als Alex' Bugmeldung: nach einer Reichweitenaenderung kurz
        // zwei Punkte fuer dasselbe Flugzeug auf dem Radar, die sich erst
        // nach 1-2 Abfragen von selbst korrigierten (sobald die Quelle
        // ihrerseits wieder nur einen Eintrag lieferte). Hier bereits beim
        // Einlesen ausschliessen statt erst beim Zeichnen zu bemerken - ein
        // bereits uebernommener Hex-Code wird ignoriert, der erste Eintrag
        // (idx 0..idx-1) bleibt massgeblich.
        bool duplicate = false;
        if (hex[0] != '\0') {
            for (uint8_t j = 0; j < idx; j++) {
                if (strcmp(table[j].hex, hex) == 0) {
                    duplicate = true;
                    break;
                }
            }
        }

        if (!duplicate) {
            // In ein lokales Aircraft geparst, statt direkt in table[idx] -
            // die Distanz zum eigenen Standort (siehe unten) entscheidet
            // erst NACH dem Parsen, ob/wohin dieses Flugzeug in die Tabelle
            // kommt (normal anhaengen, einen weiter entfernten Eintrag
            // ersetzen, oder verwerfen).
            Aircraft a{};

            strncpy(a.hex, hex, sizeof(a.hex) - 1);

            // prevAirportDistKm aus dem oben erstellten Schnappschuss
            // wiederherstellen, falls dieses Flugzeug schon im vorherigen Zyklus
            // bekannt war (siehe Kommentar dort) - alle anderen Felder bleiben
            // bewusst beim Aircraft{}-Default, nur dieser eine Tracking-Wert
            // muss ueber den Reset hinweg erhalten bleiben.
            for (uint8_t j = 0; j < prevAirportDistCount; j++) {
                if (strcmp(prevAirportDistByHex[j].hex, hex) == 0) {
                    a.prevAirportDistKm = prevAirportDistByHex[j].dist;
                    break;
                }
            }
            // prevDistanceKm ebenso wiederherstellen (siehe Kommentar beim
            // Schnappschuss oben) - fuer den Naeherungs-/Entfernungs-Trend im
            // Detail-Panel.
            for (uint8_t j = 0; j < prevDistanceCount; j++) {
                if (strcmp(prevDistanceByHex[j].hex, hex) == 0) {
                    a.prevDistanceKm = prevDistanceByHex[j].dist;
                    break;
                }
            }
            // firstSeenMs/firstSeenEpoch ebenso wiederherstellen (siehe
            // Kommentar beim Schnappschuss oben) - falls nicht gefunden,
            // bleiben beide beim Aircraft{}-Default 0 und werden gleich unten
            // als "gerade jetzt zum ersten Mal in dieser Sitzung gesehen"
            // gesetzt.
            for (uint8_t j = 0; j < prevFirstSeenCount; j++) {
                if (strcmp(prevFirstSeenByHex[j].hex, hex) == 0) {
                    a.firstSeenMs = prevFirstSeenByHex[j].firstSeenMs;
                    a.firstSeenEpoch = prevFirstSeenByHex[j].firstSeenEpoch;
                    break;
                }
            }
            // sessionMinDistanceKm/sessionMaxSpeedKt ebenso wiederherstellen
            // (siehe Kommentar beim Schnappschuss oben) - fuer den
            // "Flugzeug-Steckbrief" im Flugbuch. Nicht gefunden = neues
            // Flugzeug, bleibt beim Aircraft{}-Default (-1), wird gleich im
            // ersten postFetchUpdate()-Durchlauf gesetzt.
            for (uint8_t j = 0; j < prevProfileCount; j++) {
                if (strcmp(prevProfileByHex[j].hex, hex) == 0) {
                    a.sessionMinDistanceKm = prevProfileByHex[j].minDist;
                    a.sessionMaxSpeedKt = prevProfileByHex[j].maxSpeed;
                    break;
                }
            }
            // lastFlightStoryMs ebenso wiederherstellen (siehe Kommentar
            // beim Schnappschuss oben) - fuer die "Flight Stories"-
            // Wiederholungssperre. Nicht gefunden = neues Flugzeug, bleibt
            // beim Aircraft{}-Default 0 (noch nie eine Meldung verschickt).
            for (uint8_t j = 0; j < prevFlightStoryCount; j++) {
                if (strcmp(prevFlightStoryByHex[j].hex, hex) == 0) {
                    a.lastFlightStoryMs = prevFlightStoryByHex[j].lastMs;
                    break;
                }
            }
            if (a.firstSeenMs == 0) {
                a.firstSeenMs = millis();
                // Echte Wanduhrzeit NUR erfassen, wenn sie GENAU JETZT (beim
                // tatsaechlichen Erstsichten) schon NTP-synchronisiert ist -
                // sonst bleibt firstSeenEpoch bewusst 0 (siehe Kommentar bei
                // Aircraft::firstSeenEpoch, kein nachtraegliches "Aufholen" mit
                // einer dann nicht mehr zutreffenden Uhrzeit).
                time_t nowEpoch = time(nullptr);
                if (nowEpoch > 8 * 3600 * 2) {
                    a.firstSeenEpoch = (uint32_t)nowEpoch;
                }
            }

            const char* flight = ac["flight"] | "";
            strncpy(a.callsign, flight, sizeof(a.callsign) - 1);

            const char* reg = ac["r"] | "";
            strncpy(a.reg, reg, sizeof(a.reg) - 1);

            const char* type = ac["t"] | "";
            strncpy(a.typeCode, type, sizeof(a.typeCode) - 1);

            a.lat = ac["lat"] | 0.0f;
            a.lon = ac["lon"] | 0.0f;

            if (ac["alt_baro"].is<const char*>()) {
                a.altBaroFt = 0;
            } else {
                a.altBaroFt = ac["alt_baro"] | 0;
            }

            a.vertRateFtMin = ac["baro_rate"] | 0;
            a.groundSpeedKt = ac["gs"] | 0.0f;
            a.headingDeg    = ac["track"] | 0.0f;

            const char* squawk = ac["squawk"] | "";
            strncpy(a.squawk, squawk, sizeof(a.squawk) - 1);

            const char* category = ac["category"] | "";
            strncpy(a.category, category, sizeof(a.category) - 1);

            a.lastSeenMs = millis();
            a.valid = (a.lat != 0.0f || a.lon != 0.0f);

            if (a.valid) {
                // Distanz zum eigenen Standort VORGEZOGEN (wird von
                // postFetchUpdate() spaeter ohnehin nochmal identisch
                // berechnet und dort ueberschrieben - hier aber schon
                // benoetigt, um zu entscheiden, ob dieses Flugzeug einen der
                // 40 Tabellenplaetze verdient).
                a.distanceKm = RadarMath::toPolar(homeLat, homeLon, a.lat, a.lon).distanceKm;

                if (idx < tableCapacity) {
                    table[idx] = a;
                    idx++;
                    if (idx == tableCapacity) {
                        // Tabelle jetzt zum ersten Mal voll - weitesten
                        // Eintrag einmalig bestimmen, ab jetzt braucht es
                        // ihn fuer jede weitere Ersetzungs-Entscheidung.
                        findFarthest(table, tableCapacity, farthestIdx, farthestDist);
                    }
                } else if (a.distanceKm < farthestDist) {
                    // Tabelle voll, aber dieses Flugzeug ist naeher als der
                    // aktuell weiteste Eintrag - ersetzen, dann den neuen
                    // weitesten Eintrag bestimmen (linearer Scan ueber nur
                    // tableCapacity Eintraege, siehe findFarthest()).
                    table[farthestIdx] = a;
                    findFarthest(table, tableCapacity, farthestIdx, farthestDist);
                }
                // sonst: Tabelle voll UND nicht naeher als der weiteste
                // Eintrag - dieses Flugzeug wird verworfen, table[]
                // unveraendert.
            }
        }

        int sep = readNextNonSpace(stream, 32);
        if (sep == ',') continue;
        if (sep == ']') break;
        // Weder ',' noch ']' - Antwort nicht wohlgeformt.
        http.end();
        result.ok = false;
        return result;
    }

    http.end();
    result.ok = true;
    result.aircraftCount = idx;
    return result;
}

}