#include "web_export_server.h"
#include "config.h"
#include "flight_logbook.h"
#include "sd_mutex.h"
#include "airline_filter.h"
#include "aircraft_watchlist.h"
#include "squawk_watchlist.h"
#include "aircraft_table.h"
#include "settings_store.h"
#include "location_manager.h"
#include "units.h"
#include "weather.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <cstring>
#include <cmath>

namespace WebExportServer {

namespace {
    constexpr uint8_t MAX_DAYS_QUERIED = 31;

    WebServer server(80);

    // Zeitstempel der letzten "/radar.json"-Abfrage - Grundlage fuer
    // isRadarUiActive() (siehe web_export_server.h). Gefahrlos ohne Mutex/
    // Atomics: server.handleClient() (und damit handleRadarJson()) laeuft
    // synchron innerhalb von WebExportServer::update(), das ausschliesslich
    // von NetTask auf Core 0 aufgerufen wird - kein anderer Task schreibt
    // oder liest diese Variable.
    uint32_t lastRadarJsonRequestMs = 0;
    constexpr uint32_t RADAR_UI_ACTIVE_WINDOW_MS = 20000; // > 8s Poll-Intervall der Seite, mit Puffer

    // Web-Pendant zu UiTheme::accentColor() (siehe ui_theme.h/.cpp auf dem
    // Geraet) - Alex' Wunsch, das WebUI-Farbthema (Gruen/Amber/Blau)
    // automatisch mit dem Geraet zu synchronisieren, statt fest gruen zu
    // bleiben. Drei Werte pro Thema statt nur einer Akzentfarbe: "accent"
    // (Haupttext/Rahmen/Links), "accentBorder" (dezente Trennlinien/
    // Kreis-Rahmen, dunklere Abstufung) und "accentMuted" (Sekundaertext
    // wie #radarStatus/Footer) - dieselbe Abstufungslogik wie
    // UiTheme::accentColorDimmed() auf dem Geraet, hier aber als eigene,
    // fest hinterlegte Hex-Werte statt einer Laufzeit-Berechnung (spart
    // Code auf beiden Seiten: Server UND die Client-JS-Kopie unten in
    // appendRadarSection() muessen dieselben drei Werte kennen, siehe
    // dortiges THEME_PALETTES). Das Sternenfunkeln (siehe
    // appendStarBackground()) zerlegt "accent" client-seitig per
    // hexToRgb() in einzelne Kanaele, um die Helligkeit pro Stern zu
    // skalieren, statt eine feste Farbe zu verwenden.
    struct WebTheme {
        const char* accent;
        const char* accentBorder;
        const char* accentMuted;
    };

    WebTheme currentWebTheme() {
        switch (SettingsStore::radarThemeIndex()) {
            case 1: return {"#ffb000", "#3a2c1a", "#a08a5a"};  // Amber
            case 2: return {"#00c8ff", "#1a2c3a", "#6a90a0"};  // Blau
            case 3: return {"#ff0000", "#3a1a1a", "#a06a6a"};  // Rot
            case 4: return {"#b400ff", "#2a1a3a", "#8a6aa0"};  // Lila
            default: return {"#39ff14", "#1f3a2b", "#7a9a86"}; // Gruen (Standard)
        }
    }

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

    // Vollbild-Sternenhintergrund fuer die ganze Seite (nicht nur innerhalb
    // des kleinen Radar-Canvas) - 1:1 uebernommen vom Web-Flasher
    // (index.html, separat gehostet auf GitHub Pages, nicht Teil dieses
    // Firmware-Repos), auf Wunsch von Alex, damit beide Web-Auftritte des
    // Projekts denselben Look haben. Gleiche Dreieckswellen-Twinkle-Formel
    // (Phase 0-255-0) wie MenuStars auf dem Geraet und wie die bestehenden
    // Sterne INNERHALB des Radar-Canvas (siehe appendRadarSection() unten) -
    // hier nur als eigenstaendiger Vollbild-Layer hinter dem gesamten
    // Seiteninhalt statt nur hinter dem Radarkreis. Als eigenes <canvas>
    // "#star-bg" ganz am Anfang von <body> eingefuegt (fixed, z-index:0,
    // pointer-events:none - siehe CSS in htmlHeader()), waehrend der
    // restliche Seiteninhalt in einen ".page"-Wrapper mit z-index:1
    // gepackt wird (siehe htmlHeader()/handleRoot()/handleLists()), damit
    // die Sterne zuverlaessig HINTER Text/Tabellen/Buttons bleiben.
    void appendStarBackground(String& html) {
        html += "<canvas id=\"star-bg\"></canvas>";
        html += "<script>(function(){";
        html += "var canvas=document.getElementById('star-bg');";
        html += "if(!canvas)return;";
        html += "var ctx=canvas.getContext('2d');";
        html += "var stars=[];";
        html += "function starCountFor(w,h){var density=(w*h)/9000;return Math.max(40,Math.min(160,Math.round(density)));}";
        html += "function resize(){canvas.width=window.innerWidth;canvas.height=window.innerHeight;}";
        html += "function initStars(){var count=starCountFor(canvas.width,canvas.height);stars=[];";
        html += "for(var i=0;i<count;i++){stars.push({x:Math.random()*canvas.width,y:Math.random()*canvas.height,phase:Math.random()*256,speed:1+Math.random()*2});}}";
        html += "var resizeTimer=null;";
        html += "window.addEventListener('resize',function(){clearTimeout(resizeTimer);resizeTimer=setTimeout(function(){resize();initStars();},150);});";
        html += "resize();initStars();";
        // Sternenfarbe folgt jetzt der CSS-Variable "--accent" (siehe
        // htmlHeader()) statt fest Gruen - hexToRgb() einmal PRO FRAME (nicht
        // pro Stern) aufgeloest, damit ein Themenwechsel waehrend die Seite
        // offen ist (poll() aendert --accent live, siehe appendRadarSection())
        // automatisch beim naechsten requestAnimationFrame-Tick greift, ohne
        // dass dieses eigenstaendige Skript selbst etwas davon "mitbekommen"
        // muss.
        html += "function hexToRgb(hex){var v=parseInt(hex.replace('#',''),16);return [(v>>16)&255,(v>>8)&255,v&255];}";

        // Schnee-Overlay fuer den GESAMTEN Seitenhintergrund (nicht den
        // Radar-Canvas - der zeigt weiterhin nur Regen, siehe drawRain() in
        // appendRadarSection()). Gleiche Optik/Physik wie ScreensaverSnow in
        // main.cpp: kleine weisse Punkte, langsam fallend, mit seitlichem
        // sinusfoermigem Wackeln statt einer geraden Linie wie beim Regen -
        // bewusst IMMER Weiss (nicht --accent), damit Schnee optisch klar
        // vom themenfarbigen Regen unterscheidbar bleibt, genau wie am
        // Geraet (dort TFT_WHITE, fest, unabhaengig vom Farbthema). Datenquelle
        // ist "window.__radarData", das appendRadarSection() bei jedem
        // /radar.json-Poll setzt (siehe dortiges draw(data), Feld
        // "snowing"/"snow_intensity") - dieses Skript hier laeuft in einer
        // eigenen Closure (siehe "(function(){" oben), "lastData" dort ist
        // NICHT direkt sichtbar, daher der Umweg ueber "window". Auf Seiten
        // ohne Radar-Canvas (z.B. Listen-Seiten) bleibt "window.__radarData"
        // undefined, das Overlay bleibt dann einfach inaktiv.
        html += "var SNOW_MAX=13,SNOW_R=2;var snowFlakes=[];var snowInited=false;var lastSnowMs=null;";
        html += "function snowParamsFor(level){if(level>=3)return{count:13,speed:35};if(level===1)return{count:4,speed:15};return{count:8,speed:25};}";
        html += "function snowSpawn(f){f.baseX=Math.random()*canvas.width;f.y=-Math.random()*canvas.height/2;f.phase=Math.random()*6.28;}";
        html += "function drawSnow(level){var p=snowParamsFor(level);";
        html += "var now=performance.now();var dt=lastSnowMs?Math.min(now-lastSnowMs,300):16;lastSnowMs=now;var step=p.speed*dt/1000;";
        html += "if(!snowInited){snowFlakes=[];for(var i=0;i<SNOW_MAX;i++){var f={};snowSpawn(f);snowFlakes.push(f);}snowInited=true;}";
        html += "ctx.save();ctx.fillStyle='#ffffff';";
        html += "for(var i=0;i<snowFlakes.length;i++){var f=snowFlakes[i];f.y+=step;f.phase+=2.5*dt/1000;";
        html += "if(f.y-SNOW_R>canvas.height){snowSpawn(f);}";
        html += "if(i>=p.count)continue;";
        html += "var x=f.baseX+10*Math.sin(f.phase);";
        html += "ctx.beginPath();ctx.arc(x,f.y,SNOW_R,0,Math.PI*2);ctx.fill();}";
        html += "ctx.restore();}";

        html += "function draw(){ctx.clearRect(0,0,canvas.width,canvas.height);";
        html += "var accentHex=getComputedStyle(document.documentElement).getPropertyValue('--accent').trim();";
        html += "var rgb=hexToRgb(accentHex||'#39ff14');";
        html += "for(var i=0;i<stars.length;i++){var s=stars[i];s.phase=(s.phase+s.speed)%256;";
        html += "var bright=s.phase<128?s.phase*2:(255-s.phase)*2;";
        html += "ctx.fillStyle='rgb('+Math.round(rgb[0]*bright/255)+','+Math.round(rgb[1]*bright/255)+','+Math.round(rgb[2]*bright/255)+')';ctx.fillRect(s.x,s.y,2,2);}";
        html += "var rd=window.__radarData;";
        html += "if(rd&&rd.snowing){drawSnow(rd.snow_intensity||2);}else{lastSnowMs=null;snowInited=false;}";
        html += "requestAnimationFrame(draw);}";
        html += "draw();";
        html += "})();</script>";
    }

