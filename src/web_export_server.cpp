#include "web_export_server.h"
#include "config.h"
#include "flight_logbook.h"
#include "sd_mutex.h"
#include "airline_filter.h"
#include "aircraft_watchlist.h"
#include "aircraft_table.h"
#include "settings_store.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <cstring>
#include <cmath>

namespace WebExportServer {

namespace {
    constexpr uint8_t MAX_DAYS_QUERIED = 31;

    WebServer server(80);

    // Nur reine Dateinamen/Labels aus Formularfeldern akzeptieren - kein
    // "/" und kein ".." - damit ueber die WebUI kein Ausbruch aus dem
    // jeweiligen SD-Verzeichnis moeglich ist (Pfad-Traversal).
    bool isSafeName(const String& name) {
        if (name.length() == 0 || name.length() > 40) return false;
        if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) return false;
        if (name.indexOf("..") >= 0) return false;
        return true;
    }

    // Absichtliche Duplikate der gleichnamigen (lokalen/statischen) Funktionen
    // aus radar_screen.cpp - dort nicht exportiert, und nach der im Projekt
    // etablierten Konvention "jeder Screen/jedes Modul dupliziert seine
    // eigenen kleinen Helfer statt eines gemeinsamen Moduls" (siehe z.B.
    // timeout_screen.cpp) bewusst hier erneut definiert statt radar_screen.cpp
    // umzubauen. Bei Aenderungen an der Logik in radar_screen.cpp bitte diese
    // Kopie hier synchron halten, damit das WebUI-Radar dieselben Ringe/
    // Markierungen zeigt wie das Geraete-Display.
    bool isEmergencySquawkWeb(const char* squawk) {
        if (!squawk[0]) return false;
        for (uint8_t i = 0; i < Config::EMERGENCY_SQUAWK_COUNT; i++) {
            if (strcmp(squawk, Config::EMERGENCY_SQUAWKS[i]) == 0) return true;
        }
        return false;
    }

    // "Auffaellig" (oranger Ring) beschraenkt sich aktuell auf gar nichts -
    // der Teil ueber Militaer-/Regierungs-Rufzeichen-Praefixe
    // (Config::NOTABLE_CALLSIGN_PREFIXES) ist NICHT umgesetzt, weil diese
    // Konstante nirgends im Projekt existiert (auch nicht in
    // radar_screen.cpp) - das zugehoerige Feature wurde bisher nie
    // tatsaechlich spezifiziert/implementiert. "notable" wird deshalb unten
    // in handleRadarJson() fest auf false gesetzt. Sobald es eine echte
    // Praefixliste gibt, hier eine isNotableCallsignWeb()-Funktion analog zu
    // isEmergencySquawkWeb() ergaenzen.
    bool isHeavyCategoryWeb(const char* category) {
        return category[0] == 'A' && category[1] == '5';
    }