    String htmlHeader(const String& title) {
        String html;
        // Einmalige, grosszuegige Vorab-Reservierung statt hunderter
        // einzelner "+="-Reallozierungen: Arduino Strings scheitern bei
        // einem fehlgeschlagenen realloc() (fragmentierter ESP32-Heap)
        // STILL - der Rueckgabewert von concat() wird ueberall im Projekt
        // ungeprueft verworfen, ein einzelner missgluecketer "+=" mitten in
        // dieser sehr grossen Seite laesst dann klanglos genau das
        // dahinterstehende Stueck HTML/JS (z.B. den Leaflet-<script>-Tag)
        // verschwinden, ohne Fehler/Crash - der Browser bekommt dadurch ein
        // kaputtes <script src="..."> und laedt stattdessen faelschlich die
        // ESP32-eigene 404-Seite als "JavaScript" (siehe Bugreport: "Map"-
        // Tab, "Unexpected token '<'" + 404). Eine einzige fruehe grosse
        // Reservierung (statt vieler kleiner, spaeter im schon
        // fragmentierteren Heap) macht diesen Fehlschlag deutlich
        // unwahrscheinlicher.
        html.reserve(20480);
        WebTheme wt = currentWebTheme();
        html += "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
        html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
        html += "<title>" + title + "</title>";
        html += "<style>";
        // CSS-Variablen statt fest verdrahtetem Gruen - Alex' Wunsch,
        // dieselbe systemweite Farbthema-Logik wie auf dem Geraet selbst
        // (siehe ui_theme.h) auch fuer das WebUI zu haben. Server-seitig
        // beim Seitenaufbau mit dem AKTUELLEN Thema vorbefuellt (kein
        // "erst gruen, dann Nachladen" beim ersten Rendern), die Startseite
        // (appendRadarSection()) aktualisiert diese Variablen zusaetzlich
        // per JS live, falls sich das Geraete-Thema aendert, waehrend die
        // Seite bereits offen ist.
        html += ":root{--accent:" + String(wt.accent) + ";--accent-border:" + String(wt.accentBorder) +
                ";--accent-muted:" + String(wt.accentMuted) + ";}";
        html += "body{background-color:#0a0f0d;color:var(--accent);font-family:'Courier New',Courier,monospace;padding:20px;}";
        html += "h1{font-size:20px;}h2{font-size:16px;margin-top:24px;border-top:1px solid var(--accent-border);padding-top:12px;}";
        html += "table{border-collapse:collapse;margin-top:10px;width:100%;}";
        html += "td,th{padding:4px 12px;text-align:left;border-bottom:1px solid var(--accent-border);}";
        html += "a{color:var(--accent);}";
        html += "form{display:inline;}";
        html += "button{background:#0a0f0d;color:#ff3b3b;border:1px solid #ff3b3b;border-radius:4px;padding:3px 10px;font-family:inherit;cursor:pointer;}";
        html += "button:hover{background:#ff3b3b;color:#0a0f0d;}";
        html += "button:disabled{opacity:.5;cursor:default;background:#0a0f0d;color:#ff3b3b;}";
        html += ".dl{color:var(--accent);text-decoration:none;border:1px solid var(--accent);border-radius:4px;padding:3px 10px;margin-right:6px;display:inline-block;}";
        // Bewusst weiterhin die Akzentfarbe (nicht Rot) fuer "Hinzufuegen"-
        // Buttons - die roten button{}-Regeln oben bleiben fuer alle
        // destruktiven "Entfernen/Loeschen"-Buttons unveraendert (Rot ist
        // eine feste Alarmfarbe, folgt NICHT dem Thema, genau wie am
        // Geraete-Display), .addbtn ist ausschliesslich fuer die neuen
        // Listen-Formulare (siehe handleLists()) gedacht.
        html += ".addbtn{background:#0a0f0d;color:var(--accent);border:1px solid var(--accent);border-radius:4px;padding:3px 10px;font-family:inherit;cursor:pointer;}";
        html += ".addbtn:hover{background:var(--accent);color:#0a0f0d;}";
        html += "input[type=text]{background:#0a0f0d;color:var(--accent);border:1px solid var(--accent);border-radius:4px;padding:5px 8px;font-family:inherit;}";
        html += "nav{margin-bottom:10px;}nav a{color:var(--accent);margin-right:16px;}";
        html += "#radarCanvas{width:100%;max-width:400px;height:auto;background:#05100a;border:1px solid var(--accent-border);border-radius:8px;display:block;cursor:pointer;}";
        html += "#radarStatus{font-size:12px;color:var(--accent-muted);margin-top:4px;}";
        html += "#radarControls{font-size:12px;margin-bottom:8px;}";
        html += "#radarControls select{background:#0a0f0d;color:var(--accent);border:1px solid var(--accent);border-radius:4px;padding:2px 6px;font-family:inherit;}";
        html += "#acInfo{display:none;max-width:400px;margin-top:8px;padding:8px 10px;border:1px solid var(--accent);border-radius:6px;font-size:13px;line-height:1.7;}";
        html += "#acInfo a{color:#ff3b3b;text-decoration:none;border:1px solid #ff3b3b;border-radius:4px;padding:2px 8px;display:inline-block;margin-top:4px;}";
        // Sofortige optische Rueckmeldung beim Antippen/Klicken - reine
        // CSS-":active"-Pseudoklasse, greift also schon beim Antippen
        // (touchstart/mousedown), bevor ueberhaupt JavaScript ausgefuehrt
        // wird. Gilt fuer JEDEN Button/Link im Info-Panel (Close UND den
        // FlightAware-Link), Alex' Grundprinzip: ein Tap muss immer sofort
        // sichtbar reagieren, egal was danach passiert oder wie lange es
        // dauert - siehe gleiche Ueberlegung beim OTA-Neustart-Button.
        html += "#acInfo a:active{background:#ff3b3b;color:#0a0f0d;}";
        // Umschalter "Radar"/"Map" (siehe appendRadarSection()) - gleicher
        // Button-Stil wie die restliche Seite, aktiver Tab invertiert
        // (gefuellte Akzentfarbe, dunkler Text), genau wie aktive Eintraege
        // ueberall sonst im Projekt (siehe Geraete-UI-Konvention).
        html += "#viewTabs{margin-bottom:8px;}";
        html += "#viewTabs button{background:#0a0f0d;color:var(--accent);border:1px solid var(--accent);border-radius:4px;padding:4px 14px;font-family:inherit;cursor:pointer;margin-right:6px;}";
        html += "#viewTabs button.active{background:var(--accent);color:#0a0f0d;}";
        // Leaflet verlangt eine feste Hoehe auf dem Karten-Container (kein
        // Auto-Sizing wie beim Canvas oben) - gleiche Breite/Rahmen-Optik
        // wie #radarCanvas, damit beide Ansichten optisch zusammengehoeren.
        html += "#mapView{display:none;}";
        html += "#leafletMap{width:100%;max-width:400px;height:340px;border:1px solid var(--accent-border);border-radius:8px;background:#05100a;}";
        // Leaflets eigene Popup-Box ist per Default hell/weiss - an das
        // dunkle Seitendesign angeglichen, gleiche Farben wie #acInfo oben.
        html += ".leaflet-popup-content-wrapper,.leaflet-popup-tip{background:#0a0f0d;color:var(--accent);}";
        html += ".leaflet-popup-content{font-family:inherit;font-size:13px;line-height:1.7;}";
        html += ".leaflet-popup-content a{color:#ff3b3b;}";
        // Sternenhintergrund liegt als eigener, fixierter Layer HINTER der
        // eigentlichen Seite (siehe appendStarBackground() oben) - ".page"
        // bekommt deshalb einen eigenen Stacking-Context mit hoeherem
        // z-index, sonst wuerde der Canvas-Layer (position:fixed) trotz
        // z-index:0 vor dem normal fliessenden Seiteninhalt liegen.
        html += "#star-bg{position:fixed;inset:0;width:100%;height:100%;z-index:0;pointer-events:none;}";
        html += ".page{position:relative;z-index:1;}";
        html += "</style></head><body>";
        appendStarBackground(html);
        html += "<div class=\"page\">";
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

        // Gleiche Auto/Metrisch/Imperial-Logik wie ueberall sonst am Geraet
        // (Menues, Detailpanel, Listen - siehe LocationManager::
        // useMetricUnits()) - die WebUI-Seite bleibt bewusst komplett
        // Englisch (auf Wunsch von Alex, kein Aufwand fuer eine 6-sprachige
        // Uebersetzung von >40 Textstellen), aber die angezeigten EINHEITEN
        // sollen trotzdem zur Geraete-Einstellung passen, statt fuer alle
        // Nutzer hart Kilometer zu zeigen (z.B. fuer jemanden in den USA).
        // Das Options-"value" bleibt bewusst in km (wird 1:1 als
        // "range_km"-Query-Parameter verschickt und in handleRadarJson()
        // gegen Config::RANGE_STEPS_KM verglichen) - nur das sichtbare Label
        // wechselt auf nm, exakt wie beim Reichweiten-Label am
        // Geraete-Display selbst (siehe radar_screen.cpp, "%.0fnm").
        bool metric = LocationManager::useMetricUnits();

        html += "<h2>Live Radar</h2>";
        // Karten-Tab (Leaflet + OpenStreetMap-Kacheln, beide per CDN aus dem
        // Browser des Nutzers geladen - NICHT im ESP32-Flash eingebettet,
        // siehe Alex' ausdruecklicher Vorgabe). Radar-Canvas bleibt
        // unveraendert der Default-Tab, die Karte ist eine zusaetzliche,
        // gleichberechtigte Ansicht derselben /radar.json-Daten, kein
        // Ersatz. Feste Versionsnummer (1.9.4) statt "latest", wie bei
        // jeder externen Bibliothek im Projekt ueblich.
        html += "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/leaflet.min.css\">";
        html += "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.9.4/leaflet.min.js\"></script>";
        html += "<div id=\"viewTabs\"><button id=\"tabRadar\" class=\"active\" type=\"button\">Radar</button><button id=\"tabMap\" type=\"button\">Map</button></div>";
        html += "<div id=\"radarView\">";
        html += "<div id=\"radarControls\">Range: <select id=\"radarRange\">";
        for (uint8_t i = 0; i < Config::RANGE_STEP_COUNT; i++) {
            bool isDefault = fabsf(Config::RANGE_STEPS_KM[i] - deviceRangeKm) < 0.5f;
            float labelValue = metric ? Config::RANGE_STEPS_KM[i] : Units::kmToNm(Config::RANGE_STEPS_KM[i]);
            html += "<option value=\"" + String(Config::RANGE_STEPS_KM[i], 0) + "\"";
            if (isDefault) html += " selected";
            html += ">" + String(labelValue, 0) + (metric ? " km</option>" : " nm</option>");
        }
        html += "</select></div>";
        // Live mitzaehlende "zuletzt aktualisiert"-Anzeige (Alex' Wunsch) -
        // eigenes Element ueber dem Canvas statt in #radarStatus verbaut
        // (das zeigt weiterhin nur Flugzeuganzahl/Reichweite nach jedem
        // erfolgreichen Abruf) - so bleiben "Ergebnis des letzten Abrufs"
        // und "wie lange ist das her" zwei getrennte, unabhaengig lesbare
        // Informationen. Aktualisiert sich per eigenem 1s-Intervall
        // (updateFreshness() unten), NICHT nur einmalig beim Laden - direkt
        // erkennbar, ob die Verbindung gerade frisch ist oder hakt.
        html += "<div id=\"radarFreshness\" style=\"font-size:12px;color:var(--accent-muted);margin-bottom:6px;\">Waiting for first update...</div>";
        html += "<canvas id=\"radarCanvas\" width=\"360\" height=\"360\"></canvas>";
        html += "<p id=\"radarStatus\">Loading...</p>";
        html += "<div id=\"acInfo\"></div>";
        html += "</div>"; // #radarView
        html += "<div id=\"mapView\"><div id=\"leafletMap\"></div></div>";
        html += "<script>(function(){";
        html += "var canvas=document.getElementById('radarCanvas');";
        html += "var ctx=canvas.getContext('2d');";
        html += "var status=document.getElementById('radarStatus');";
        html += "var infoBox=document.getElementById('acInfo');";
        html += "var rangeSel=document.getElementById('radarRange');";
        html += "var W=canvas.width,H=canvas.height,cx=W/2,cy=H/2,R=Math.min(W,H)/2-24;";
        html += "var lastData={range_km:" + String(deviceRangeKm, 0) + ",aircraft:[]};";
        html += "var lastUpdateMs=null;"; // fuer die "zuletzt aktualisiert"-Anzeige, siehe updateFreshness() unten
        // Client-seitige Kopie derselben 3 Farbthemen wie WebTheme/
        // currentWebTheme() in C++ (siehe dortiger Kommentar) - noetig, damit
        // ein Themenwechsel am Geraet WAEHREND die Seite offen ist (poll()
        // liefert dann ein geaendertes data.theme_index) sofort live per JS
        // uebernommen werden kann, ohne die Seite neu laden zu muessen. Bei
        // Aenderung an einer der drei Farben bitte BEIDE Stellen synchron
        // halten.
        html += "var THEME_PALETTES=[['#39ff14','#1f3a2b','#7a9a86'],['#ffb000','#3a2c1a','#a08a5a'],['#00c8ff','#1a2c3a','#6a90a0'],['#ff0000','#3a1a1a','#a06a6a'],['#b400ff','#2a1a3a','#8a6aa0']];";
        html += "var lastThemeIndex=" + String(SettingsStore::radarThemeIndex()) + ";";
        html += "function applyTheme(idx){var p=THEME_PALETTES[idx]||THEME_PALETTES[0];var s=document.documentElement.style;";
        html += "s.setProperty('--accent',p[0]);s.setProperty('--accent-border',p[1]);s.setProperty('--accent-muted',p[2]);}";
        html += "function hexToRgb(hex){var v=parseInt(hex.replace('#',''),16);return [(v>>16)&255,(v>>8)&255,v&255];}";
        html += "function cssVar(name){return getComputedStyle(document.documentElement).getPropertyValue(name).trim();}";
        html += "var markers=[];";
        html += "var selectedHex=null;";
        // Alle sichtbaren Distanz-/Reichweitenangaben (Ringe, Statuszeile,
        // Info-Panel) laufen ueber diese zwei Funktionen statt Kilometer
        // fest einzubrennen - die Werte selbst (data.range_km, a.dist_km)
        // bleiben unveraendert in km, nur die ANZEIGE wechselt auf nm, wenn
        // das Geraet auf Imperial/Auto-Imperial steht (siehe metric-Flag
        // oben, serverseitig aus LocationManager::useMetricUnits() gesetzt -
        // dieselbe Umrechnung 1nm=1.852km wie Units::kmToNm() in C++).
        html += "var metric=" + String(metric ? "true" : "false") + ";";
        html += "function fmtRange(km){return metric?(Math.round(km)+' km'):(Math.round(km/1.852)+' nm');}";
        html += "function fmtDist(km){return metric?(km.toFixed(1)+' km'):((km/1.852).toFixed(1)+' nm');}";

        // Hintergrund-Sterne AUSSERHALB des Radarkreises - gleiches Prinzip
        // wie updateBgStars()/initBgStarsIfNeeded() in radar_screen.cpp
        // (Rejection-Sampling, damit kein Stern innerhalb des Kreises
        // landet), hier per Canvas/JS statt TFT_eSPI nachgebaut.
        html += "var stars=[];";
        html += "(function(){var minDistSq=(R+6)*(R+6);for(var i=0;i<24;i++){var x,y,tries=0;";
        html += "do{x=4+Math.random()*(W-8);y=4+Math.random()*(H-8);tries++;}";
        html += "while(((x-cx)*(x-cx)+(y-cy)*(y-cy))<minDistSq&&tries<25);";
        html += "stars.push({x:x,y:y,phase:Math.random()*255,speed:1+Math.random()*2});}})();";
        html += "function drawStars(accentRgb){for(var i=0;i<stars.length;i++){var s=stars[i];";
        html += "s.phase=(s.phase+s.speed)%256;";
        html += "var bright=Math.round(s.phase<128?s.phase*2:(255-s.phase)*2);";
        html += "ctx.fillStyle='rgb('+Math.round(accentRgb[0]*bright/255)+','+Math.round(accentRgb[1]*bright/255)+','+Math.round(accentRgb[2]*bright/255)+')';ctx.fillRect(s.x,s.y,1,1);}}";

        // Regen-Overlay (Alex' Wunsch: "Spiegel des CYD-Radarscreens") - nur
        // aktiv, wenn data.raining true ist (siehe handleRadarJson():
        // SettingsStore::rainEffectEnabled() + Weather::current() + gueltige
        // Windrichtung). Geometrie 1:1 wie radar_screen.cpp::spawnRainDrop():
        // Tropfen starten am Radarkreis-Rand bei bearing=windDirDeg±70°
        // (Streuung), fliegen als parallele Sehnen in Richtung
        // "windDirDeg+180" (= windabwaerts) durch den Kreis, werden beim
        // Verlassen sofort an neuer Randposition neu gestartet. Gleiche
        // Kompass-Konvention (0=Norden=oben, im Uhrzeigersinn) wie die
        // Flugzeug-Positionierung oben (bearing_deg -> sin/cos). Anders als
        // beim Geraete-Regen (fest TFT_SKYBLUE) zeichnet das WebUI die
        // Tropfen in der aktuellen Themenfarbe (--accent), passend zum
        // bestehenden Farbthema-Sync (applyTheme()/THEME_PALETTES oben) -
        // Alex' ausdruecklicher Wunsch fuer diese Stelle. draw() leert das
        // gesamte Canvas bei JEDEM Aufruf (ctx.clearRect oben) und zeichnet
        // alles neu, deshalb ist hier - anders als beim Geraete-Ruhebildschirm
        // - KEIN manuelles Erase/Redraw einzelner Tropfen noetig.
        // rainInited/lastRainMs werden zurueckgesetzt, sobald raining=false
        // ist, damit beim naechsten Aktivwerden (evtl. mit geaenderter
        // Windrichtung) alle Tropfen sauber neu am Rand gestartet werden statt
        // von einer veralteten Position aus weiterzufliegen.
        //
        // Tropfenzahl/Fallgeschwindigkeit sind an data.rain_intensity
        // gekoppelt (0=None/1=Light/2=Moderate/3=Heavy, siehe
        // Weather::RainIntensity + handleRadarJson()) - dieselben drei
        // Stufen-Werte wie radar_screen.cpp/ScreensaverRain (main.cpp),
        // dort dupliziert statt geteilt (CLAUDE.md-Konvention), bitte bei
        // Aenderungen synchron halten. Das Array ist immer auf die groesste
        // Stufe ("stark", 20 Tropfen) dimensioniert und wird auch bei
        // niedrigerer Stufe komplett WEITERBEWEGT (nur die ersten
        // "count" werden tatsaechlich GEZEICHNET) - so faellt kein Tropfen
        // "eingefroren" beim naechsten Hochstufen ploetzlich aus dem Stand
        // an, sondern ist schon in Bewegung.
        html += "var RAIN_LEN=10,RAIN_MAX=20;";
        html += "function rainParamsFor(level){if(level>=3)return{count:20,speed:130};if(level===1)return{count:6,speed:60};return{count:12,speed:90};}";
        html += "var rainDrops=[];var rainInited=false;var lastRainMs=null;";
        html += "function rainSpawn(d,windDirDeg){var spread=Math.random()*140-70;";
        html += "var entryRad=(windDirDeg+spread)*Math.PI/180;d.x=cx+R*Math.sin(entryRad);d.y=cy-R*Math.cos(entryRad);";
        html += "var travelRad=(windDirDeg+180)*Math.PI/180;d.vx=Math.sin(travelRad);d.vy=-Math.cos(travelRad);}";
        html += "function drawRain(windDirDeg,level,accentColor){var p=rainParamsFor(level);";
        html += "var now=performance.now();var dt=lastRainMs?Math.min(now-lastRainMs,300):16;lastRainMs=now;var step=p.speed*dt/1000;";
        html += "if(!rainInited){rainDrops=[];for(var i=0;i<RAIN_MAX;i++){var d={};rainSpawn(d,windDirDeg);rainDrops.push(d);}rainInited=true;}";
        html += "ctx.save();ctx.strokeStyle=accentColor;ctx.globalAlpha=0.45;ctx.lineWidth=1;";
        html += "for(var i=0;i<rainDrops.length;i++){var d=rainDrops[i];d.x+=d.vx*step;d.y+=d.vy*step;";
        html += "var ddx=d.x-cx,ddy=d.y-cy;if(ddx*ddx+ddy*ddy>R*R){rainSpawn(d,windDirDeg);}";
        html += "if(i>=p.count)continue;";
        html += "var x1=d.x,y1=d.y,x2=d.x-d.vx*RAIN_LEN,y2=d.y-d.vy*RAIN_LEN;";
        html += "var d1=(x1-cx)*(x1-cx)+(y1-cy)*(y1-cy),d2=(x2-cx)*(x2-cx)+(y2-cy)*(y2-cy);";
        html += "if(d1<=R*R&&d2<=R*R){ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.stroke();}}";
        html += "ctx.restore();}";

        // altColor() bildet die Flughoehe ab (Notfall-/Warnfarben) - bleibt
        // AUSDRUECKLICH themenunabhaengig, exakt wie colorForAltitude() am
        // Geraete-Display (radar_screen.cpp) - NICHT anfassen/umfaerben.
        html += "function altColor(ft){if(ft<3000)return '#ff4d4d';if(ft<10000)return '#ffb84d';if(ft<25000)return '#ffe14d';if(ft<35000)return '#39ff14';return '#4dd2ff';}";

        html += "function draw(data){";
        html += "lastData=data;";
        // Zusaetzlich auf "window" gespiegelt (dieser gesamte Block ist in
        // einer eigenen IIFE gekapselt, siehe "<script>(function(){" oben -
        // "lastData" ist also NUR innerhalb dieser Closure sichtbar). Das
        // separate #star-bg-Skript (siehe appendStarBackground(), eigene
        // IIFE) braucht die "snowing"/"snow_intensity"-Felder aber ebenfalls,
        // um Schnee auf dem Seitenhintergrund zu zeichnen - "window.__radarData"
        // ist der bewusst explizite, minimale Kanal dafuer statt beide
        // Skripte in einer gemeinsamen Closure zusammenzulegen.
        html += "window.__radarData=data;";
        // Aktuelle Thema-Farben EINMAL pro draw()-Aufruf (nicht pro Stern/
        // Element) aus den CSS-Variablen gelesen - draw() laeuft sowohl bei
        // jedem poll() (8s) als auch bei jedem lokalen 150ms-Redraw, liest
        // die evtl. von applyTheme() geaenderten Werte also automatisch mit,
        // ohne dass draw() selbst etwas ueber Themenwechsel wissen muss.
        html += "var accentColor=cssVar('--accent')||'#39ff14';var borderColor=cssVar('--accent-border')||'#1f3a2b';var accentRgb=hexToRgb(accentColor);";
        html += "ctx.clearRect(0,0,W,H);";
        html += "drawStars(accentRgb);";
        html += "ctx.strokeStyle=borderColor;ctx.fillStyle=accentColor;ctx.font='10px monospace';ctx.textAlign='left';";
        html += "for(var ring=1;ring<=3;ring++){var r=R*ring/3;ctx.beginPath();ctx.arc(cx,cy,r,0,Math.PI*2);ctx.stroke();";
        html += "ctx.fillText(fmtRange(data.range_km*ring/3),cx+4,cy-r+10);}";
        html += "ctx.strokeStyle=borderColor;ctx.beginPath();ctx.moveTo(cx-R,cy);ctx.lineTo(cx+R,cy);ctx.moveTo(cx,cy-R);ctx.lineTo(cx,cy+R);ctx.stroke();";
        // Alle vier Himmelsrichtungen (N/E/S/W), genau wie
        // drawStaticBackground() in radar_screen.cpp - vorher stand hier nur
        // "N", was auf Nachfrage ergaenzt wurde.
        html += "ctx.fillStyle=accentColor;ctx.textAlign='center';ctx.fillText('N',cx,cy-R-8);";
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
        html += "if(data.raining){drawRain(data.wind_dir_deg||0,data.rain_intensity||2,accentColor);}else{lastRainMs=null;rainInited=false;}";
        html += "status.textContent=(data.aircraft||[]).length+' aircraft \\u00b7 range '+fmtRange(data.range_km);";
        html += "}";

        // WICHTIG: Das erneute Aufbauen der Infobox (showInfo(), baut u.a.
        // den FlightAware-Link per innerHTML NEU auf) darf NICHT bei jedem
        // draw()-Aufruf passieren - draw() laeuft auch alle 150ms rein lokal
        // fuer das Sternenfunkeln (siehe setInterval() weiter unten), ganz
        // ohne neue Daten. Bisher stand dieser Block direkt in draw() und
        // hat dadurch den Link-Knoten im Info-Panel ~6-7 mal pro Sekunde neu
        // erzeugt - ein Tap/Klick mitten in diesem staendigen Austausch traf
        // oft ins Leere, weil der urspruengliche Link-Knoten schon durch
        // einen neuen ersetzt war, bevor der Klick beim Browser "ankam"
        // (Alex' Meldung: "muss man 10 mal anklicken"). Deshalb jetzt eine
        // eigene Funktion, die NUR dann aufgerufen wird, wenn tatsaechlich
        // neue Daten da sind (siehe poll() weiter unten, alle 8s) - der rein
        // lokale 150ms-Sterne-Takt fasst die Infobox gar nicht mehr an.
        html += "function refreshSelectedInfo(){";
        html += "if(!selectedHex)return;";
        html += "var found=markers.filter(function(m){return m.a.hex===selectedHex;})[0];";
        html += "if(found){showInfo(found.a);}else{selectedHex=null;hideInfo();}";
        html += "}";

        // Info-Panel fuer ein angetipptes Flugzeug - bewusst eine eigene,
        // stehenbleibende Box (kein Tooltip/Popup, das beim naechsten
        // Neuzeichnen einfach verschwindet), mit explizitem Schliessen-Link,
        // gleiches Grundprinzip wie infoScreen() am Geraet: der Nutzer soll
        // aktiv entscheiden, wann die Info wieder verschwindet.
        html += "function showInfo(a){";
        html += "var lines=[];";
        html += "lines.push('<b>'+(a.callsign||a.hex)+'</b> ('+a.hex+')');";
        html += "lines.push('Altitude: '+Math.round(a.alt_ft)+' ft');";
        html += "if(a.speed_kt){lines.push('Speed: '+Math.round(a.speed_kt)+' kt');}";
        html += "lines.push('Distance: '+fmtDist(a.dist_km)+', bearing '+Math.round(a.bearing_deg)+'\\u00b0');";
        html += "lines.push('Heading: '+Math.round(a.track_deg)+'\\u00b0');";
        html += "if(a.squawk){lines.push('Squawk: '+a.squawk);}";
        // Link auf dieselbe FlightAware-Tracking-Seite, die auch der
        // QR-Code am Geraete-Display zeigt (siehe runFlightQrScreen() in
        // radar_screen.cpp) - keine eigene Foto-Logik noetig, FlightAware
        // zeigt beim Herunterscrollen selbst schon Route/Details/Foto.
        // Bewusst KEIN planespotters.net oder aehnliches - siehe
        // Projektentscheidung, das dort nirgends mehr einzubauen
        // (unzuverlaessig, zu viel Werbung). Nur bei echtem Rufzeichen
        // anzeigen (has_callsign), sonst wuerde der Link auf einen
        // Hex-Code zeigen und ins Leere fuehren.
        html += "var hasTrackLink=!!a.has_callsign;";
        html += "if(hasTrackLink){lines.push('<a href=\"https://flightaware.com/live/flight/'+encodeURIComponent(a.callsign)+'\" target=\"_blank\" rel=\"noopener\" id=\"acInfoTrack\">Track &amp; photo on FlightAware &rarr;</a>');}";
        html += "infoBox.innerHTML=lines.join('<br>')+'<br><a href=\"#\" id=\"acInfoClose\">Close</a>';";
        html += "infoBox.style.display='block';";
        // Sofortiges Feedback beim Antippen des FlightAware-Links, damit klar
        // ist, dass der Tipp angekommen ist, waehrend der neue Tab noch
        // aufgebaut wird - der eigentliche Grund fuer die vorherige
        // Unzuverlaessigkeit war aber refreshSelectedInfo() (siehe oben, war
        // frueher Teil von draw()), nicht eine fehlende Rueckmeldung. Setzt
        // sich von selbst zurueck, sobald die Infobox beim naechsten
        // Poll-Zyklus (alle 8s) oder durch erneutes Antippen neu aufgebaut
        // wird - kein manueller Reset noetig.
        html += "if(hasTrackLink){var trackLink=document.getElementById('acInfoTrack');";
        html += "trackLink.addEventListener('click',function(){trackLink.textContent='Seite wird geöffnet, bitte warten...';});}";
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

        // "Zuletzt aktualisiert vor Xs"-Anzeige (#radarFreshness, siehe
        // appendRadarSection() oben) - zaehlt per eigenem 1s-Intervall LIVE
        // mit, statt nur einmalig beim Laden gesetzt zu werden, und setzt
        // sich bei jedem ERFOLGREICHEN poll() zurueck (siehe dort). Sowie
        // der Verbindungsstatus-Punkt/-Text im Footer (#connDot/#connText,
        // siehe handleRoot()) - beide haengen am selben poll()-Erfolg/
        // Fehlschlag, daher hier gemeinsam verdrahtet statt zwei getrennter
        // Mechanismen.
        html += "var freshEl=document.getElementById('radarFreshness');";
        html += "function updateFreshness(){if(!freshEl)return;if(lastUpdateMs===null){freshEl.textContent='Waiting for first update...';return;}";
        html += "var s=Math.round((Date.now()-lastUpdateMs)/1000);freshEl.textContent='Updated '+(s<=1?'just now':s+'s ago');}";
        // WICHTIG: #connDot/#connText liegen im Footer, der im HTML ERST
        // NACH diesem <script>-Block folgt (Logbuch-Tabelle dazwischen) -
        // document.getElementById() darf deshalb NICHT einmalig beim Parsen
        // dieses Scripts aufgerufen werden (die Elemente existieren zu dem
        // Zeitpunkt noch gar nicht im DOM, das Ergebnis waere dauerhaft
        // null) - stattdessen bei JEDEM updateConnStatus()-Aufruf frisch
        // nachschlagen.
        html += "function updateConnStatus(ok){var connDot=document.getElementById('connDot'),connText=document.getElementById('connText');if(!connDot||!connText)return;";
        html += "connDot.style.background=ok?cssVar('--accent'):'#ff3b3b';connText.textContent=ok?'Connected':'No connection';}";
        // Watchlist-/Squawk-Wachposten-Badge (#watchBadge im Footer, siehe
        // handleRoot()) - zeigt/versteckt sich je nachdem, ob mindestens ein
        // Flugzeug im AKTUELLSTEN poll()-Ergebnis a.watched===true hat.
        // Gleiches Nachschlage-Muster wie updateConnStatus() (Element liegt
        // ebenfalls im spaeter folgenden Footer, nicht einmalig cachen).
        html += "function updateWatchBadge(aircraft){var el=document.getElementById('watchBadge');if(!el)return;";
        html += "var any=(aircraft||[]).some(function(a){return a.watched;});el.style.display=any?'inline':'none';}";
        html += "setInterval(updateFreshness,1000);";

        html += "if(rangeSel){rangeSel.addEventListener('change',poll);}";
        html += "function poll(){";
        html += "var url='/radar.json';";
        html += "if(rangeSel&&rangeSel.value){url+='?range_km='+rangeSel.value;}";
        html += "fetch(url).then(function(r){return r.json();}).then(function(data){";
        // Themenwechsel am Geraet (waehrend die Seite offen ist) live
        // uebernehmen - vergleicht data.theme_index gegen den zuletzt
        // bekannten Wert, ruft applyTheme() nur bei tatsaechlicher Aenderung
        // auf (unnoetiges Neusetzen der CSS-Variablen bei jedem Poll waere
        // harmlos, aber unnoetig).
        html += "if(data.theme_index!==undefined&&data.theme_index!==lastThemeIndex){lastThemeIndex=data.theme_index;applyTheme(data.theme_index);}";
        html += "draw(data);refreshSelectedInfo();updateWatchBadge(data.aircraft);updateMapMarkers(data);";
        html += "lastUpdateMs=Date.now();updateFreshness();updateConnStatus(true);";
        html += "}).catch(function(){status.textContent='Connection lost - retrying...';updateConnStatus(false);});";
        html += "}";
        html += "poll();";
        // Vorher alle 3s - staerker gedrosselt auf 8s (genau der Takt, in
        // dem sich AircraftTable auf dem Geraet ueberhaupt erst aendert,
        // siehe Config::FETCH_INTERVAL_MS in config.h), NACHDEM Alex ein
        // ernstes Problem gemeldet hat: bei laenger geoeffnetem WebUI-
        // Liveradar blieb die ADS-B-Aktualisierung auf dem Geraet komplett
        // stehen (Radarscreen + Naeherungs-LED blinkten minutenlang mit
        // einem laengst verschwundenen Flugzeug weiter), und normalisierte
        // sich sofort wieder, sobald die WebUI-Seite geschlossen wurde.
        // WebServer und die periodische ADS-B-Abfrage laufen beide im
        // selben NetTask auf Core 0 und teilen sich denselben knappen
        // WLAN-/Speicher-Spielraum des ESP32 (siehe auch WiFiClientSecure in
        // adsb_client.cpp) - ein Abfragetakt von 3s war schneller als der
        // Geraete-eigene Aktualisierungstakt von 8s und damit reine,
        // vermeidbare Zusatzlast genau in dem Moment, in dem das Einfrieren
        // auftrat. 8s deckt sich jetzt mit dem tatsaechlichen Update-Takt
        // des Geraets - schnelleres Pollen haette ohnehin nie neuere Daten
        // gezeigt.
        html += "setInterval(poll,8000);";
        // Schnellerer, rein lokaler Redraw-Takt (alle 150ms, ohne Netzwerk-
        // Anfrage) nur fuer das Sternenfunkeln + die Auswahlmarkierung -
        // gleiches Grundprinzip wie tick() vs. render() auf dem
        // Geraete-Display: Flugzeugpositionen aktualisieren sich weiterhin
        // nur alle 8s per poll(), die Sterne twinkeln aber fluessig dazwischen.
        html += "setInterval(function(){draw(lastData);},150);";

        // Kartenansicht (Leaflet + OpenStreetMap) - eigenstaendiger Block,
        // teilt sich aber lastData/altColor/fmtDist/cssVar mit dem
        // Radar-Canvas oben (gleiche IIFE-Closure), damit KEIN zweites,
        // unabhaengiges Polling noetig ist - updateMapMarkers() wird direkt
        // aus dem bestehenden poll()-Erfolgs-Zweig oben mitaufgerufen.
        // map() selbst wird bewusst ERST beim allerersten Umschalten auf
        // den "Map"-Tab angelegt (nicht schon beim Seitenaufbau) - Leaflet
        // kann seine Kachel-/Kartengroesse nicht zuverlaessig ermitteln,
        // solange der Container per "display:none" verborgen ist.
        html += "var map=null,aircraftLayer=null,homeMarker=null;";
        html += "function popupHtml(a){var lines=[];";
        html += "lines.push('<b>'+(a.callsign||a.hex)+'</b> ('+a.hex+')');";
        html += "lines.push('Altitude: '+Math.round(a.alt_ft)+' ft');";
        html += "if(a.speed_kt){lines.push('Speed: '+Math.round(a.speed_kt)+' kt');}";
        html += "lines.push('Distance: '+fmtDist(a.dist_km)+', bearing '+Math.round(a.bearing_deg)+'\\u00b0');";
        html += "lines.push('Heading: '+Math.round(a.track_deg)+'\\u00b0');";
        html += "if(a.squawk){lines.push('Squawk: '+a.squawk);}";
        html += "if(a.has_callsign){lines.push('<a href=\"https://flightaware.com/live/flight/'+encodeURIComponent(a.callsign)+'\" target=\"_blank\" rel=\"noopener\">Track &amp; photo on FlightAware &rarr;</a>');}";
        html += "return lines.join('<br>');}";
        // Kleiner, in Fluglinie zeigender Pfeil pro Flugzeug (analog zum
        // neuen Richtungs-Chevron auf dem Geraete-Radar) - per CSS
        // transform:rotate() gedreht, 0 Grad = Spitze nach Norden/oben,
        // deckungsgleich mit der Kompasskonvention aus track_deg
        // (0=Norden, im Uhrzeigersinn), kein Vorzeichen-Umrechnen noetig.
        // Bodenfahrzeuge bekommen dieselbe Form in Grau statt Hoehenfarbe -
        // rein kosmetisch, kein eigenes Symbol noetig fuer diese Ansicht.
        html += "function aircraftIcon(a){var color=a.ground_vehicle?'#aaaaaa':altColor(a.alt_ft);";
        html += "var html='<div style=\"width:18px;height:18px;transform:rotate('+(a.track_deg||0)+'deg);\">'+";
        html += "'<svg viewBox=\"0 0 18 18\" width=\"18\" height=\"18\"><polygon points=\"9,1 16,17 9,13 2,17\" fill=\"'+color+'\" stroke=\"#000\" stroke-width=\"0.5\"/></svg></div>';";
        html += "return L.divIcon({html:html,className:'',iconSize:[18,18],iconAnchor:[9,9]});}";
        html += "function updateMapMarkers(data){if(!map)return;";
        html += "aircraftLayer.clearLayers();";
        html += "(data.aircraft||[]).forEach(function(a){";
        html += "if(!a.lat&&!a.lon)return;";
        html += "L.marker([a.lat,a.lon],{icon:aircraftIcon(a)}).bindPopup(popupHtml(a)).addTo(aircraftLayer);";
        html += "});";
        html += "if(homeMarker&&(data.home_lat||data.home_lon)){homeMarker.setLatLng([data.home_lat,data.home_lon]);}";
        html += "}";
        html += "function initMap(){";
        html += "map=L.map('leafletMap');";
        // Offizieller OSM-Standard-Tile-Server mit korrekter Attribution
        // (OSM-Nutzungsbedingungen) - kein aggressives Vorausladen, Leaflet
        // holt Kacheln ausschliesslich bei tatsaechlichem Pan/Zoom, genau
        // wie beim Standardverhalten der Bibliothek vorgesehen.
        html += "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'&copy; <a href=\"https://www.openstreetmap.org/copyright\" target=\"_blank\" rel=\"noopener\">OpenStreetMap</a> contributors'}).addTo(map);";
        html += "aircraftLayer=L.layerGroup().addTo(map);";
        html += "var hLat=lastData.home_lat||0,hLon=lastData.home_lon||0;";
        html += "homeMarker=L.circleMarker([hLat,hLon],{radius:7,color:'#ffffff',weight:2,fillColor:cssVar('--accent')||'#39ff14',fillOpacity:1}).addTo(map);";
        html += "homeMarker.bindTooltip('Home');";
        // Anfangs-Zoom NUR hier beim allerersten Anlegen der Karte
        // (initMap() laeuft laut obigem Kommentar genau einmal) automatisch
        // an die tatsaechlichen Daten anpassen, statt einer festen
        // Zoomstufe (Alex' Meldung: bei 25km Reichweite und nur 3
        // sichtbaren Flugzeugen musste man vorher 5x manuell herauszoomen).
        // updateMapMarkers() ruehrt die Kartenansicht selbst NIE an
        // (siehe dort) - spaeteres manuelles Zoomen/Pannen des Nutzers
        // bleibt dadurch bei jedem weiteren Datenupdate unangetastet.
        html += "var initialAc=(lastData.aircraft||[]).filter(function(a){return a.lat||a.lon;});";
        html += "if(initialAc.length>0){";
        // fitBounds() ueber Home-Marker UND alle gerade sichtbaren
        // Flugzeuge - deckt genau den Fall ab, den Alex gemeldet hat.
        // maxZoom verhindert ein zu starkes Heranzoomen, falls alle
        // Flugzeuge zufaellig dicht beieinander (nahe am Home-Marker)
        // liegen.
        html += "var pts=[[hLat,hLon]];";
        html += "initialAc.forEach(function(a){pts.push([a.lat,a.lon]);});";
        html += "map.fitBounds(pts,{padding:[30,30],maxZoom:13});";
        html += "}else{";
        // Fallback bei leerem Himmel: Zoom passend zur eingestellten
        // Radar-Reichweite waehlen - ein (nie tatsaechlich gezeichneter)
        // Kreis mit range_km Radius um den Home-Marker liefert per
        // getBounds() genau die Flaeche, auf die fitBounds() dann zoomt.
        html += "var rKm=lastData.range_km||25;";
        html += "map.fitBounds(L.circle([hLat,hLon],{radius:rKm*1000}).getBounds(),{padding:[10,10]});";
        html += "}";
        html += "updateMapMarkers(lastData);";
        html += "}";

        // Tab-Umschalter Radar/Map - zeigt/versteckt die beiden Ansichten,
        // legt die Karte beim allerersten Wechsel dorthin an (siehe
        // initMap()-Kommentar oben) und ruft danach nur noch
        // invalidateSize() auf (Leaflet muss seine Kachel-Groesse neu
        // berechnen, sobald der zuvor verborgene Container wieder sichtbar
        // wird, sonst bleiben Kacheln teilweise grau).
        html += "var radarViewEl=document.getElementById('radarView'),mapViewEl=document.getElementById('mapView');";
        html += "var tabRadar=document.getElementById('tabRadar'),tabMap=document.getElementById('tabMap');";
        html += "tabRadar.addEventListener('click',function(){radarViewEl.style.display='block';mapViewEl.style.display='none';tabRadar.classList.add('active');tabMap.classList.remove('active');});";
        html += "tabMap.addEventListener('click',function(){radarViewEl.style.display='none';mapViewEl.style.display='block';tabMap.classList.add('active');tabRadar.classList.remove('active');";
        html += "if(!map){initMap();}else{map.invalidateSize();}";
        html += "});";

        html += "})();</script>";
    }