    String htmlHeader(const String& title) {
        String html;
        html += "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
        html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
        html += "<title>" + title + "</title>";
        html += "<style>";
        html += "body{background-color:#0a0f0d;color:#39ff14;font-family:'Courier New',Courier,monospace;padding:20px;}";
        html += "h1{font-size:20px;}h2{font-size:16px;margin-top:24px;border-top:1px solid #1f3a2b;padding-top:12px;}";
        html += "table{border-collapse:collapse;margin-top:10px;width:100%;}";
        html += "td,th{padding:4px 12px;text-align:left;border-bottom:1px solid #1f3a2b;}";
        html += "a{color:#39ff14;}";
        html += "form{display:inline;}";
        html += "button{background:#0a0f0d;color:#ff3b3b;border:1px solid #ff3b3b;border-radius:4px;padding:3px 10px;font-family:inherit;cursor:pointer;}";
        html += "button:hover{background:#ff3b3b;color:#0a0f0d;}";
        html += ".dl{color:#39ff14;text-decoration:none;border:1px solid #39ff14;border-radius:4px;padding:3px 10px;margin-right:6px;display:inline-block;}";
        // Gruen statt Rot fuer "Hinzufuegen"-Buttons - die roten button{}-Regeln
        // oben bleiben fuer alle destruktiven "Entfernen/Loeschen"-Buttons
        // unveraendert, .addbtn ist ausschliesslich fuer die neuen Listen-
        // Formulare (siehe handleLists()) gedacht.
        html += ".addbtn{background:#0a0f0d;color:#39ff14;border:1px solid #39ff14;border-radius:4px;padding:3px 10px;font-family:inherit;cursor:pointer;}";
        html += ".addbtn:hover{background:#39ff14;color:#0a0f0d;}";
        html += "input[type=text]{background:#0a0f0d;color:#39ff14;border:1px solid #39ff14;border-radius:4px;padding:5px 8px;font-family:inherit;}";
        html += "nav{margin-bottom:10px;}nav a{color:#39ff14;margin-right:16px;}";
        html += "#radarCanvas{width:100%;max-width:400px;height:auto;background:#05100a;border:1px solid #1f3a2b;border-radius:8px;display:block;cursor:pointer;}";
        html += "#radarStatus{font-size:12px;color:#7a9a86;margin-top:4px;}";
        html += "#radarControls{font-size:12px;margin-bottom:8px;}";
        html += "#radarControls select{background:#0a0f0d;color:#39ff14;border:1px solid #39ff14;border-radius:4px;padding:2px 6px;font-family:inherit;}";
        html += "#acInfo{display:none;max-width:400px;margin-top:8px;padding:8px 10px;border:1px solid #39ff14;border-radius:6px;font-size:13px;line-height:1.7;}";
        html += "#acInfo a{color:#ff3b3b;text-decoration:none;border:1px solid #ff3b3b;border-radius:4px;padding:2px 8px;display:inline-block;margin-top:4px;}";
        html += "</style></head><body>";
        html += "<h1>" + title + "</h1>";
        return html;
    }

    // Live-Radar-Ansicht fuer die Startseite: ein <canvas>, das per JavaScript
    // alle paar Sekunden /radar.json abruft und die Flugzeuge polar (Peilung/
    // Distanz, genau wie auf dem Geraete-Display) zeichnet. Bewusst per
    // fetch()-Polling statt WebSocket/SSE gehalten - deutlich weniger Code
    // und Speicherbedarf auf dem ESP32, und fuer eine gelegentlich vom Handy
    // aus aufgerufene Seite voellig ausreichend.
    //
    // Der Reichweiten-Waehler (<select>) ist rein clientseitig/pro Seiten-
    // aufruf - er aendert NICHT die Geraete-Einstellung (SettingsStore::
    // rangeIndex()), sondern wird als "range_km"-Query-Parameter an
    // /radar.json mitgeschickt (siehe handleRadarJson()) und erlaubt so ein
    // unabhaengiges Herein-/Herauszoomen auf dem Handy, ohne das Geraete-
    // Display zu beeinflussen. Default ist die aktuelle Geraete-Reichweite.
    void appendRadarSection(String& html) {
        float deviceRangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];

        html += "<h2>Live Radar</h2>";
        html += "<div id=\"radarControls\">Range: <select id=\"radarRange\">";
        for (uint8_t i = 0; i < Config::RANGE_STEP_COUNT; i++) {
            bool isDefault = fabsf(Config::RANGE_STEPS_KM[i] - deviceRangeKm) < 0.5f;
            html += "<option value=\"" + String(Config::RANGE_STEPS_KM[i], 0) + "\"";
            if (isDefault) html += " selected";
            html += ">" + String(Config::RANGE_STEPS_KM[i], 0) + " km</option>";
        }
        html += "</select></div>";
        html += "<canvas id=\"radarCanvas\" width=\"360\" height=\"360\"></canvas>";
        html += "<p id=\"radarStatus\">Loading...</p>";
        html += "<div id=\"acInfo\"></div>";
        html += "<script>(function(){";
        html += "var canvas=document.getElementById('radarCanvas');";
        html += "var ctx=canvas.getContext('2d');";
        html += "var status=document.getElementById('radarStatus');";
        html += "var infoBox=document.getElementById('acInfo');";
        html += "var rangeSel=document.getElementById('radarRange');";
        html += "var W=canvas.width,H=canvas.height,cx=W/2,cy=H/2,R=Math.min(W,H)/2-24;";
        html += "var lastData={range_km:" + String(deviceRangeKm, 0) + ",aircraft:[]};";
        html += "var markers=[];";
        html += "var selectedHex=null;";

        // Hintergrund-Sterne AUSSERHALB des Radarkreises - gleiches Prinzip
        // wie updateBgStars()/initBgStarsIfNeeded() in radar_screen.cpp
        // (Rejection-Sampling, damit kein Stern innerhalb des Kreises
        // landet), hier per Canvas/JS statt TFT_eSPI nachgebaut.
        html += "var stars=[];";
        html += "(function(){var minDistSq=(R+6)*(R+6);for(var i=0;i<24;i++){var x,y,tries=0;";
        html += "do{x=4+Math.random()*(W-8);y=4+Math.random()*(H-8);tries++;}";
        html += "while(((x-cx)*(x-cx)+(y-cy)*(y-cy))<minDistSq&&tries<25);";
        html += "stars.push({x:x,y:y,phase:Math.random()*255,speed:1+Math.random()*2});}})();";
        html += "function drawStars(){for(var i=0;i<stars.length;i++){var s=stars[i];";
        html += "s.phase=(s.phase+s.speed)%256;";
        html += "var bright=Math.round(s.phase<128?s.phase*2:(255-s.phase)*2);";
        html += "ctx.fillStyle='rgb(0,'+bright+',0)';ctx.fillRect(s.x,s.y,1,1);}}";

        html += "function altColor(ft){if(ft<3000)return '#ff4d4d';if(ft<10000)return '#ffb84d';if(ft<25000)return '#ffe14d';if(ft<35000)return '#39ff14';return '#4dd2ff';}";