    void handleRoot() {
        FlightLogbook::DayEntry days[MAX_DAYS_QUERIED];
        uint8_t dayCount = FlightLogbook::listDays(days, MAX_DAYS_QUERIED);

        String html = htmlHeader("Eiswolfs Flightradar");

        appendRadarSection(html);
        // Logbuch-Tabelle danach ist variabel lang (bis MAX_DAYS_QUERIED
        // Tage) - hier nochmal in einem Rutsch nachreservieren statt vieler
        // weiterer kleiner Reallozierungen weiter unten (siehe Kommentar in
        // htmlHeader() zum selben Hintergrund).
        html.reserve(html.length() + 4096);

        html += "<nav><a href=\"/lists\">Manage Airline Filter &amp; Watchlist &rarr;</a></nav>";

        html += "<h2>Flight Logbook</h2>";
        if (dayCount == 0) {
            html += "<p>No logbook entries yet.</p>";
        } else {
            // Sofortige optische Rueckmeldung bei Delete/Download (Alex'
            // Meldung: SD-Kartenzugriff dauert spuerbar, ohne Rueckmeldung
            // klickt man verwirrt mehrfach nach - Mehrfachklicks auf Delete
            // koennten sogar mehrfache Loesch-Anfragen ausloesen). Gleiches
            // Grundprinzip wie die bestehende "#acInfo a:active"-Regel oben
            // (siehe dortiger Kommentar) - hier zusaetzlich per JS, weil ein
            // rein CSS-basiertes ":active" verschwindet, sobald man den
            // Finger/die Maustaste loslaesst, aber die eigentliche Wartezeit
            // (SD-Zugriff bzw. Download-Vorbereitung) laenger dauert als der
            // Tap selbst.
            // prepareDelete(): deaktiviert den Button SOFORT und aendert den
            // Text - der anschliessende 303-Redirect von /logbook/delete
            // (siehe handleLogbookDelete()) laedt die Seite ohnehin komplett
            // neu, das "Deaktiviert"-Aussehen bleibt also automatisch bis
            // zur aktualisierten Liste bestehen, kein weiterer Code noetig.
            // prepareDownload(): der Download selbst (Content-Disposition:
            // attachment, siehe handleCsvDownload()) verlaesst die Seite
            // NICHT - der Text wird deshalb per Timeout wieder zurueckgesetzt,
            // da es keine verlaessliche "Download fertig"-Callback-Methode
            // fuer einen einfachen <a>-Klick gibt.
            html += "<script>";
            html += "function prepareDelete(f){var b=f.querySelector('button');b.disabled=true;b.textContent='Deleting...';return true;}";
            html += "function prepareDownload(a){if(a.dataset.busy)return false;a.dataset.busy='1';";
            html += "var orig=a.textContent;a.textContent='Preparing...';a.style.opacity='.6';";
            html += "setTimeout(function(){a.textContent=orig;a.style.opacity='';delete a.dataset.busy;},1500);return true;}";
            // Suchfeld + sortierbare Spaltenkoepfe (Alex' Wunsch, "sobald die
            // Liste mit der Zeit laenger wird") - rein clientseitig auf der
            // bereits server-gerenderten Tabelle (kein zusaetzlicher
            // Netzwerk-/SD-Zugriff fuers Filtern/Sortieren noetig, die
            // komplette Liste steht ja schon im DOM). sortLog() liest/
            // schreibt data-sort-col/-dir am <table>-Element, um bei
            // wiederholtem Klick auf dieselbe Spalte zwischen auf-/
            // absteigend umzuschalten. Zahlen (Aircraft-Spalte) werden
            // numerisch verglichen, alles andere (Datum) als String -
            // reicht fuer die "JJJJ-MM-TT"-Formate von FlightLogbook, auch
            // mit dem "_2"-Suffix bei mehreren Sitzungen am selben Tag.
            html += "<input type=\"text\" id=\"logSearch\" placeholder=\"Filter by date...\" style=\"margin-bottom:8px;width:100%;max-width:300px;box-sizing:border-box;\">";
            html += "<script>";
            html += "function sortLog(col){var t=document.getElementById('logTable');var tbody=t.tBodies[0];";
            html += "var rows=Array.prototype.slice.call(tbody.rows);";
            html += "var asc=t.dataset.sortCol==String(col)?t.dataset.sortDir!=='asc':true;";
            html += "rows.sort(function(a,b){var av=a.cells[col].textContent.trim(),bv=b.cells[col].textContent.trim();";
            html += "var an=parseFloat(av),bn=parseFloat(bv);";
            html += "var cmp=(!isNaN(an)&&!isNaN(bn))?(an-bn):av.localeCompare(bv);return asc?cmp:-cmp;});";
            html += "rows.forEach(function(r){tbody.appendChild(r);});";
            html += "t.dataset.sortCol=col;t.dataset.sortDir=asc?'asc':'desc';}";
            html += "document.getElementById('logSearch').addEventListener('input',function(){";
            html += "var q=this.value.toLowerCase();var t=document.getElementById('logTable');";
            html += "Array.prototype.forEach.call(t.tBodies[0].rows,function(r){";
            html += "r.style.display=r.cells[0].textContent.toLowerCase().indexOf(q)>=0?'':'none';});});";
            html += "</script>";
            html += "<p><a class=\"dl\" href=\"/export.csv\" onclick=\"return prepareDownload(this);\">Download merged CSV (all days)</a></p>";
            html += "<table id=\"logTable\"><tr>";
            html += "<th onclick=\"sortLog(0);\" style=\"cursor:pointer;\">Date &#8645;</th>";
            html += "<th onclick=\"sortLog(1);\" style=\"cursor:pointer;\">Aircraft &#8645;</th>";
            html += "<th></th><th></th></tr>";
            for (uint8_t i = 0; i < dayCount; i++) {
                String date = String(days[i].date);
                html += "<tr><td>" + date + "</td><td>" + String(days[i].count) + "</td>";
                html += "<td><a class=\"dl\" href=\"/csv?date=" + date + "\" onclick=\"return prepareDownload(this);\">Download</a></td>";
                html += "<td><form method=\"POST\" action=\"/logbook/delete\" onsubmit=\"return prepareDelete(this);\">";
                html += "<input type=\"hidden\" name=\"date\" value=\"" + date + "\">";
                html += "<button type=\"submit\">Delete</button></form></td></tr>";
            }
            html += "</table>";
        }

        // Dezenter Footer (Alex' Wunsch): Firmware-Version - dieselbe
        // Config::APP_VERSION, die auch der OTA-Update-Check auf dem Geraet
        // selbst vergleicht (siehe menu_screen.cpp) - server-seitig fest in
        // die Seite eingebettet (aendert sich waehrend eine Seite offen ist
        // ohnehin nicht, kein JS/Fetch dafuer noetig), sowie ein LIVE
        // Verbindungsstatus-Punkt+Text (#connDot/#connText), der an
        // denselben poll()-Erfolg/Fehlschlag haengt wie die "zuletzt
        // aktualisiert"-Anzeige oben (siehe updateConnStatus() in
        // appendRadarSection()). Start-Zustand grau/"Checking...", bis der
        // allererste poll() durchgelaufen ist.
        html += "<footer style=\"margin-top:24px;padding-top:10px;border-top:1px solid var(--accent-border);font-size:11px;color:var(--accent-muted);\">";
        html += "Eiswolfs Flightradar v" + String(Config::APP_VERSION) + " &middot; ";
        html += "<span id=\"connDot\" style=\"display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--accent-muted);margin-right:4px;\"></span>";
        html += "<span id=\"connText\">Checking...</span>";
        // Watchlist-/Squawk-Wachposten-Badge (Alex' Wunsch) - zeigt an, ob
        // GERADE (im aktuellsten poll()-Ergebnis) mindestens ein Flugzeug
        // sichtbar ist, das entweder auf der Rufzeichen-Watchlist steht oder
        // einem hinterlegten Wach-Squawk entspricht (a.watched deckt beides
        // ab, siehe handleRadarJson() - dort jetzt auch SquawkWatchlist mit
        // eingerechnet, vorher fehlte das). Bewusst Cyan (#00e5ff) statt der
        // Thema-Akzentfarbe - dieselbe feste, NICHT themenabhaengige Farbe
        // wie der Beobachtungs-Ring um den Marker selbst im Radar-Canvas
        // (semantischer Alarm-/Status-Ton, kein UI-Chrome). Standardmaessig
        // versteckt, erscheint nur bei tatsaechlichem Treffer.
        html += " &middot; <span id=\"watchBadge\" style=\"display:none;color:#00e5ff;\">&#9679; Watchlist match</span>";
        html += "</footer>";

        html += "</div></body></html>";
        // TESTWEISE: Diagnose fuer den Map-Tab-Bug (fehlgeschlagener
        // Leaflet-<script>-Tag) - falls das Problem trotz der Vorab-
        // Reservierung oben nochmal auftritt, zeigt dieser Log-Eintrag, ob
        // html.length() plausibel ist und wie knapp der freie Heap zum
        // Sendezeitpunkt tatsaechlich war. Wieder entfernen, sobald der Fix
        // bestaetigt ist.
        Serial.printf("[WebUI] handleRoot: html.length()=%u freeHeap=%u\n",
                      (unsigned)html.length(), (unsigned)ESP.getFreeHeap());
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

        html += "</div></body></html>";
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
        lastRadarJsonRequestMs = millis();
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
        bool onlyHeli = SettingsStore::onlyHelicopters();
        bool emergencyOn = SettingsStore::emergencyAlertEnabled();

        JsonDocument doc;
        doc["range_km"] = rangeKm;
        // Fuer die neue Kartenansicht (Leaflet, siehe appendRadarSection()) -
        // der eigene Standort als Kartenmittelpunkt/Home-Marker. Wird bei
        // jedem Poll mitgeschickt (nicht nur einmalig beim Seitenaufbau),
        // falls sich der aktive Standort waehrend die Seite offen ist
        // aendert (z.B. Standort-Preset am Geraet gewechselt).
        {
            double homeLat = 0, homeLon = 0;
            LocationManager::getHomeLocation(homeLat, homeLon);
            doc["home_lat"] = homeLat;
            doc["home_lon"] = homeLon;
        }
        // Fuer das WebUI-Farbthema (Alex' Wunsch) - dieselbe SettingsStore::
        // radarThemeIndex(), aus der auch WebTheme::currentWebTheme() beim
        // Seitenaufbau die :root-CSS-Variablen ableitet (siehe htmlHeader()).
        // Hier zusaetzlich im laufenden Poll mitgeschickt, damit ein
        // Themenwechsel am Geraet auch bei bereits offener Seite live
        // uebernommen wird (siehe applyTheme() in appendRadarSection()),
        // ohne dass die Seite neu geladen werden muss.
        doc["theme_index"] = SettingsStore::radarThemeIndex();
        // Regen-Overlay im WebUI-Radar (Alex' Wunsch: "Spiegel des CYD-
        // Radarscreens") - EXAKT dieselbe Bedingung UND Windrichtungs-Logik
        // wie beim Radarscreen-Regen (radar_screen.cpp::spawnRainDrop()):
        // Schalter an, Weather::current() zeigt Regen/Gewitter, UND eine
        // gueltige Windrichtung ist bekannt (windDir >= 0.0f, sonst bleibt
        // wind_dir_deg auf dem letzten bekannten Wert stehen bzw. -1 vor der
        // ersten erfolgreichen Wetterabfrage - siehe Weather::update()).
        // wind_dir_deg wird IMMER mitgeschickt (auch wenn gerade nicht
        // regnet), das Client-JS nutzt es nur, wenn data.raining true ist.
        // rain_intensity (0=leicht/1=mittel/2=stark, Weather::RainIntensity)
        // steuert Tropfenzahl/Fallgeschwindigkeit im Client-JS (rainParamsFor()
        // dort) - dieselben drei Stufen wie radar_screen.cpp/main.cpp,
        // siehe Kommentar bei rainParamsForIntensity() in radar_screen.cpp.
        {
            Weather::Condition cond = Weather::current();
            float windDir = Weather::currentWindDirectionDeg();
            doc["raining"] = SettingsStore::rainEffectEnabled() &&
                              (cond == Weather::Condition::Rain || cond == Weather::Condition::Thunderstorm) &&
                              windDir >= 0.0f;
            doc["wind_dir_deg"] = windDir;
            doc["rain_intensity"] = (int)Weather::currentRainIntensity(); // 0=None,1=Light,2=Moderate,3=Heavy
            // Schnee-Overlay (Alex' Wunsch, analog zum Regen-Overlay oben) -
            // haengt am selben Schalter wie Regen (SettingsStore::
            // rainEffectEnabled(), "Wetter anzeigen") und derselben
            // RainIntensity-Stufung wie Weather::currentSnowIntensity() am
            // Geraet (siehe ScreensaverSnow::snowParamsForIntensity() in
            // main.cpp). Anders als Regen KEINE Windrichtung noetig (Schnee
            // faellt beim Geraete-Ruhebildschirm/WebUI gerade, nur mit
            // seitlichem Wackeln - kein Wind-Vektor). Wird NICHT auf dem
            // Radar-Canvas selbst gezeichnet (dort bleibt nur der Regen,
            // "Spiegel des CYD-Radarscreens"), sondern auf dem separaten
            // Vollbild-Sternenhintergrund (#star-bg, siehe
            // appendStarBackground()) rund um den Radarkreis - das
            // Client-JS dort liest dieses Feld ueber "window.__radarData"
            // mit (siehe appendRadarSection()/appendStarBackground()).
            doc["snowing"] = SettingsStore::rainEffectEnabled() &&
                              cond == Weather::Condition::Snow;
            doc["snow_intensity"] = (int)Weather::currentSnowIntensity(); // 0=None,1=Light,2=Moderate,3=Heavy
        }
        JsonArray arr = doc["aircraft"].to<JsonArray>();

        AircraftTable::lock();
        Aircraft* table = AircraftTable::raw();
        for (uint8_t i = 0; i < AircraftTable::capacity(); i++) {
            Aircraft& a = table[i];
            if (!a.valid) continue;
            if (a.distanceKm > rangeKm * 1.05f) continue;

            bool isGroundVehicle = a.category[0] == 'C';
            if (hideGround && isGroundVehicle) continue;

            bool isRotorcraft = a.category[0] == 'A' && a.category[1] == '7';
            if (onlyHeli && !isRotorcraft) continue;

            if (AirlineFilter::isHidden(a.callsign)) continue;

            bool isHeavy = isHeavyCategoryWeb(a.category);
            bool isEmergency = emergencyOn && isEmergencySquawkWeb(a.squawk);
            // Deckt jetzt beide Watchlist-Mechanismen ab, genau wie am
            // Geraete-Display (radar_screen.cpp) - vorher fehlte hier die
            // Squawk-Wachliste komplett, ein Flugzeug, das nur ueber seinen
            // Squawk-Code (nicht das Rufzeichen) beobachtet wird, waere im
            // WebUI faelschlich als "nicht beobachtet" erschienen.
            bool isWatched = AircraftWatchlist::isWatched(a.callsign) || SquawkWatchlist::isWatched(a.squawk);
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
            // Fuer die Kartenansicht (siehe appendRadarSection()) - echte
            // WGS84-Koordinaten, unabhaengig von der Peilung/Distanz-Polar-
            // Darstellung des Radar-Canvas oben. Steht im Aircraft-Snapshot
            // ohnehin schon zur Verfuegung (aus dem ADS-B-Feed), kein
            // zusaetzlicher Rechen-/Netzwerkaufwand.
            o["lat"] = a.lat;
            o["lon"] = a.lon;
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
            // Unterscheidet ein echtes Rufzeichen vom Hex-Code-Fallback in
            // "callsign" oben - der FlightAware-Tracking-Link im Info-Panel
            // (siehe showInfo() weiter unten) braucht ein echtes Rufzeichen,
            // sonst fuehrt der Link ins Leere (genau wie beim QR-Button am
            // Geraete-Display, der aus demselben Grund nur bei a.callsign[0]
            // ueberhaupt angezeigt wird, siehe radar_screen.cpp).
            o["has_callsign"] = a.callsign[0] != 0;
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

bool isRadarUiActive() {
    return lastRadarJsonRequestMs != 0 &&
           (millis() - lastRadarJsonRequestMs) < RADAR_UI_ACTIVE_WINDOW_MS;
}

}