        html += "function draw(data){";
        html += "lastData=data;";
        html += "ctx.clearRect(0,0,W,H);";
        html += "drawStars();";
        html += "ctx.strokeStyle='#1f3a2b';ctx.fillStyle='#39ff14';ctx.font='10px monospace';ctx.textAlign='left';";
        html += "for(var ring=1;ring<=3;ring++){var r=R*ring/3;ctx.beginPath();ctx.arc(cx,cy,r,0,Math.PI*2);ctx.stroke();";
        html += "ctx.fillText(Math.round(data.range_km*ring/3)+' km',cx+4,cy-r+10);}";
        html += "ctx.strokeStyle='#12261a';ctx.beginPath();ctx.moveTo(cx-R,cy);ctx.lineTo(cx+R,cy);ctx.moveTo(cx,cy-R);ctx.lineTo(cx,cy+R);ctx.stroke();";
        // Alle vier Himmelsrichtungen (N/E/S/W), genau wie
        // drawStaticBackground() in radar_screen.cpp - vorher stand hier nur
        // "N", was auf Nachfrage ergaenzt wurde.
        html += "ctx.fillStyle='#39ff14';ctx.textAlign='center';ctx.fillText('N',cx,cy-R-8);";
        html += "ctx.fillText('S',cx,cy+R+16);";
        html += "ctx.textAlign='left';ctx.fillText('E',cx+R+4,cy+3);";
        html += "ctx.textAlign='right';ctx.fillText('W',cx-R-4,cy+3);";
        html += "ctx.textAlign='center';";
        html += "ctx.fillStyle='#ffffff';ctx.beginPath();ctx.arc(cx,cy,3,0,Math.PI*2);ctx.fill();";
        html += "markers=[];";
        html += "(data.aircraft||[]).forEach(function(a){";
        html += "var theta=a.bearing_deg*Math.PI/180;";
        html += "var r=Math.min(a.dist_km/data.range_km,1)*R;";
        html += "var x=cx+r*Math.sin(theta),y=cy-r*Math.cos(theta);";
        html += "var color=a.ground_vehicle?'#aaaaaa':altColor(a.alt_ft);";
        html += "ctx.fillStyle=color;ctx.strokeStyle=color;";
        html += "if(a.ground_vehicle){ctx.fillRect(x-3,y-3,6,6);}";
        html += "else if(a.rotorcraft){ctx.beginPath();ctx.moveTo(x,y-5);ctx.lineTo(x+5,y);ctx.lineTo(x,y+5);ctx.lineTo(x-5,y);ctx.closePath();ctx.fill();}";
        html += "else if(a.heavy){ctx.beginPath();ctx.arc(x,y,5,0,Math.PI*2);ctx.fill();ctx.beginPath();ctx.arc(x,y,7,0,Math.PI*2);ctx.stroke();";
        html += "var hr2=a.track_deg*Math.PI/180;ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(x+10*Math.sin(hr2),y-10*Math.cos(hr2));ctx.stroke();}";
        html += "else{ctx.beginPath();ctx.arc(x,y,4,0,Math.PI*2);ctx.fill();";
        html += "var hr=a.track_deg*Math.PI/180;ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(x+10*Math.sin(hr),y-10*Math.cos(hr));ctx.stroke();}";
        html += "if(a.emergency){ctx.strokeStyle='#ff3b3b';ctx.beginPath();ctx.arc(x,y,9,0,Math.PI*2);ctx.stroke();}";
        html += "else if(a.watched){ctx.strokeStyle='#00e5ff';ctx.beginPath();ctx.arc(x,y,9,0,Math.PI*2);ctx.stroke();}";
        html += "else if(a.notable){ctx.strokeStyle='#ff9f1a';ctx.beginPath();ctx.arc(x,y,9,0,Math.PI*2);ctx.stroke();}";
        // Ausgewaehltes Flugzeug (per Klick/Tap, siehe unten) bekommt einen
        // weissen Auswahlring, gleiches Prinzip wie isSelected auf dem
        // Geraete-Display (radar_screen.cpp render()).
        html += "if(a.hex===selectedHex){ctx.strokeStyle='#ffffff';ctx.beginPath();ctx.arc(x,y,11,0,Math.PI*2);ctx.stroke();}";
        html += "ctx.fillStyle=color;ctx.textAlign='center';ctx.fillText(a.callsign,x,y-8);";
        html += "markers.push({x:x,y:y,a:a});";
        html += "});";
        html += "status.textContent=(data.aircraft||[]).length+' aircraft \\u00b7 range '+data.range_km+' km';";
        // Falls das ausgewaehlte Flugzeug in dieser Aktualisierung nicht
        // mehr vorkommt (z.B. aus der Reichweite geflogen), Infobox wieder
        // schliessen statt veraltete Daten stehen zu lassen.
        html += "if(selectedHex){var found=markers.filter(function(m){return m.a.hex===selectedHex;})[0];";
        html += "if(found){showInfo(found.a);}else{selectedHex=null;hideInfo();}}";
        html += "}";

        // Info-Panel fuer ein angetipptes Flugzeug - bewusst eine eigene,
        // stehenbleibende Box (kein Tooltip/Popup, das beim naechsten
        // Neuzeichnen einfach verschwindet), mit explizitem Schliessen-Link,
        // gleiches Grundprinzip wie infoScreen() am Geraet: der Nutzer soll
        // aktiv entscheiden, wann die Info wieder verschwindet.
        html += "function fmtNum(n,d){return (typeof n==='number')?n.toFixed(d):'?';}";
        html += "function showInfo(a){";
        html += "var lines=[];";
        html += "lines.push('<b>'+(a.callsign||a.hex)+'</b> ('+a.hex+')');";
        html += "lines.push('Altitude: '+Math.round(a.alt_ft)+' ft');";
        html += "if(a.speed_kt){lines.push('Speed: '+Math.round(a.speed_kt)+' kt');}";
        html += "lines.push('Distance: '+fmtNum(a.dist_km,1)+' km, bearing '+Math.round(a.bearing_deg)+'\\u00b0');";
        html += "lines.push('Heading: '+Math.round(a.track_deg)+'\\u00b0');";
        html += "if(a.squawk){lines.push('Squawk: '+a.squawk);}";
        html += "infoBox.innerHTML=lines.join('<br>')+'<br><a href=\"#\" id=\"acInfoClose\">Close</a>';";
        html += "infoBox.style.display='block';";
        html += "document.getElementById('acInfoClose').onclick=function(e){e.preventDefault();selectedHex=null;hideInfo();draw(lastData);};";
        html += "}";
        html += "function hideInfo(){infoBox.style.display='none';}";

        html += "canvas.addEventListener('click',function(ev){";
        html += "var rect=canvas.getBoundingClientRect();";
        html += "var scaleX=canvas.width/rect.width,scaleY=canvas.height/rect.height;";
        html += "var px=(ev.clientX-rect.left)*scaleX,py=(ev.clientY-rect.top)*scaleY;";
        html += "var best=null,bestD=14*14;";
        html += "markers.forEach(function(m){var dx=m.x-px,dy=m.y-py,d=dx*dx+dy*dy;if(d<bestD){bestD=d;best=m;}});";
        html += "if(best){selectedHex=best.a.hex;showInfo(best.a);}else{selectedHex=null;hideInfo();}";
        html += "draw(lastData);";
        html += "});";

        html += "if(rangeSel){rangeSel.addEventListener('change',poll);}";
        html += "function poll(){";
        html += "var url='/radar.json';";
        html += "if(rangeSel&&rangeSel.value){url+='?range_km='+rangeSel.value;}";
        html += "fetch(url).then(function(r){return r.json();}).then(draw).catch(function(){status.textContent='Connection lost - retrying...';});";
        html += "}";
        html += "poll();";
        html += "setInterval(poll,3000);";
        // Schnellerer, rein lokaler Redraw-Takt (alle 150ms, ohne Netzwerk-
        // Anfrage) nur fuer das Sternenfunkeln + die Auswahlmarkierung -
        // gleiches Grundprinzip wie tick() vs. render() auf dem
        // Geraete-Display: Flugzeugpositionen aktualisieren sich weiterhin
        // nur alle 3s per poll(), die Sterne twinkeln aber fluessig dazwischen.
        html += "setInterval(function(){draw(lastData);},150);";
        html += "})();</script>";
    }

    void handleRoot() {
        FlightLogbook::DayEntry days[MAX_DAYS_QUERIED];
        uint8_t dayCount = FlightLogbook::listDays(days, MAX_DAYS_QUERIED);

        String html = htmlHeader("Eiswolfs Flightradar");

        appendRadarSection(html);

        html += "<nav><a href=\"/lists\">Manage Airline Filter &amp; Watchlist &rarr;</a></nav>";

        html += "<h2>Flight Logbook</h2>";
        if (dayCount == 0) {
            html += "<p>No logbook entries yet.</p>";
        } else {
            html += "<p><a class=\"dl\" href=\"/export.csv\">Download merged CSV (all days)</a></p>";
            html += "<table><tr><th>Date</th><th>Aircraft</th><th></th><th></th></tr>";
            for (uint8_t i = 0; i < dayCount; i++) {
                String date = String(days[i].date);
                html += "<tr><td>" + date + "</td><td>" + String(days[i].count) + "</td>";
                html += "<td><a class=\"dl\" href=\"/csv?date=" + date + "\">Download</a></td>";
                html += "<td><form method=\"POST\" action=\"/logbook/delete\">";
                html += "<input type=\"hidden\" name=\"date\" value=\"" + date + "\">";
                html += "<button type=\"submit\">Delete</button></form></td></tr>";
            }
            html += "</table>";
        }

        html += "</body></html>";
        server.send(200, "text/html", html);
    }

    void handleExportCsv() {
        FlightLogbook::DayEntry days[MAX_DAYS_QUERIED];
        uint8_t count = FlightLogbook::listDays(days, MAX_DAYS_QUERIED);

        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "text/csv", "");
        server.sendContent("date,timestamp,hex,callsign,reg,type,distance_km,altitude_ft\n");

        SdMutex::Guard guard;
        uint16_t linesSent = 0;
        for (uint8_t i = 0; i < count; i++) {
            char path[48];
            snprintf(path, sizeof(path), "%s/%s.csv", Config::SD_LOG_DIR, days[i].date);

            File f = SD.open(path, FILE_READ);
            if (!f) continue;

            f.readStringUntil('\n');
            while (f.available()) {
                String line = f.readStringUntil('\n');
                line.trim();
                if (line.length() == 0) continue;
                server.sendContent(String(days[i].date) + "," + line + "\n");

                // Wichtig: Ohne regelmaessiges Abgeben der CPU haengt dieser
                // Task (Core 0, Prioritaet 1) die Idle-Task aus, die den
                // Task-Watchdog fuettert. Bei groesseren Logbuechern fuehrt
                // das nach ca. 5s ohne Yield zu einem Watchdog-Reset -
                // genau der Reboot mitten im CSV-Download, den der Nutzer
                // beobachtet hat. delay(1) erzwingt einen Kontextwechsel.
                if (++linesSent % 10 == 0) {
                    delay(1);
                }
            }
            f.close();
            delay(1);
        }
    }

    void handleCsvDownload() {
        if (!server.hasArg("date")) {
            server.send(400, "text/plain", "Bad request");
            return;
        }
        String date = server.arg("date");
        if (!isSafeName(date)) {
            server.send(400, "text/plain", "Bad request");
            return;
        }

        char path[64];
        snprintf(path, sizeof(path), "%s/%s.csv", Config::SD_LOG_DIR, date.c_str());

        SdMutex::Guard guard;
        File f = SD.open(path, FILE_READ);
        if (!f) {
            server.send(404, "text/plain", "Not found");
            return;
        }
        server.sendHeader("Content-Disposition", "attachment; filename=\"" + date + ".csv\"");
        server.streamFile(f, "text/csv");
        f.close();
    }

    void handleLogbookDelete() {
        if (server.hasArg("date")) {
            String date = server.arg("date");
            if (isSafeName(date)) {
                FlightLogbook::deleteFile(date.c_str());
            }
        }
        server.sendHeader("Location", "/");
        server.send(303);
    }

    // Verwaltung von Airline-Filter und Beobachtungsliste per Browser -
    // beide Backend-Module (AirlineFilter/AircraftWatchlist) sind seit
    // dieser Erweiterung mutex-geschuetzt (siehe dort), da sie jetzt sowohl
    // von Core 1 (Radar-/Menue-Screens) als auch von hier aus - Core 0,
    // WebExportServer laeuft innerhalb von NetTask - aufgerufen werden.
    // Praktisch vor allem fuer laengere Eingaben (Rufzeichen, ICAO-Codes),
    // die sich per Handy-Tastatur deutlich bequemer eintippen lassen als
    // ueber die kleine Bildschirmtastatur des Geraets.
    void handleLists() {
        String html = htmlHeader("Eiswolfs Flightradar - Lists");

        html += "<nav><a href=\"/\">&larr; Logbook</a></nav>";

        html += "<h2>Hidden Airlines</h2>";
        html += "<p>Aircraft from these airlines (matched by callsign prefix, e.g. \"DLH\") are hidden from the radar.</p>";
        uint8_t airlineCount = AirlineFilter::count();
        if (airlineCount == 0) {
            html += "<p>No hidden airlines yet.</p>";
        } else {
            html += "<table><tr><th>ICAO prefix</th><th></th></tr>";
            for (uint8_t i = 0; i < airlineCount; i++) {
                html += "<tr><td>" + AirlineFilter::icaoAt(i) + "</td><td>";
                html += "<form method=\"POST\" action=\"/lists/airlines/delete\">";
                html += "<input type=\"hidden\" name=\"index\" value=\"" + String(i) + "\">";
                html += "<button type=\"submit\">Remove</button></form></td></tr>";
            }
            html += "</table>";
        }
        html += "<form method=\"POST\" action=\"/lists/airlines/add\">";
        html += "<input type=\"text\" name=\"icao\" maxlength=\"3\" placeholder=\"e.g. DLH\"> ";
        html += "<button class=\"addbtn\" type=\"submit\">Add</button></form>";

        html += "<h2>Watched Aircraft</h2>";
        html += "<p>The blue LED blinks when any of these exact callsigns appears on the radar (up to 5).</p>";
        uint8_t watchCount = AircraftWatchlist::count();
        if (watchCount == 0) {
            html += "<p>No watched aircraft yet.</p>";
        } else {
            html += "<table><tr><th>Callsign</th><th></th></tr>";
            for (uint8_t i = 0; i < watchCount; i++) {
                html += "<tr><td>" + AircraftWatchlist::callsignAt(i) + "</td><td>";
                html += "<form method=\"POST\" action=\"/lists/watchlist/delete\">";
                html += "<input type=\"hidden\" name=\"index\" value=\"" + String(i) + "\">";
                html += "<button type=\"submit\">Remove</button></form></td></tr>";
            }
            html += "</table>";
        }
        html += "<form method=\"POST\" action=\"/lists/watchlist/add\">";
        html += "<input type=\"text\" name=\"callsign\" maxlength=\"8\" placeholder=\"e.g. DLH441\"> ";
        html += "<button class=\"addbtn\" type=\"submit\">Add</button></form>";

        html += "</body></html>";
        server.send(200, "text/html", html);
    }

    void handleAirlineAdd() {
        if (server.hasArg("icao")) {
            String icao = server.arg("icao");
            icao.trim();
            if (icao.length() > 0 && icao.length() <= 3) {
                AirlineFilter::addHidden(icao.c_str());
            }
        }
        server.sendHeader("Location", "/lists");
        server.send(303);
    }

    void handleAirlineDelete() {
        if (server.hasArg("index")) {
            int idx = server.arg("index").toInt();
            if (idx >= 0 && idx < 255) {
                AirlineFilter::removeHidden((uint8_t)idx);
            }
        }
        server.sendHeader("Location", "/lists");
        server.send(303);
    }

    void handleWatchlistAdd() {
        if (server.hasArg("callsign")) {
            String callsign = server.arg("callsign");
            callsign.trim();
            if (callsign.length() > 0 && callsign.length() <= 8) {
                AircraftWatchlist::addWatched(callsign.c_str());
            }
        }
        server.sendHeader("Location", "/lists");
        server.send(303);
    }

    void handleWatchlistDelete() {
        if (server.hasArg("index")) {
            int idx = server.arg("index").toInt();
            if (idx >= 0 && idx < 255) {
                AircraftWatchlist::removeWatched((uint8_t)idx);
            }
        }
        server.sendHeader("Location", "/lists");
        server.send(303);
    }

    // Datenquelle fuer das Live-Radar auf der Startseite (siehe
    // appendRadarSection()). Wendet dieselben Filter/Prioritaeten an wie das
    // Geraete-Display (render() in radar_screen.cpp): Reichweite, "Boden-
    // fahrzeuge ausblenden", Airline-Filter, sowie Notfall/Beobachtungsliste/
    // "auffaellig" als sich gegenseitig ausschliessende Ring-Markierungen in
    // genau dieser Prioritaet.
    void handleRadarJson() {
        float rangeKm = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];
        // Erlaubt der Web-Ansicht ein eigenes, unabhaengiges Zoomen (siehe
        // Reichweiten-Waehler in appendRadarSection()), OHNE die Geraete-
        // Einstellung zu veraendern. Nur einen der bekannten
        // Config::RANGE_STEPS_KM-Werte akzeptieren - ein fehlender oder
        // nicht erkannter Parameter faellt auf die Geraete-Reichweite zurueck.
        if (server.hasArg("range_km")) {
            float requested = server.arg("range_km").toFloat();
            for (uint8_t i = 0; i < Config::RANGE_STEP_COUNT; i++) {
                if (fabsf(Config::RANGE_STEPS_KM[i] - requested) < 0.5f) {
                    rangeKm = Config::RANGE_STEPS_KM[i];
                    break;
                }
            }
        }
        bool hideGround = SettingsStore::hideGroundVehicles();
        bool emergencyOn = SettingsStore::emergencyAlertEnabled();
        bool watchOn = SettingsStore::watchlistAlertEnabled();

        JsonDocument doc;
        doc["range_km"] = rangeKm;
        JsonArray arr = doc["aircraft"].to<JsonArray>();

        AircraftTable::lock();
        Aircraft* table = AircraftTable::raw();
        for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
            Aircraft& a = table[i];
            if (!a.valid) continue;
            if (a.distanceKm > rangeKm * 1.05f) continue;

            bool isGroundVehicle = a.category[0] == 'C';
            if (hideGround && isGroundVehicle) continue;
            if (AirlineFilter::isHidden(a.callsign)) continue;

            bool isRotorcraft = a.category[0] == 'A' && a.category[1] == '7';
            bool isHeavy = isHeavyCategoryWeb(a.category);
            bool isEmergency = emergencyOn && isEmergencySquawkWeb(a.squawk);
            bool isWatched = watchOn && AircraftWatchlist::isWatched(a.callsign);
            // "notable" (oranger Ring) ist fuer auffaellige Rufzeichen
            // (Militaer/Regierung) reserviert - Heavy-Flugzeuge bekommen
            // stattdessen die eigene Markerform (siehe "heavy" oben). Es
            // gibt aktuell aber keine Militaer-/Regierungs-Praefixliste im
            // Projekt (auch nicht am Geraete-Display, siehe
            // radar_screen.cpp) - "notable" bleibt daher bis auf Weiteres
            // immer false.
            bool isNotable = false;

            JsonObject o = arr.add<JsonObject>();
            o["hex"] = a.hex;
            o["callsign"] = a.callsign[0] ? a.callsign : a.hex;
            o["dist_km"] = a.distanceKm;
            o["bearing_deg"] = a.bearingDeg;
            o["alt_ft"] = a.altBaroFt;
            o["track_deg"] = a.headingDeg;
            o["ground_vehicle"] = isGroundVehicle;
            o["rotorcraft"] = isRotorcraft;
            o["heavy"] = isHeavy;
            o["emergency"] = isEmergency;
            o["watched"] = isWatched;
            o["notable"] = isNotable;
            // Zusaetzliche Felder nur fuer das Info-Panel bei Klick/Tap auf
            // ein Flugzeug (siehe showInfo() in appendRadarSection()) - beide
            // stehen bereits verlustfrei im Aircraft-Snapshot, kein
            // zusaetzlicher Netzwerk-/SD-Zugriff noetig.
            o["speed_kt"] = a.groundSpeedKt;
            o["squawk"] = a.squawk;
        }
        AircraftTable::unlock();

        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    void handleNotFound() {
        server.send(404, "text/plain", "Not found");
    }
}

void begin() {
    server.on("/", handleRoot);
    server.on("/radar.json", handleRadarJson);
    server.on("/export.csv", handleExportCsv);
    server.on("/csv", HTTP_GET, handleCsvDownload);
    server.on("/logbook/delete", HTTP_POST, handleLogbookDelete);
    server.on("/lists", handleLists);
    server.on("/lists/airlines/add", HTTP_POST, handleAirlineAdd);
    server.on("/lists/airlines/delete", HTTP_POST, handleAirlineDelete);
    server.on("/lists/watchlist/add", HTTP_POST, handleWatchlistAdd);
    server.on("/lists/watchlist/delete", HTTP_POST, handleWatchlistDelete);
    server.onNotFound(handleNotFound);
    server.begin();
}

void update() {
    server.handleClient();
}

}
