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
#include "sun_times.h"
#include "web_pwa_icon.h"
#include "web_avatar_logo.h"
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

    // Debouncing fuer den Fernsteuerungs-Endpunkt /control/range (siehe
    // Design-Absprache im Chat) - SettingsStore::setRangeIndex() schreibt
    // bei JEDEM Aufruf die komplette Einstellungsdatei auf die SD-Karte neu
    // (teuer, siehe frueherer Watchdog-Vorfall bei zu haeufigen Settings-
    // Schreibvorgaengen). Bewusst NUR hier an der Web-Schicht gedrosselt,
    // NICHT im Setter selbst - der bleibt fuer den physischen Bedienpfad
    // (radar_screen.cpp) unveraendert und wird bei einer tatsaechlich
    // angenommenen Web-Aenderung 1:1 genauso aufgerufen wie bei einem Tap
    // am Geraet (identische Persistierung).
    uint32_t lastRangeCommandMs = 0;
    constexpr uint32_t MIN_CONTROL_INTERVAL_MS = 400;

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

    // Militaer-/Behoerdenflug-Erkennung ueber Squawk-Code-Bereiche -
    // gleiche Duplikations-Begruendung wie bei isEmergencySquawkWeb() oben,
    // Bereiche 1:1 aus radar_screen.cpp's MILITARY_SQUAWK_RANGES/
    // isMilitaryGovSquawk() uebernommen (NORAD USA/Kanada + Australien,
    // siehe dortiger ausfuehrlicher Kommentar zur Herkunft der einzelnen
    // Bereiche). Faerbt den "auffaellig"-Ring (oranger Ring, "notable" im
    // JSON) fuer die betroffenen Flugzeuge ein - Rufzeichen-Praefixe
    // (isNotableCallsign() am Geraet) sind hier bewusst NICHT nachgebaut,
    // da die zugehoerige Praefixliste im Projekt nirgends existiert (siehe
    // radar_screen.cpp, isNotableCallsign() liefert dort ebenfalls immer
    // false).
    struct SquawkRangeWeb { uint16_t lo, hi; };
    constexpr SquawkRangeWeb MILITARY_SQUAWK_RANGES_WEB[] = {
        {4400, 4477},
        {5000, 5000},
        {5400, 5400},
        {6000, 6000},
        {6100, 6100},
        {6400, 6400},
        {7501, 7577},
    };
    constexpr uint8_t MILITARY_SQUAWK_RANGE_WEB_COUNT =
        sizeof(MILITARY_SQUAWK_RANGES_WEB) / sizeof(MILITARY_SQUAWK_RANGES_WEB[0]);

    bool isMilitaryGovSquawkWeb(const char* squawk) {
        if (!squawk[0]) return false;
        uint16_t val = (uint16_t)atoi(squawk);
        for (uint8_t i = 0; i < MILITARY_SQUAWK_RANGE_WEB_COUNT; i++) {
            if (val >= MILITARY_SQUAWK_RANGES_WEB[i].lo && val <= MILITARY_SQUAWK_RANGES_WEB[i].hi) return true;
        }
        return false;
    }

    bool isHeavyCategoryWeb(const char* category) {
        return category[0] == 'A' && category[1] == '5';
    }

    // Gleiche Logik wie radar_screen.cpp::isNightHours() - Nacht = zwischen
    // Sonnenuntergang und Sonnenaufgang am aktiven Standort (SunTimes::
    // compute()), mit Rueckfall auf ein festes 22:00-06:00-Fenster, solange
    // Standort/Uhrzeit noch nicht bekannt sind. Gleiche Duplikations-
    // Begruendung wie isEmergencySquawkWeb()/isMilitaryGovSquawkWeb() oben.
    bool isNightHoursWeb() {
        time_t now = time(nullptr);
        if (now <= 8 * 3600 * 2) return false; // Uhrzeit noch nicht per NTP synchronisiert

        struct tm tmNow;
        localtime_r(&now, &tmNow);

        double lat = 0, lon = 0;
        LocationManager::getHomeLocation(lat, lon);
        if (lat != 0.0 || lon != 0.0) {
            SunTimes::Result sun = SunTimes::compute(lat, lon, tmNow.tm_year + 1900, tmNow.tm_mon + 1,
                                                      tmNow.tm_mday, LocationManager::utcOffsetSeconds());
            if (sun.valid) {
                if (sun.alwaysDay) return false;
                if (sun.alwaysNight) return true;
                float hourNow = tmNow.tm_hour + tmNow.tm_min / 60.0f;
                return (hourNow < sun.sunriseHour) || (hourNow >= sun.sunsetHour);
            }
        }

        int hour = tmNow.tm_hour;
        return (hour >= 22 || hour < 6);
    }

    // Typ-Silhouette-Klassifizierung (Linienflugzeug/Privatjet/Turboprop)
    // fuer die Marker-Form auf der Live-Radar-Webseite - gleiche
    // Duplikations-Begruendung wie isEmergencySquawkWeb()/
    // isMilitaryGovSquawkWeb() oben, Praefix-Tabelle UND die beiden Neo-/
    // MAX-Sonderfaelle 1:1 aus radar_screen.cpp's TYPE_SILHOUETTE_TABLE/
    // classifyTypeSilhouette() uebernommen. Ergebnis wird unten in
    // handleRadarJson() als einfacher String ("airliner"/"privatejet"/
    // "turboprop"/"unknown") ins JSON geschrieben, statt eines Enums -
    // einfacher fuers JavaScript auf der Empfaengerseite zu konsumieren.
    enum class TypeSilhouetteWeb { Unknown, Airliner, PrivateJet, Turboprop };

    struct TypePrefixEntryWeb {
        const char* prefix;
        TypeSilhouetteWeb cls;
    };

    constexpr TypePrefixEntryWeb TYPE_SILHOUETTE_TABLE_WEB[] = {
        // -- Privatjets --
        {"LJ",   TypeSilhouetteWeb::PrivateJet},
        {"C25",  TypeSilhouetteWeb::PrivateJet},
        {"C5",   TypeSilhouetteWeb::PrivateJet},
        {"C6",   TypeSilhouetteWeb::PrivateJet},
        {"C7",   TypeSilhouetteWeb::PrivateJet},
        {"GLF",  TypeSilhouetteWeb::PrivateJet},
        {"G280", TypeSilhouetteWeb::PrivateJet},
        {"G650", TypeSilhouetteWeb::PrivateJet},
        {"H25",  TypeSilhouetteWeb::PrivateJet},
        {"BE40", TypeSilhouetteWeb::PrivateJet},
        {"FA7",  TypeSilhouetteWeb::PrivateJet},
        {"FA8",  TypeSilhouetteWeb::PrivateJet},
        {"F900", TypeSilhouetteWeb::PrivateJet},
        {"F2TH", TypeSilhouetteWeb::PrivateJet},
        {"CL30", TypeSilhouetteWeb::PrivateJet},
        {"CL60", TypeSilhouetteWeb::PrivateJet},
        {"GLEX", TypeSilhouetteWeb::PrivateJet},
        {"GL5T", TypeSilhouetteWeb::PrivateJet},
        {"GL6T", TypeSilhouetteWeb::PrivateJet},
        {"E50P", TypeSilhouetteWeb::PrivateJet},
        {"E55P", TypeSilhouetteWeb::PrivateJet},
        {"PC24", TypeSilhouetteWeb::PrivateJet},

        // -- Turboprops --
        {"AT4",  TypeSilhouetteWeb::Turboprop},
        {"AT7",  TypeSilhouetteWeb::Turboprop},
        {"DH8",  TypeSilhouetteWeb::Turboprop},
        {"SF34", TypeSilhouetteWeb::Turboprop},
        {"SB20", TypeSilhouetteWeb::Turboprop},
        {"BE20", TypeSilhouetteWeb::Turboprop},
        {"BE30", TypeSilhouetteWeb::Turboprop},
        {"BE9L", TypeSilhouetteWeb::Turboprop},
        {"B350", TypeSilhouetteWeb::Turboprop},
        {"C208", TypeSilhouetteWeb::Turboprop},
        {"PC12", TypeSilhouetteWeb::Turboprop},
        {"DHC6", TypeSilhouetteWeb::Turboprop},
        {"SW4",  TypeSilhouetteWeb::Turboprop},
        {"F50",  TypeSilhouetteWeb::Turboprop},
        {"L410", TypeSilhouetteWeb::Turboprop},

        // -- Airliner --
        {"A3",   TypeSilhouetteWeb::Airliner},
        {"B7",   TypeSilhouetteWeb::Airliner},
        {"MD",   TypeSilhouetteWeb::Airliner},
        {"CRJ",  TypeSilhouetteWeb::Airliner},
        {"E1",   TypeSilhouetteWeb::Airliner},
        {"E29",  TypeSilhouetteWeb::Airliner},
        {"SU9",  TypeSilhouetteWeb::Airliner},
        {"BCS",  TypeSilhouetteWeb::Airliner},
    };
    constexpr uint8_t TYPE_SILHOUETTE_WEB_COUNT =
        sizeof(TYPE_SILHOUETTE_TABLE_WEB) / sizeof(TYPE_SILHOUETTE_TABLE_WEB[0]);

    bool isAirbusNeoCodeWeb(const char* t) {
        return t[0] == 'A' && t[1] >= '1' && t[1] <= '3' &&
               t[2] >= '0' && t[2] <= '9' && t[3] == 'N' && t[4] == '\0';
    }

    bool isBoeingMaxCodeWeb(const char* t) {
        return t[0] == 'B' && t[1] == '3' && t[2] >= '0' && t[2] <= '9' &&
               t[3] == 'M' && t[4] == '\0';
    }

    TypeSilhouetteWeb classifyTypeSilhouetteWeb(const char* typeCode) {
        if (!typeCode[0]) return TypeSilhouetteWeb::Unknown;
        size_t len = strlen(typeCode);
        if (len == 4 && (isAirbusNeoCodeWeb(typeCode) || isBoeingMaxCodeWeb(typeCode))) {
            return TypeSilhouetteWeb::Airliner;
        }
        for (uint8_t i = 0; i < TYPE_SILHOUETTE_WEB_COUNT; i++) {
            size_t plen = strlen(TYPE_SILHOUETTE_TABLE_WEB[i].prefix);
            if (len >= plen && strncmp(typeCode, TYPE_SILHOUETTE_TABLE_WEB[i].prefix, plen) == 0) {
                return TYPE_SILHOUETTE_TABLE_WEB[i].cls;
            }
        }
        return TypeSilhouetteWeb::Unknown;
    }

    const char* typeSilhouetteWebLabel(TypeSilhouetteWeb cls) {
        switch (cls) {
            case TypeSilhouetteWeb::Airliner:   return "airliner";
            case TypeSilhouetteWeb::PrivateJet: return "privatejet";
            case TypeSilhouetteWeb::Turboprop:  return "turboprop";
            case TypeSilhouetteWeb::Unknown:
            default:                             return "unknown";
        }
    }

    // String-Version von Weather::Condition fuers JSON (handleRadarJson()
    // unten) - fuer "weather_condition" UND "forecast_condition", das
    // Client-JS steuert damit sowohl das Wetter-Icon als auch den
    // Vorhersage-Text im Info-Popup. Gleiche 6 Werte wie
    // Weather::Condition, "unknown" fuer Condition::Unknown (noch keine
    // erfolgreiche Abfrage) - das Icon zeichnet dann bewusst nichts,
    // genau wie main.cpp::drawWeatherIcon() am Geraet.
    const char* weatherConditionWebLabel(Weather::Condition c) {
        switch (c) {
            case Weather::Condition::Clear:        return "clear";
            case Weather::Condition::PartlyCloudy: return "partly_cloudy";
            case Weather::Condition::Cloudy:       return "cloudy";
            case Weather::Condition::Rain:         return "rain";
            case Weather::Condition::Snow:         return "snow";
            case Weather::Condition::Thunderstorm: return "thunderstorm";
            case Weather::Condition::Unknown:
            default:                                return "unknown";
        }
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
        // Werte/Form 1:1 an ScreensaverSnow (main.cpp) angeglichen - dort
        // dupliziert statt geteilt (CLAUDE.md-Konvention), bitte bei
        // Aenderungen synchron halten. SNOW_MAX = groesste Stufe ("stark").
        html += "var SNOW_MAX=20;var snowFlakes=[];var snowInited=false;var lastSnowMs=null;";
        html += "function snowParamsFor(level){if(level>=3)return{count:20,speed:35};if(level===1)return{count:6,speed:15};return{count:12,speed:25};}";
        html += "function snowSpawn(f){f.baseX=Math.random()*canvas.width;f.y=-Math.random()*canvas.height/2;f.phase=Math.random()*6.28;}";
        // "Dendrit"-Form (main.cpp::drawFlake()) statt eines einfachen
        // Punkts - sechsstrahliger Stern aus 3 Hauptlinien (0/60/120 Grad)
        // mit je einem Aestchen an jeder der 6 Spitzen, gleiche Masse wie
        // am Geraet (Arm 3px, Aestchen 2px, 30 Grad Abwinkelung).
        html += "function drawSnowFlake(x,y){var ARM=3,BR=2,ANG=0.5236;";
        html += "for(var i=0;i<3;i++){var angle=i*(Math.PI/3);";
        html += "var dx=Math.cos(angle)*ARM,dy=Math.sin(angle)*ARM;";
        html += "var x1=x+dx,y1=y+dy,x2=x-dx,y2=y-dy;";
        html += "ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.stroke();";
        html += "var b1x=Math.cos(angle+ANG)*BR,b1y=Math.sin(angle+ANG)*BR;";
        html += "ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x1+b1x,y1+b1y);ctx.stroke();";
        html += "var opp=angle+Math.PI;var b2x=Math.cos(opp+ANG)*BR,b2y=Math.sin(opp+ANG)*BR;";
        html += "ctx.beginPath();ctx.moveTo(x2,y2);ctx.lineTo(x2+b2x,y2+b2y);ctx.stroke();}}";
        html += "function drawSnow(level){var p=snowParamsFor(level);";
        html += "var now=performance.now();var dt=lastSnowMs?Math.min(now-lastSnowMs,300):16;lastSnowMs=now;var step=p.speed*dt/1000;";
        html += "if(!snowInited){snowFlakes=[];for(var i=0;i<SNOW_MAX;i++){var f={};snowSpawn(f);snowFlakes.push(f);}snowInited=true;}";
        html += "ctx.save();ctx.strokeStyle='#ffffff';ctx.lineWidth=1;";
        html += "for(var i=0;i<snowFlakes.length;i++){var f=snowFlakes[i];f.y+=step;f.phase+=2.5*dt/1000;";
        html += "if(f.y-6>canvas.height){snowSpawn(f);}";
        html += "if(i>=p.count)continue;";
        html += "var x=f.baseX+10*Math.sin(f.phase);";
        html += "drawSnowFlake(x,f.y);}";
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
        // Feature 12 "PWA fuer die Web-UI" - macht die Seite auf dem
        // Smartphone als eigenstaendige App installierbar (Icon auf dem
        // Homescreen, Start im Vollbild ohne Adressleiste). "theme-color"
        // folgt dynamisch dem AKTUELL gewaehlten Geraete-Farbthema (wt.accent,
        // siehe currentWebTheme() oben) - dieselbe Instanz, die auch die
        // CSS-Variablen weiter unten setzt. Die "apple-mobile-web-app-*"-
        // Metas sind Safaris ALTE, aber weiterhin noetige Variante von
        // manifest.json's "display":"standalone" - ohne sie ignoriert iOS
        // Safari den Manifest-Eintrag beim "Zum Home-Bildschirm hinzufuegen"
        // komplett und oeffnet stattdessen weiterhin einen normalen
        // Browser-Tab (siehe handleManifest()/handleIcon() unten fuer die
        // beiden referenzierten Routen).
        html += "<link rel=\"manifest\" href=\"/manifest.json\">";
        html += "<meta name=\"theme-color\" content=\"" + String(wt.accent) + "\">";
        html += "<link rel=\"icon\" href=\"/icon.png\" type=\"image/png\">";
        html += "<link rel=\"apple-touch-icon\" href=\"/icon.png\">";
        html += "<meta name=\"mobile-web-app-capable\" content=\"yes\">";
        html += "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">";
        html += "<meta name=\"apple-mobile-web-app-status-bar-style\" content=\"black-translucent\">";
        html += "<meta name=\"apple-mobile-web-app-title\" content=\"Flightradar\">";
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
        // Wetter-Info-Popup (siehe weatherIcon-Canvas unten) - exakt derselbe
        // visuelle Stil wie #acInfo oben (Alex' Vorgabe), eigene ID statt
        // Wiederverwendung von #acInfo, damit sich beide Popups nicht
        // gegenseitig ueberschreiben/verstecken (Flugzeug-Auswahl und
        // Wetter-Icon sind unabhaengige Interaktionen).
        html += "#weatherInfo{display:none;max-width:400px;margin-top:8px;padding:8px 10px;border:1px solid var(--accent);border-radius:6px;font-size:13px;line-height:1.7;}";
        html += "#weatherInfo a{color:#ff3b3b;text-decoration:none;border:1px solid #ff3b3b;border-radius:4px;padding:2px 8px;display:inline-block;margin-top:4px;}";
        html += "#weatherInfo a:active{background:#ff3b3b;color:#0a0f0d;}";
        // Umschalter "Radar"/"Map" (siehe appendRadarSection()) - gleicher
        // Button-Stil wie die restliche Seite, aktiver Tab invertiert
        // (gefuellte Akzentfarbe, dunkler Text), genau wie aktive Eintraege
        // ueberall sonst im Projekt (siehe Geraete-UI-Konvention).
        // #viewTabs ist jetzt eine Flex-Zeile: Radar/Map-Buttons links
        // (eigener #viewTabsButtons-Wrapper, damit "justify-content:
        // space-between" genau 2 Elemente auseinanderschiebt statt die
        // beiden Buttons selbst), Wetter-Icon rechts (siehe weiter unten,
        // umgezogen aus der "Updated Xs ago"-Zeile - dort hat sich die
        // Icon-Position bei jeder Sekundenaenderung sichtbar mitverschoben,
        // Alex' Meldung "hüpft hin und her"). Hier bleibt die Position fix.
        // BUGFIX (Alex' Meldung: Icon klebt auf breiten/Desktop-Fenstern am
        // rechten Rand): #viewTabs spannte sich vorher ueber die volle
        // Seitenbreite (kein max-width), waehrend #radarCanvas & Co. auf
        // 400px begrenzt sind - "justify-content:space-between" schob das
        // Icon dadurch bis ganz an den Fensterrand statt neben "Map".
        // Gleiches max-width wie #radarCanvas oben haelt beide in derselben
        // schmalen Inhaltsspalte.
        html += "#viewTabs{display:flex;align-items:center;justify-content:space-between;margin-bottom:8px;max-width:400px;}";
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
        // Dezenter GitHub-Link unten rechts (Alex' Avatar-Logo, siehe
        // handleAvatar()/appendRadarSection()) - "position:fixed" haelt ihn
        // in der Bildschirmecke, unabhaengig vom Scroll-Stand, ueber allem
        // anderen (hoeherer z-index als ".page"). Reduzierte Deckkraft im
        // Ruhezustand (0.55), geht bei Hover/Tap auf voll hoch - klar
        // erkennbar interaktiv, aber nicht ablenkend vom eigentlichen Radar.
        html += "#avatarLink{position:fixed;right:12px;bottom:12px;z-index:500;opacity:0.55;transition:opacity 0.2s;line-height:0;}";
        html += "#avatarLink:hover,#avatarLink:active{opacity:1;}";
        html += "#avatarLink img{width:44px;height:44px;border-radius:50%;border:2px solid var(--accent);display:block;}";
        html += "</style></head><body>";
        // Service-Worker-Registrierung (siehe handleServiceWorker() unten) -
        // "in navigator"-Check noetig, weil Service Worker nur ueber HTTPS
        // ODER "localhost" verfuegbar sind; das Geraet wird ausschliesslich
        // ueber eine reine HTTP-IP-Adresse im lokalen WLAN aufgerufen, dort
        // ist die API in den meisten Browsern schlicht nicht vorhanden -
        // ohne diesen Check wuerde register() dort mit einer Konsolen-
        // Fehlermeldung fehlschlagen, aber ansonsten folgenlos bleiben (die
        // Seite selbst funktioniert unveraendert weiter, nur ohne Offline-
        // Caching der Seitenhuelle).
        html += "<script>if('serviceWorker' in navigator){navigator.serviceWorker.register('/sw.js');}</script>";
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
        // Wetter-Icon (Canvas, gleicher Zeichenstil wie main.cpp::
        // drawWeatherIcon() am Geraet, siehe drawWeatherIconCanvas() weiter
        // unten im Skript) hier rechtsbuendig in der Radar/Map-Tab-Zeile,
        // statt wie vorher winzig direkt in der sich staendig aendernden
        // "Updated Xs ago"-Zeile (siehe #viewTabs-CSS-Kommentar oben) -
        // deutlich groesser (40x40 statt vorher 28x28) und an fester
        // Position, kein Hin-und-her-Huepfen mehr.
        html += "<div id=\"viewTabs\"><div id=\"viewTabsButtons\"><button id=\"tabRadar\" class=\"active\" type=\"button\">Radar</button><button id=\"tabMap\" type=\"button\">Map</button></div>";
        // "transform" verschiebt das Icon rein optisch (Alex' Vorgabe: 100px
        // runter, 250px nach links ausgehend von der bisherigen Position),
        // ohne den restlichen Flex-Zeilenablauf (Radar/Map-Buttons) zu
        // beeinflussen - der urspruengliche Platz in der Zeile bleibt
        // reserviert, nur das Icon selbst rutscht optisch daneben.
        html += "<canvas id=\"weatherIconCanvas\" width=\"120\" height=\"120\" style=\"cursor:pointer;flex-shrink:0;transform:translate(-130px,50px);\" title=\"Weather info\"></canvas></div>";
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
        html += "<div id=\"weatherInfo\"></div>";
        html += "</div>"; // #radarView
        html += "<div id=\"mapView\"><div id=\"leafletMap\"></div></div>";
        html += "<script>(function(){";
        html += "var canvas=document.getElementById('radarCanvas');";
        html += "var ctx=canvas.getContext('2d');";
        html += "var status=document.getElementById('radarStatus');";
        html += "var infoBox=document.getElementById('acInfo');";
        html += "var weatherIconCanvas=document.getElementById('weatherIconCanvas');";
        html += "var weatherIconCtx=weatherIconCanvas.getContext('2d');";
        html += "var weatherInfoBox=document.getElementById('weatherInfo');";
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
        // Gedimmte Gegenstuecke fuer die Nachtdimmung (Alex' Wunsch) - Akzent-
        // farben 1:1 von radar_screen.cpp's Nacht-Farben uebernommen (siehe
        // themeDimColor() dort), Rand-/Hintergrundfarbe proportional im
        // gleichen Verhaeltnis abgedunkelt (ca. 60% Helligkeit) wie die
        // jeweilige Akzentfarbe - reiner Web-Optik-Wert, dafuer gibt es kein
        // 1:1-Geraete-Aequivalent.
        html += "var NIGHT_THEME_PALETTES=[['#00A000','#13231a','#495c50'],['#A06E00','#231a10','#605336'],['#0078A0','#101a23','#405660'],['#A00101','#231010','#604040'],['#7100A0','#191023','#534060']];";
        html += "var lastThemeIndex=" + String(SettingsStore::radarThemeIndex()) + ";";
        html += "var lastIsNight=false;";
        // Laufend (bei JEDEM Poll, nicht nur bei Aenderung) aktuell gehalten
        // - altColor() unten liest das direkt, unabhaengig davon, ob sich
        // seit dem letzten Poll ueberhaupt etwas geaendert hat (der Ring/
        // Marker wird ja ohnehin bei jedem draw()-Aufruf neu gezeichnet).
        html += "var currentIsNight=false;";
        html += "function applyTheme(idx,isNight){var pal=isNight?NIGHT_THEME_PALETTES:THEME_PALETTES;var p=pal[idx]||pal[0];var s=document.documentElement.style;";
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
        // Geschwindigkeiten 1:1 an ScreensaverRain::rainParamsForIntensity()
        // (main.cpp) angeglichen (67.5/105/150 statt vorher veralteter
        // 60/90/130) - Anzahl war bereits korrekt (6/12/20).
        html += "function rainParamsFor(level){if(level>=3)return{count:20,speed:150};if(level===1)return{count:6,speed:67.5};return{count:12,speed:105};}";
        html += "var rainDrops=[];var rainInited=false;var lastRainMs=null;";
        html += "function rainSpawn(d,windDirDeg){var spread=Math.random()*140-70;";
        html += "var entryRad=(windDirDeg+spread)*Math.PI/180;d.x=cx+R*Math.sin(entryRad);d.y=cy-R*Math.cos(entryRad);";
        html += "var travelRad=(windDirDeg+180)*Math.PI/180;d.vx=Math.sin(travelRad);d.vy=-Math.cos(travelRad);}";
        html += "function drawRain(windDirDeg,level,accentColor){var p=rainParamsFor(level);";
        html += "var now=performance.now();var dt=lastRainMs?Math.min(now-lastRainMs,300):16;lastRainMs=now;var step=p.speed*dt/1000;";
        html += "if(!rainInited){rainDrops=[];for(var i=0;i<RAIN_MAX;i++){var d={};rainSpawn(d,windDirDeg);rainDrops.push(d);}rainInited=true;}";
        html += "ctx.save();ctx.strokeStyle=accentColor;ctx.lineWidth=1;";
        html += "for(var i=0;i<rainDrops.length;i++){var d=rainDrops[i];d.x+=d.vx*step;d.y+=d.vy*step;";
        html += "var ddx=d.x-cx,ddy=d.y-cy;if(ddx*ddx+ddy*ddy>R*R){rainSpawn(d,windDirDeg);}";
        html += "if(i>=p.count)continue;";
        html += "var x1=d.x,y1=d.y,x2=d.x-d.vx*RAIN_LEN,y2=d.y-d.vy*RAIN_LEN;";
        html += "var d1=(x1-cx)*(x1-cx)+(y1-cy)*(y1-cy),d2=(x2-cx)*(x2-cx)+(y2-cy)*(y2-cy);";
        html += "if(d1<=R*R&&d2<=R*R){";
        // "Glanzstrich" (ScreensaverRain in main.cpp) - Hauptlinie normal,
        // zweite Linie 1px seitlich versetzt (senkrecht zur Falllinie, da
        // die Tropfen hier schraeg nach Windrichtung fallen statt rein
        // vertikal) in ca. 40% Deckkraft der Hauptlinie.
        html += "ctx.globalAlpha=0.45;ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.stroke();";
        html += "var px=-d.vy,py=d.vx;";
        html += "ctx.globalAlpha=0.18;ctx.beginPath();ctx.moveTo(x1+px,y1+py);ctx.lineTo(x2+px,y2+py);ctx.stroke();";
        html += "}}";
        html += "ctx.restore();}";

        // altColor() bildet die Flughoehe ab (Notfall-/Warnfarben) - bleibt
        // AUSDRUECKLICH themenunabhaengig, exakt wie colorForAltitude() am
        // Geraete-Display (radar_screen.cpp) - NICHT anfassen/umfaerben.
        // Bei Nachtdimmung (currentIsNight, siehe is_night-Feld oben) grob
        // um ca. 38% abgedunkelte Varianten derselben 5 Farben - reiner
        // Web-Optik-Wert (die Web-Hoehenfarbskala ist ohnehin schon eine
        // eigene, unabhaengige Kopie mit anderen Stufen als am Geraet, kein
        // 1:1-Aequivalent noetig).
        html += "function altColor(ft){if(currentIsNight){";
        html += "if(ft<3000)return '#9e3030';if(ft<10000)return '#9e7230';if(ft<25000)return '#9e8c30';if(ft<35000)return '#239e0c';return '#30829e';}";
        html += "if(ft<3000)return '#ff4d4d';if(ft<10000)return '#ffb84d';if(ft<25000)return '#ffe14d';if(ft<35000)return '#39ff14';return '#4dd2ff';}";

        // Wetter-Icon fuer die Live-Radar-Webseite (Alex' Wunsch) - bildet
        // main.cpp::drawWeatherIcon()/drawCloudShape()/drawSunShape() am
        // Geraet nach (gleiche relative Proportionen/Versaetze), nur auf
        // einem eigenen Canvas statt TFT_eSPI-Aufrufen, hochskaliert um den
        // Faktor "s" (Alex' Wunsch: deutlich groesser als das urspruengliche
        // 28x28-Icon, jetzt 40x40 - alle urspruenglichen Geraete-Pixel-
        // Versaetze mit s multipliziert, damit die Proportionen zueinander
        // gleich bleiben). "unknown" zeichnet bewusst nichts, genau wie am
        // Geraet.
        html += "function drawCloudIcon(c,cx,cy,color,s){c.fillStyle=color;";
        html += "c.beginPath();c.arc(cx-5*s,cy+1*s,3*s,0,Math.PI*2);c.fill();";
        html += "c.beginPath();c.arc(cx-1*s,cy-2*s,4*s,0,Math.PI*2);c.fill();";
        html += "c.beginPath();c.arc(cx+4*s,cy,4*s,0,Math.PI*2);c.fill();";
        html += "c.fillRect(cx-8*s,cy,13*s,3*s);}";
        html += "function drawSunIcon(c,cx,cy,r,color,s){c.fillStyle=color;c.strokeStyle=color;c.lineWidth=s;";
        html += "c.beginPath();c.arc(cx,cy,r,0,Math.PI*2);c.fill();";
        html += "for(var i=0;i<8;i++){var ang=i*(Math.PI/4);";
        html += "var x1=cx+(r+2*s)*Math.cos(ang),y1=cy+(r+2*s)*Math.sin(ang);";
        html += "var x2=cx+(r+4*s)*Math.cos(ang),y2=cy+(r+4*s)*Math.sin(ang);";
        html += "c.beginPath();c.moveTo(x1,y1);c.lineTo(x2,y2);c.stroke();}}";
        html += "function drawWeatherIconCanvas(cond){var c=weatherIconCtx;";
        html += "c.clearRect(0,0,weatherIconCanvas.width,weatherIconCanvas.height);";
        // 120x120 statt vorher 40x40 (Alex' Wunsch: nochmal verdreifacht) -
        // s und die Mittelpunkt-Koordinaten proportional mit Faktor 3
        // mitskaliert (1.43->4.29, 20/16->60/48), Form/Proportionen bleiben
        // dadurch unveraendert, nur groesser.
        html += "var s=4.29,cx=60,cy=48;";
        // Bei Nachtdimmung (currentIsNight, siehe is_night-Feld/altColor()
        // oben) dieselbe grobe ~38%-Abdunkelung wie bei den Hoehenfarben -
        // Sonne/Blitz-Gelb, Wolken-Hellgrau, Regen-Hellblau und Schnee-Weiss
        // bekommen je eine gedaempfte Nacht-Variante, statt bei Nacht
        // weiterhin grell hell zu leuchten.
        html += "var sunColor=currentIsNight?'#9e8300':'#ffd400';";
        html += "var cloudColor=currentIsNight?'#808386':'#cfd4d8';";
        html += "var rainColor=currentIsNight?'#3d7b9e':'#63c7ff';";
        html += "var snowColor=currentIsNight?'#9e9e9e':'#ffffff';";
        html += "if(cond==='clear'){drawSunIcon(c,cx,cy,6*s,sunColor,s);}";
        html += "else if(cond==='partly_cloudy'){drawSunIcon(c,cx-4*s,cy-3*s,4*s,sunColor,s);drawCloudIcon(c,cx+3*s,cy+2*s,cloudColor,s);}";
        html += "else if(cond==='cloudy'){drawCloudIcon(c,cx,cy,cloudColor,s);}";
        html += "else if(cond==='rain'){drawCloudIcon(c,cx,cy-3*s,cloudColor,s);c.strokeStyle=rainColor;c.lineWidth=s;";
        html += "c.beginPath();c.moveTo(cx-4*s,cy+4*s);c.lineTo(cx-6*s,cy+8*s);c.stroke();";
        html += "c.beginPath();c.moveTo(cx,cy+4*s);c.lineTo(cx-2*s,cy+8*s);c.stroke();";
        html += "c.beginPath();c.moveTo(cx+4*s,cy+4*s);c.lineTo(cx+2*s,cy+8*s);c.stroke();}";
        html += "else if(cond==='snow'){drawCloudIcon(c,cx,cy-3*s,cloudColor,s);c.fillStyle=snowColor;";
        html += "c.beginPath();c.arc(cx-4*s,cy+6*s,1.2*s,0,Math.PI*2);c.fill();";
        html += "c.beginPath();c.arc(cx,cy+7*s,1.2*s,0,Math.PI*2);c.fill();";
        html += "c.beginPath();c.arc(cx+4*s,cy+6*s,1.2*s,0,Math.PI*2);c.fill();}";
        html += "else if(cond==='thunderstorm'){drawCloudIcon(c,cx,cy-3*s,cloudColor,s);c.strokeStyle=sunColor;c.lineWidth=s;";
        html += "c.beginPath();c.moveTo(cx,cy+3*s);c.lineTo(cx-3*s,cy+7*s);c.lineTo(cx+1*s,cy+7*s);c.lineTo(cx-2*s,cy+11*s);c.stroke();}";
        html += "}"; // drawWeatherIconCanvas

        // Textform der Condition-Werte fuers Popup (gleicher Zweck wie
        // main.cpp::conditionLabel(), hier als reines JS-Pendant).
        html += "function weatherConditionText(cond){switch(cond){";
        html += "case 'clear':return 'Clear';case 'partly_cloudy':return 'Partly cloudy';case 'cloudy':return 'Cloudy';";
        html += "case 'rain':return 'Rain';case 'snow':return 'Snow';case 'thunderstorm':return 'Thunderstorm';";
        html += "default:return 'Unknown';}}";

        // Wetter-Info-Popup (Antippen/Klicken des Icons) - gleicher Inhalt
        // wie main.cpp::showWeatherInfo() am Geraet (Standort-Hinweis,
        // METAR, Sonnenauf-/-untergang, Kurzvorhersage), alle Werte kommen
        // bereits server-seitig fertig aufgeloest aus handleRadarJson().
        html += "function showWeatherInfoPopup(){var d=lastData;var lines=[];";
        html += "lines.push('Weather shown is for the currently active location.');";
        html += "if(d.metar_available){lines.push('<br>METAR ('+d.metar_airport_code+'):<br>'+d.metar_raw);}";
        html += "if(d.sun_available){if(d.sun_always_day){lines.push('<br>Sun: up all day today (polar day)');}";
        html += "else if(d.sun_always_night){lines.push('<br>Sun: down all day today (polar night)');}";
        html += "else{lines.push('<br>Sunrise: '+d.sunrise_local+'&emsp;Sunset: '+d.sunset_local);}}";
        html += "if(d.forecast_available){var t=metric?d.forecast_temp_c:(d.forecast_temp_c*9/5+32);";
        html += "lines.push('<br>Forecast (+'+d.forecast_hours_ahead+'h): '+Math.round(t)+'\\u00b0'+(metric?'C':'F')+', '+weatherConditionText(d.forecast_condition));}";
        html += "weatherInfoBox.innerHTML=lines.join('<br>')+'<br><a href=\"#\" id=\"weatherInfoClose\">Close</a>';";
        html += "weatherInfoBox.style.display='block';";
        html += "document.getElementById('weatherInfoClose').onclick=function(e){e.preventDefault();hideWeatherInfo();};";
        html += "}";
        html += "function hideWeatherInfo(){weatherInfoBox.style.display='none';}";
        html += "weatherIconCanvas.addEventListener('click',function(){";
        html += "if(weatherInfoBox.style.display==='block'){hideWeatherInfo();}else{showWeatherInfoPopup();}});";

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
        // Typ-Silhouette (Rumpf- + Tragflaechen-Linie statt Kreis+Strich,
        // gleiches Grundprinzip wie drawTypedMarker() in radar_screen.cpp,
        // nur als zwei einfache Linien statt versetzter Doppellinien - auf
        // dem Canvas reicht dafuer ctx.lineWidth statt manuellem Versatz).
        // "type_class" kommt aus handleRadarJson() (classifyTypeSilhouetteWeb()
        // unten) - "airliner"/"unknown" nutzen dieselbe Groesse, "privatejet"
        // ist kleiner, "turboprop" wie "airliner" plus zwei Triebwerks-
        // Punkte auf der Tragflaeche. "heavy" vergroessert alle Masse UND
        // die Linienbreite, ersetzt die fruehere separate Ring+Kreis-Form.
        html += "else{var hr=a.track_deg*Math.PI/180,pr=hr+Math.PI/2;var tc=a.type_class||'unknown';";
        html += "var noseLen,tailLen,wingLen;";
        html += "if(tc==='privatejet'){noseLen=a.heavy?10:7;tailLen=a.heavy?7:5;wingLen=a.heavy?8:6;}";
        html += "else{noseLen=a.heavy?12:9;tailLen=a.heavy?9:7;wingLen=a.heavy?10:8;}";
        html += "ctx.lineWidth=a.heavy?2:1;";
        html += "ctx.beginPath();ctx.moveTo(x-tailLen*Math.sin(hr),y+tailLen*Math.cos(hr));ctx.lineTo(x+noseLen*Math.sin(hr),y-noseLen*Math.cos(hr));ctx.stroke();";
        html += "ctx.beginPath();ctx.moveTo(x-wingLen*Math.sin(pr),y+wingLen*Math.cos(pr));ctx.lineTo(x+wingLen*Math.sin(pr),y-wingLen*Math.cos(pr));ctx.stroke();";
        html += "if(tc==='turboprop'){var engDist=a.heavy?7:5,engR=a.heavy?3:2;";
        html += "ctx.beginPath();ctx.arc(x+engDist*Math.sin(pr),y-engDist*Math.cos(pr),engR,0,Math.PI*2);ctx.fill();";
        html += "ctx.beginPath();ctx.arc(x-engDist*Math.sin(pr),y+engDist*Math.cos(pr),engR,0,Math.PI*2);ctx.fill();}";
        html += "ctx.lineWidth=1;}";
        // Ring-Radius etwas groesser bei "heavy" (Alex' Vorgabe), damit der
        // Notfall-/Beobachtungs-/Militaer-Ring nicht durch die jetzt
        // groessere Silhouette (Nase bis zu 12px) schneidet.
        html += "var ringR=a.heavy?13:9;";
        html += "if(a.emergency){ctx.strokeStyle='#ff3b3b';ctx.beginPath();ctx.arc(x,y,ringR,0,Math.PI*2);ctx.stroke();}";
        html += "else if(a.watched){ctx.strokeStyle='#00e5ff';ctx.beginPath();ctx.arc(x,y,ringR,0,Math.PI*2);ctx.stroke();}";
        html += "else if(a.notable){ctx.strokeStyle='#ff9f1a';ctx.beginPath();ctx.arc(x,y,ringR,0,Math.PI*2);ctx.stroke();}";
        // Ausgewaehltes Flugzeug (per Klick/Tap, siehe unten) bekommt einen
        // weissen Auswahlring, gleiches Prinzip wie isSelected auf dem
        // Geraete-Display (radar_screen.cpp render()).
        html += "if(a.hex===selectedHex){ctx.strokeStyle='#ffffff';ctx.beginPath();ctx.arc(x,y,11,0,Math.PI*2);ctx.stroke();}";
        html += "ctx.fillStyle=color;ctx.textAlign='center';ctx.fillText(a.callsign,x,y-8);";
        html += "markers.push({x:x,y:y,a:a});";
        html += "});";
        html += "if(data.raining){drawRain(data.wind_dir_deg||0,data.rain_intensity||2,accentColor);}else{lastRainMs=null;rainInited=false;}";
        html += "drawWeatherIconCanvas(data.weather_condition||'unknown');";
        html += "if(weatherInfoBox.style.display==='block'){showWeatherInfoPopup();}"; // haelt das offene Popup mit frischen Werten synchron (poll() alle 8s)
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

        // Airline-Logo oben links im Flugzeug-Info-Panel (Alex' Wunsch,
        // NUR fuer die Webseite - am Geraet selbst gibt es das nicht).
        // images.kiwi.com liefert saubere PNG-Logos ueber den 2-stelligen
        // IATA-Code, kennt aber KEINE 3-stelligen ICAO-Praefixe - das
        // Projekt identifiziert Airlines bisher nur ueber den ICAO-Praefix
        // aus dem Callsign (siehe airline_lookup.cpp::extractAirlinePrefix()
        // am Geraet, hier als JS-Pendant nachgebaut). Deshalb eine eigene
        // kleine ICAO->IATA-Tabelle (haeufige internationale/europaeische
        // Carrier, bewusst nicht vollstaendig - unbekannte Praefixe liefern
        // einfach null und damit das gezeichnete Fallback-Icon unten).
        html += "var ICAO_TO_IATA={DLH:'LH',BAW:'BA',AFR:'AF',KLM:'KL',SWR:'LX',AUA:'OS',IBE:'IB',TAP:'TP',SAS:'SK',FIN:'AY',"
                "THY:'TK',AEE:'A3',RYR:'FR',EZY:'U2',WZZ:'W6',VLG:'VY',EWG:'EW',NAX:'DY',IBS:'I2',TRA:'HV',"
                "PGT:'PC',BEL:'SN',CFG:'DE',EXS:'LS',TOM:'BY',LGL:'LG',BTI:'BT',LOT:'LO',CSA:'OK',ROT:'RO',"
                "AFL:'SU',UAE:'EK',QTR:'QR',ETD:'EY',SVA:'SV',MSR:'MS',RJA:'RJ',ELY:'LY',GFA:'GF',KAC:'KU',"
                "OMA:'WY',MEA:'ME',RAM:'AT',TUN:'TU',ETH:'ET',SAA:'SA',KQA:'KQ',DAH:'AH',ICE:'FI',"
                "UAL:'UA',AAL:'AA',DAL:'DL',SWA:'WN',JBU:'B6',ASA:'AS',FFT:'F9',NKS:'NK',ACA:'AC',WJA:'WS',"
                "CPA:'CX',SIA:'SQ',ANA:'NH',JAL:'JL',KAL:'KE',AAR:'OZ',CCA:'CA',CES:'MU',CSN:'CZ',THA:'TG',"
                "MAS:'MH',GIA:'GA',PAL:'PR',CAL:'CI',EVA:'BR',AIC:'AI',IGO:'6E',QFA:'QF',ANZ:'NZ',VOZ:'VA',"
                "PIA:'PK',LAN:'LA',TAM:'JJ',ARG:'AR',AVA:'AV',CMP:'CM',AMX:'AM',GLO:'G3',AZU:'AD',"
                "FDX:'FX',UPS:'5X',GTI:'5Y',CLX:'CV',ITY:'AZ',EIN:'EI',CRL:'SS',TSC:'TS'};";
        // Extrahiert bis zu 3 Buchstaben vom Callsign-Anfang (gleiche simple
        // Logik wie extractAirlinePrefix() am Geraet) und schlaegt sie in
        // der Tabelle oben nach - null bei keinem Treffer.
        html += "function airlineLogoUrl(callsign){";
        html += "if(!callsign)return null;";
        html += "var m=callsign.match(/^[A-Za-z]{1,3}/);";
        html += "if(!m)return null;";
        html += "var iata=ICAO_TO_IATA[m[0].toUpperCase()];";
        html += "return iata?('https://images.kiwi.com/airlines/64x64/'+iata+'.png'):null;";
        html += "}";
        // Fallback: selbst gezeichnetes "durchgestrichenes Flugzeug"-Icon
        // (Inline-SVG statt Bild, damit keine weitere externe Anfrage noetig
        // ist) - fuer keinen Tabellentreffer UND fuer den Fall, dass das
        // echte Logo beim Laden fehlschlaegt (onerror unten). "fill/stroke:
        // var(--accent)" faerbt es automatisch in der aktuell aktiven
        // Radar-Themefarbe ein, genau wie das restliche UI.
        html += "function airlineLogoFallbackSvg(){";
        html += "return '<svg viewBox=\"0 0 24 24\" width=\"100%\" height=\"100%\" style=\"display:block;\">'+"
                "'<path d=\"M21,16v-2l-8-5V3.5C13,2.67,12.33,2,11.5,2S10,2.67,10,3.5V9l-8,5v2l8-2.5V19l-2,1.5V22l3.5-1l3.5,1v-1.5L13,19v-5.5L21,16z\" style=\"fill:var(--accent);opacity:0.85;\"></path>'+"
                "'<line x1=\"2\" y1=\"2\" x2=\"22\" y2=\"22\" style=\"stroke:var(--accent);stroke-width:2;opacity:0.85;\"></line>'+"
                "'</svg>';";
        html += "}";
        // Baut das komplette Logo-Element (echtes, entsaettigt+eingefaerbtes
        // Logo ODER Fallback-SVG) fuer showInfo() unten. Das echte Logo
        // bleibt ein normales <img> (fuer die onerror-Fehlererkennung),
        // "filter:grayscale(1)" entsaettigt es, die halbtransparente
        // Akzentfarb-Flaeche darueber mit "mix-blend-mode:color" faerbt es
        // ein UND behaelt dabei die urspruengliche Helligkeit/Zeichnung des
        // Logos (Standard-CSS-Trick fuer eingefaerbte Graustufenbilder) -
        // insgesamt dezent statt bunt-aufdringlich (Alex' Vorgabe).
        html += "function airlineLogoHtml(a){";
        html += "var url=a.has_callsign?airlineLogoUrl(a.callsign):null;";
        html += "var inner=url?";
        html += "('<img src=\"'+url+'\" alt=\"\" style=\"width:100%;height:100%;object-fit:contain;filter:grayscale(1);display:block;\" onerror=\"this.parentNode.innerHTML=airlineLogoFallbackSvg();\">'+"
                "'<span style=\"position:absolute;inset:0;background:var(--accent);mix-blend-mode:color;pointer-events:none;\"></span>')";
        html += ":airlineLogoFallbackSvg();";
        html += "return '<span style=\"position:relative;display:inline-block;width:36px;height:36px;flex-shrink:0;opacity:0.85;\">'+inner+'</span>';";
        html += "}";

        // Info-Panel fuer ein angetipptes Flugzeug - bewusst eine eigene,
        // stehenbleibende Box (kein Tooltip/Popup, das beim naechsten
        // Neuzeichnen einfach verschwindet), mit explizitem Schliessen-Link,
        // gleiches Grundprinzip wie infoScreen() am Geraet: der Nutzer soll
        // aktiv entscheiden, wann die Info wieder verschwindet.
        html += "function showInfo(a){";
        html += "var lines=[];";
        html += "lines.push('<b>'+(a.callsign||a.hex)+'</b> ('+a.hex+')');";
        // Airline-Name, Registrierung, Typcode - dieselben Werte, die
        // radar_screen.cpp::drawDetailPanel() am Geraet schon lange zeigt
        // (a.airlineName/a.reg/a.typeCode), bisher aber nicht im Web-JSON
        // standen. Route/Flugbuch-Historie bewusst NICHT mit dabei (siehe
        // Kommentar bei handleRadarJson() oben).
        html += "if(a.airline_name){lines.push(a.airline_name);}";
        html += "var regType=[];if(a.reg){regType.push('Reg: '+a.reg);}if(a.type_code){regType.push('Type: '+a.type_code);}";
        html += "if(regType.length){lines.push(regType.join(' \\u00b7 '));}";
        html += "lines.push('Altitude: '+Math.round(a.alt_ft)+' ft');";
        html += "if(a.speed_kt){lines.push('Speed: '+Math.round(a.speed_kt)+' kt');}";
        // Steig-/Sinkrate, gleiche Schwelle (+-100ft/min) wie DETAIL_CLIMB/
        // DETAIL_DESCENT/DETAIL_LEVEL am Geraet.
        html += "if(a.vert_rate_ft_min>100){lines.push('Climbing +'+a.vert_rate_ft_min+'ft/min');}";
        html += "else if(a.vert_rate_ft_min<-100){lines.push('Descending '+a.vert_rate_ft_min+'ft/min');}";
        html += "else{lines.push('Level');}";
        // Hoehenwinkel rein clientseitig aus alt_ft/dist_km berechnet (gleiche
        // einfache atan2-Formel wie am Geraet) - kein eigenes JSON-Feld
        // noetig, direkt neben Distanz/Peilung mit angezeigt.
        html += "var elevDeg=Math.atan2(a.alt_ft*0.3048,a.dist_km*1000)*180/Math.PI;";
        html += "lines.push('Distance: '+fmtDist(a.dist_km)+', bearing '+Math.round(a.bearing_deg)+'\\u00b0, elevation '+Math.round(elevDeg)+'\\u00b0');";
        html += "lines.push('Heading: '+Math.round(a.track_deg)+'\\u00b0');";
        // Naeherungs-/Entfernungs-Trend, gleiche 3 Zustaende wie
        // DETAIL_APPROACHING/_DEPARTING/_PASSING am Geraet (dort Text-Pfeile
        // "v"/"^"/"->" mangels Unicode-Glyphen im TFT-Font - hier echte
        // Pfeilsymbole, da der Browser sie problemlos darstellt).
        html += "var trendSymbols={approaching:'\\u2193 Approaching',departing:'\\u2191 Departing',passing:'\\u2192 Passing'};";
        html += "if(trendSymbols[a.distance_trend]){lines.push(trendSymbols[a.distance_trend]);}";
        html += "if(a.squawk){lines.push('Squawk: '+a.squawk);}";
        // "Sichtbar seit" - gleiche Xmin Ys-Beschriftung wie am Geraet
        // (DETAIL_SEEN_FOR_PREFIX + formatDurationLabeled()).
        html += "var sfMin=Math.floor(a.seen_for_sec/60),sfSec=a.seen_for_sec%60;";
        html += "lines.push('Seen for '+(sfMin>0?(sfMin+'min '+sfSec+'s'):(sfSec+'s')));";
        // Anflug-Hinweis (Best-Effort-Erkennung, siehe aircraft_table.cpp::
        // postFetchUpdate()) - nur wenn approach_likely true ist.
        html += "if(a.approach_likely){lines.push('Approaching, ETA ~'+a.approach_eta_min+' min');}";
        // "Ueberflug"-CPA - nur wenn cpa_relevant true ist.
        html += "if(a.cpa_relevant){lines.push('Fly-by in ~'+Math.round(a.cpa_eta_min)+' min');}";
        // "Steckbrief" (kuerzeste Distanz/hoechste Geschwindigkeit seit dem
        // ersten Sichten in dieser Sitzung) - nur wenn BEIDE Werte gesetzt
        // sind (ArduinoJson liefert fehlende Felder als undefined, nicht 0).
        html += "if(a.session_min_distance_km!==undefined&&a.session_max_speed_kt!==undefined){";
        html += "lines.push('Closest: '+a.session_min_distance_km.toFixed(1)+'km \\u00b7 Fastest: '+Math.round(a.session_max_speed_kt)+'kt');}";
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
        // Logo/Fallback-Icon oben links, Textzeilen daneben (align-items:
        // flex-start haelt das Icon am oberen Rand des Textblocks) - wird
        // bei JEDEM showInfo()-Aufruf neu gebaut, aktualisiert sich also
        // automatisch bei jedem neu ausgewaehlten Flugzeug UND bei jedem
        // Poll-Refresh (siehe refreshSelectedInfo() oben).
        html += "infoBox.innerHTML='<div style=\"display:flex;gap:10px;align-items:flex-start;\">'+airlineLogoHtml(a)+"
                "'<div>'+lines.join('<br>')+'<br><a href=\"#\" id=\"acInfoClose\">Close</a></div></div>';";
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

        // Echte Fernsteuerung der Geraete-Reichweite (siehe Design-Absprache
        // im Chat) - die Dropdown-Auswahl schickt jetzt ZUSAETZLICH zum
        // bisherigen reinen Anzeige-Zoom einen POST an /control/range, der
        // die Geraete-Einstellung tatsaechlich aendert (SettingsStore::
        // setRangeIndex(), identischer Pfad wie ein physischer Tap).
        // pendingRangeKm haelt fest, was WIR selbst gerade angefragt haben,
        // bis der Server dies per data.device_range_km (siehe
        // handleRadarJson()) bestaetigt hat - solange verhindert es, dass
        // ein Poll unsere eigene, gerade abgeschickte Auswahl wieder
        // ueberschreibt (z.B. falls das Debounce-Fenster die Persistierung
        // kurz verzoegert hat).
        html += "var pendingRangeKm=null;";
        html += "if(rangeSel){rangeSel.addEventListener('change',function(){";
        html += "pendingRangeKm=rangeSel.value;";
        html += "fetch('/control/range',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'range_km='+rangeSel.value}).finally(poll);";
        html += "});}";
        html += "function poll(){";
        html += "var url='/radar.json';";
        html += "if(rangeSel&&rangeSel.value){url+='?range_km='+rangeSel.value;}";
        html += "fetch(url).then(function(r){return r.json();}).then(function(data){";
        // Themenwechsel am Geraet (waehrend die Seite offen ist) live
        // uebernehmen - vergleicht data.theme_index gegen den zuletzt
        // bekannten Wert, ruft applyTheme() nur bei tatsaechlicher Aenderung
        // auf (unnoetiges Neusetzen der CSS-Variablen bei jedem Poll waere
        // harmlos, aber unnoetig).
        // Wie beim Themenwechsel oben, jetzt zusaetzlich auf is_night
        // reagierend (Nachtdimmung, siehe NIGHT_THEME_PALETTES oben) -
        // applyTheme() wird bei JEDER Aenderung eines der beiden Werte neu
        // aufgerufen, damit Tag<->Nacht-Wechsel automatisch bei jedem Poll
        // (alle 8s) live uebernommen wird.
        html += "var curIsNight=!!data.is_night;currentIsNight=curIsNight;";
        html += "if(data.theme_index!==undefined&&(data.theme_index!==lastThemeIndex||curIsNight!==lastIsNight)){lastThemeIndex=data.theme_index;lastIsNight=curIsNight;applyTheme(data.theme_index,curIsNight);}";
        // Reichweiten-Aenderung AM GERAET (physischer Tap, waehrend die
        // Seite offen ist) im Dropdown nachziehen - liest bewusst
        // data.device_range_km (NIE durch den eigenen "range_km"-Query-
        // Parameter beeinflusst, siehe handleRadarJson()), nicht data.range_km
        // (das waere nur ein Echo unserer eigenen Anfrage und wuerde eine
        // physische Aenderung nie zeigen). Solange eine eigene Aenderung noch
        // nicht bestaetigt ist (pendingRangeKm!==null), wird das Dropdown
        // nicht von aussen ueberschrieben - erst wenn der gemeldete Geraete-
        // Wert mit der eigenen Anfrage uebereinstimmt, gilt sie als
        // bestaetigt und die normale Synchronisation greift wieder.
        html += "if(rangeSel&&data.device_range_km!==undefined){";
        html += "var deviceVal=String(Math.round(data.device_range_km));";
        html += "if(pendingRangeKm!==null&&deviceVal===pendingRangeKm){pendingRangeKm=null;}";
        html += "if(pendingRangeKm===null&&deviceVal!==rangeSel.value){rangeSel.value=deviceVal;}";
        html += "}";
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

        // Dezenter, fest positionierter GitHub-Link unten rechts (Alex'
        // Avatar-Logo, siehe web_avatar_logo.h/handleAvatar() oben und
        // #avatarLink-CSS in htmlHeader()) - verlinkt auf dasselbe Repo wie
        // der QR-Code am Geraete-Display (runGithubQrScreen() in main.cpp).
        html += "<a id=\"avatarLink\" href=\"https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD\" target=\"_blank\" rel=\"noopener\" title=\"Eiswolfs Flightradar on GitHub\"><img src=\"/avatar.png\" alt=\"GitHub\"></a>";

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
        // Feature 12 Nachtrag - dezenter Hinweis auf die PWA-Installierbarkeit
        // (siehe /manifest.json, /icon.png, /sw.js oben) direkt auf der
        // Seite selbst, da das bisher nur in der README stand, die niemand
        // liest, der einfach nur die Seite im Browser oeffnet. "display:none"
        // per Default - das kleine Skript direkt darunter blendet die Zeile
        // NUR ein, wenn die Seite gerade in einem normalen Browser-Tab laeuft
        // (nicht bereits als installierte App im Standalone-/Vollbildmodus,
        // siehe Alex' Vorgabe: ein Nutzer, der die App schon installiert hat,
        // braucht den Hinweis nicht mehr zu sehen). "display-mode: standalone"
        // deckt Android/Chrome ab, "navigator.standalone" das iOS-Aequivalent
        // (Chrome kennt diese Eigenschaft gar nicht, daher der "=== true"-
        // Vergleich statt einer reinen Wahrheitswert-Pruefung). Leicht
        // unterschiedlicher Text je Plattform, da der Installationsweg
        // tatsaechlich unterschiedlich ist (iOS: Teilen-Button, Android/
        // Desktop-Chrome: Browser-Menue) - "navigator.standalone !== undefined"
        // ist dabei KEIN echter iOS-Check per se, aber diese Eigenschaft
        // existiert ausschliesslich in iOS Safari, ist also in der Praxis ein
        // zuverlaessiges Unterscheidungsmerkmal.
        html += "<div id=\"pwaHint\" style=\"display:none;margin-top:4px;\">&#128241; <span id=\"pwaHintText\"></span></div>";
        html += "<script>(function(){";
        html += "var standalone=window.matchMedia('(display-mode: standalone)').matches||window.navigator.standalone===true;";
        html += "if(standalone)return;";
        html += "var el=document.getElementById('pwaHint');if(!el)return;";
        html += "var isIOS=window.navigator.standalone!==undefined;";
        html += "document.getElementById('pwaHintText').textContent=isIOS";
        html += "?'Add to Home Screen (Share button) for the full-screen app view'";
        html += ":'Add to Home screen (browser menu) for the full-screen app view';";
        html += "el.style.display='';";
        html += "})();</script>";
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

    // Echte Fernsteuerung der Geraete-Reichweite von der Web-UI aus (siehe
    // Design-Absprache im Chat) - im Gegensatz zu den obigen Listen-
    // Endpunkten KEIN volles Seiten-Redirect (wuerde die laufende Live-
    // Radar-Ansicht unterbrechen), sondern ein leichter AJAX-Endpunkt mit
    // knapper Klartext-Antwort, den poll() im appendRadarSection()-Skript
    // per fetch() aufruft. Ruft SettingsStore::setRangeIndex() DIREKT auf -
    // derselbe Code-Pfad wie ein physischer Tap in radar_screen.cpp, keine
    // eigene Persistenz-/LED-Logik noetig.
    void handleControlRange() {
        if (!server.hasArg("range_km")) {
            server.send(400, "text/plain", "missing range_km");
            return;
        }
        float requested = server.arg("range_km").toFloat();
        int8_t matchedIdx = -1;
        for (uint8_t i = 0; i < Config::RANGE_STEP_COUNT; i++) {
            if (fabsf(Config::RANGE_STEPS_KM[i] - requested) < 0.5f) {
                matchedIdx = (int8_t)i;
                break;
            }
        }
        if (matchedIdx < 0) {
            server.send(400, "text/plain", "unknown range_km");
            return;
        }
        uint32_t now = millis();
        if (now - lastRangeCommandMs >= MIN_CONTROL_INTERVAL_MS) {
            SettingsStore::setRangeIndex((uint8_t)matchedIdx);
            lastRangeCommandMs = now;
        }
        // Trotzdem 200 OK, auch wenn das Debounce-Fenster diese konkrete
        // Anfrage uebersprungen hat - kein Fehler im Browser, der aktuelle
        // (evtl. noch nicht ganz frischste) Wert ist ohnehin schon gesetzt.
        server.send(200, "text/plain", "ok");
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
        bool militaryOn = SettingsStore::militarySquawkDetectionEnabled();

        JsonDocument doc;
        doc["range_km"] = rangeKm;
        // Anders als "range_km" oben (das bei aktivem "range_km"-Query-
        // Parameter einfach den angefragten Anzeige-Zoom zurueckspiegelt,
        // siehe Kommentar dort) ist dieses Feld NIEMALS durch einen
        // Query-Parameter beeinflussbar - es liefert immer die tatsaechliche
        // Geraete-Einstellung (SettingsStore::rangeIndex()), analog zu
        // "theme_index" unten. Grundlage fuer die Geraet->Web-UI-
        // Reichweiten-Synchronisation (rangeSel-Abgleich in
        // appendRadarSection()) - ohne dieses eigene Feld liesse sich eine
        // physische Reichweitenaenderung am Geraet aus der Web-UI heraus
        // nie erkennen, sobald die Seite bereits einen eigenen "range_km"-
        // Query-Wert verschickt (was nach jeder Dropdown-Auswahl der Fall
        // ist).
        doc["device_range_km"] = Config::RANGE_STEPS_KM[SettingsStore::rangeIndex()];
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
        // Nachtdimmung auch fuer die Live-Radar-Webseite (Alex' Wunsch) -
        // exakt dieselbe Bedingung wie radar_screen.cpp::
        // nightDimActiveNow() (Schalter an UND aktuell Nachtstunden am
        // Heimatstandort, siehe isNightHoursWeb() oben). Client-JS nutzt das
        // fuer die gedimmte NIGHT_THEME_PALETTES-Variante (applyTheme())
        // UND fuer abgedunkelte Hoehenfarben (altColor()), siehe
        // appendRadarSection().
        doc["is_night"] = SettingsStore::nightDimmingEnabled() && isNightHoursWeb();
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

            // Wetter-Icon + Info-Popup fuer die Live-Radar-Webseite (Alex'
            // Wunsch) - bildet main.cpp::drawWeatherIcon()/showWeatherInfo()
            // am Geraet nach, alle Werte server-seitig fertig aufgeloest
            // (inkl. IATA/ICAO-Auswahl und Sonnenauf-/-untergangsberechnung),
            // damit das Client-JS keine eigene Logik dafuer braucht.
            doc["weather_condition"] = weatherConditionWebLabel(cond);

            Weather::Metar metar = Weather::currentMetar();
            Weather::NearestAirport nearestAirport = Weather::currentNearestAirport();
            doc["metar_available"] = metar.available;
            if (metar.available) {
                // Gleiche IATA/ICAO-Auswahl wie main.cpp::showWeatherInfo()
                // (SettingsStore::useIataAirportCodes()), hier server-seitig
                // aufgeloest statt im Client-JS nachzubauen.
                bool useIata = SettingsStore::useIataAirportCodes() && nearestAirport.iata[0];
                doc["metar_airport_code"] = useIata ? nearestAirport.iata : metar.icao;
                doc["metar_raw"] = metar.raw;
            }

            // Sonnenauf-/-untergang fuer den aktuell aktiven Standort -
            // gleiche SunTimes::compute()-Berechnung wie main.cpp::
            // showWeatherInfo(), hier ebenfalls server-seitig statt im
            // Client-JS (das kennt weder Standort noch hat es eine
            // Sonnenstand-Formel).
            {
                double lat = 0, lon = 0;
                LocationManager::getHomeLocation(lat, lon);
                bool sunAvailable = false;
                bool alwaysDay = false;
                bool alwaysNight = false;
                char sunriseBuf[6] = {0};
                char sunsetBuf[6] = {0};
                time_t now = time(nullptr);
                if ((lat != 0.0 || lon != 0.0) && now > 8 * 3600 * 2) {
                    struct tm tmNow;
                    localtime_r(&now, &tmNow);
                    SunTimes::Result sun = SunTimes::compute(lat, lon, tmNow.tm_year + 1900, tmNow.tm_mon + 1,
                                                              tmNow.tm_mday, LocationManager::utcOffsetSeconds());
                    if (sun.valid) {
                        sunAvailable = true;
                        alwaysDay = sun.alwaysDay;
                        alwaysNight = sun.alwaysNight;
                        if (!alwaysDay && !alwaysNight) {
                            int sunriseMin = (int)roundf(sun.sunriseHour * 60.0f) % (24 * 60);
                            int sunsetMin = (int)roundf(sun.sunsetHour * 60.0f) % (24 * 60);
                            snprintf(sunriseBuf, sizeof(sunriseBuf), "%02d:%02d", sunriseMin / 60, sunriseMin % 60);
                            snprintf(sunsetBuf, sizeof(sunsetBuf), "%02d:%02d", sunsetMin / 60, sunsetMin % 60);
                        }
                    }
                }
                doc["sun_available"] = sunAvailable;
                doc["sun_always_day"] = alwaysDay;
                doc["sun_always_night"] = alwaysNight;
                doc["sunrise_local"] = sunriseBuf;
                doc["sunset_local"] = sunsetBuf;
            }

            // Kurzvorhersage (main.cpp::showWeatherInfo() bzw.
            // Weather::currentForecast()) - Temperatur bleibt bewusst in
            // Celsius (roh), das Client-JS rechnet mit der bereits
            // vorhandenen "metric"-Variable selbst in Fahrenheit um, genau
            // wie es das schon fuer andere Werte auf der Seite tut.
            Weather::Forecast forecast = Weather::currentForecast();
            doc["forecast_available"] = forecast.available;
            if (forecast.available) {
                doc["forecast_temp_c"] = forecast.temperatureC;
                doc["forecast_condition"] = weatherConditionWebLabel(forecast.condition);
                doc["forecast_hours_ahead"] = forecast.hoursAhead;
            }
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
            // "notable" (oranger Ring) ist fuer Militaer-/Behoerdenfluege
            // reserviert - Heavy-Flugzeuge bekommen stattdessen die eigene
            // Markerform (siehe "heavy" oben). Erkennung ueber Squawk-Code-
            // Bereiche (isMilitaryGovSquawkWeb() oben, 1:1 aus
            // radar_screen.cpp uebernommen), gleiches "?"-Best-Effort-
            // Prinzip wie am Geraete-Display - Rufzeichen-Praefixe
            // (isNotableCallsign() am Geraet) bleiben unnachgebaut, da die
            // zugehoerige Praefixliste im Projekt nirgends existiert.
            // militaryOn spiegelt denselben Schalter wie am Geraet
            // (SettingsStore::militarySquawkDetectionEnabled()).
            bool isNotable = militaryOn && isMilitaryGovSquawkWeb(a.squawk);

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
            // Typ-Silhouette (Linienflugzeug/Privatjet/Turboprop/Unknown)
            // fuer die Marker-Form im JS-draw() oben - siehe
            // classifyTypeSilhouetteWeb() weiter oben in dieser Datei.
            o["type_class"] = typeSilhouetteWebLabel(classifyTypeSilhouetteWeb(a.typeCode));
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

            // Weitere Detail-Panel-Werte fuers Web-Popup (Alex' Wunsch) -
            // alle bereits fertig im Aircraft-Snapshot berechnet (siehe
            // aircraft.h/aircraft_table.cpp::postFetchUpdate()), genau wie
            // radar_screen.cpp::drawDetailPanel() sie am Geraet anzeigt.
            // Route (hexdb) und Flugbuch-Historie bewusst NICHT dabei -
            // brauchen zusaetzliche Netzwerk-/SD-Zugriffe, eigene Baustelle.
            o["reg"] = a.reg;
            o["type_code"] = a.typeCode;
            o["airline_name"] = a.airlineName;
            o["vert_rate_ft_min"] = a.vertRateFtMin;
            switch (a.distanceTrend) {
                case Aircraft::DistanceTrend::Approaching: o["distance_trend"] = "approaching"; break;
                case Aircraft::DistanceTrend::Departing:   o["distance_trend"] = "departing";   break;
                case Aircraft::DistanceTrend::Passing:     o["distance_trend"] = "passing";     break;
                case Aircraft::DistanceTrend::Unknown:
                default:                                    o["distance_trend"] = "unknown";     break;
            }
            // Sekunden statt des rohen millis()-Werts - "wie lange her" ist
            // fuer den Webclient portabel, ein absoluter Geraete-millis()-
            // Zeitstempel waere es nicht (andere Uhr/Ursprung).
            uint32_t seenForMs = (a.firstSeenMs > 0 && millis() >= a.firstSeenMs) ? (millis() - a.firstSeenMs) : 0;
            o["seen_for_sec"] = seenForMs / 1000;
            o["approach_likely"] = a.approachLikely;
            if (a.approachLikely) o["approach_eta_min"] = a.approachEtaMin;
            o["cpa_relevant"] = a.cpaRelevant;
            if (a.cpaRelevant) o["cpa_eta_min"] = a.cpaEtaMin;
            if (a.sessionMinDistanceKm >= 0) o["session_min_distance_km"] = a.sessionMinDistanceKm;
            if (a.sessionMaxSpeedKt >= 0) o["session_max_speed_kt"] = a.sessionMaxSpeedKt;
        }
        AircraftTable::unlock();

        String out;
        serializeJson(doc, out);
        server.send(200, "application/json", out);
    }

    // Feature 12 "PWA fuer die Web-UI" - drei neue, rein statische Routen
    // (Manifest/Icon/Service-Worker). Alle drei Anfragen sind komplett
    // unabhaengig von der AircraftTable/SD-Karte - kein SdMutex/AircraftTable-
    // Lock noetig, anders als die meisten anderen Handler hier in der Datei.

    // Web-App-Manifest (offizielles Format, siehe
    // https://developer.mozilla.org/en-US/docs/Web/Manifest) - "theme_color"
    // UND "background_color" folgen dem aktuellen Geraete-Farbthema (gleiche
    // WebTheme-Quelle wie das "theme-color"-Meta in htmlHeader() und die
    // CSS-Variablen der Seite selbst), damit die Splash-/Statusleisten-Farbe
    // beim App-Start zur Seite passt. "start_url":"/" fuehrt nach dem
    // Start immer zur Live-Radar-Startseite, unabhaengig davon, von welcher
    // Unterseite aus installiert wurde.
    void handleManifest() {
        WebTheme wt = currentWebTheme();
        JsonDocument doc;
        doc["name"] = "Eiswolfs Flightradar";
        doc["short_name"] = "Flightradar";
        doc["start_url"] = "/";
        doc["display"] = "standalone";
        doc["background_color"] = "#0a0f0d";
        doc["theme_color"] = wt.accent;
        JsonArray icons = doc["icons"].to<JsonArray>();
        JsonObject icon = icons.add<JsonObject>();
        icon["src"] = "/icon.png";
        icon["sizes"] = "192x192";
        icon["type"] = "image/png";
        icon["purpose"] = "any";

        String out;
        serializeJson(doc, out);
        // Offizieller MIME-Type fuer Web-App-Manifeste - manche Browser
        // pruefen ihn beim Installierbarkeits-Check, "application/json"
        // waere hier nicht 1:1 spezifikationskonform.
        server.send(200, "application/manifest+json", out);
    }

    // PNG-Icon aus dem Flash (PROGMEM, siehe web_pwa_icon.h) - dient sowohl
    // als Android/Chrome-Manifest-Icon als auch als iOS "apple-touch-icon"
    // (beide referenzieren dieselbe Route, siehe htmlHeader()/
    // handleManifest()) sowie als normales Browser-Favicon.
    void handleIcon() {
        server.send_P(200, "image/png", (PGM_P)PWA_ICON_PNG, PWA_ICON_PNG_LEN);
    }

    // Alex' Avatar-Logo (siehe web_avatar_logo.h) fuer den dezenten
    // GitHub-Link unten rechts auf der Live-Radar-Webseite (siehe
    // appendRadarSection()) - gleiches Ausliefer-Muster wie handleIcon()
    // oben.
    void handleAvatar() {
        server.send_P(200, "image/png", (PGM_P)WEB_AVATAR_LOGO_PNG, WEB_AVATAR_LOGO_PNG_LEN);
    }

    // Minimaler Service Worker fuers Offline-Caching der SEITENHUELLE
    // (HTML/CSS/JS-Grundgeruest von "/", plus Manifest/Icon) - bewusst NUR
    // diese drei URLs, alles andere (insbesondere /radar.json, aber auch
    // /lists, /export.csv, /csv und die Formular-POST-Routen) wird NIE
    // abgefangen und geht immer direkt/frisch ans Geraet, siehe Alex'
    // ausdrueckliche Vorgabe "nicht die Live-Daten selbst cachen". Netzwerk-
    // zuerst-mit-Cache-Fallback (statt Cache-zuerst): sobald das Geraet
    // erreichbar ist, bekommt der Nutzer immer die aktuelle, frisch vom
    // Geraet gerenderte Seite (inkl. z.B. des aktuellen Farbthemas) - nur
    // wenn das Geraet gerade NICHT erreichbar ist (z.B. Handy nicht mehr im
    // selben WLAN), springt die zuletzt zwischengespeicherte Version ein,
    // statt nur eine leere Fehlerseite zu zeigen.
    //
    // WICHTIG: Service Worker sind nur in einem "sicheren Kontext" (HTTPS
    // oder "localhost") verfuegbar - das Geraet wird ausschliesslich ueber
    // eine reine HTTP-IP-Adresse im lokalen WLAN erreicht, dort registriert
    // navigator.serviceWorker.register() (siehe htmlHeader()) in den
    // meisten Browsern (u.a. Chrome) gar nicht erst erfolgreich, das
    // Offline-Caching bleibt dort also praktisch wirkungslos. Trotzdem
    // sinnvoll: (a) iOS Safaris "Zum Home-Bildschirm"-Vollbildmodus haengt
    // NICHT vom Service Worker ab (siehe htmlHeader()-Kommentar), (b) minimal
    // im Flash, (c) funktioniert sofort, falls die Seite doch einmal ueber
    // HTTPS erreichbar gemacht wird (z.B. Reverse-Proxy).
    void handleServiceWorker() {
        String js;
        js.reserve(2200);
        js += "const CACHE_NAME='eiswolfs-flightradar-shell-v1';";
        js += "const SHELL_URLS=['/','/manifest.json','/icon.png'];";
        js += "self.addEventListener('install',function(event){";
        js += "self.skipWaiting();";
        js += "event.waitUntil(caches.open(CACHE_NAME).then(function(cache){return cache.addAll(SHELL_URLS);}));";
        js += "});";
        js += "self.addEventListener('activate',function(event){";
        js += "self.clients.claim();";
        js += "event.waitUntil(caches.keys().then(function(keys){";
        js += "return Promise.all(keys.filter(function(k){return k!==CACHE_NAME;}).map(function(k){return caches.delete(k);}));";
        js += "}));";
        js += "});";
        // Netzwerk-Timeout fuer den Fetch-Handler unten (Alex' Meldung:
        // ~30s schwarzer Bildschirm bei JEDEM Start der installierten iOS-
        // Standalone-App, nicht nur beim allerersten - widerlegt die zuerst
        // vermutete einmalige "Lokales Netzwerk"-Berechtigungsverhandlung.
        // Naheliegendste verbleibende Erklaerung: iOS beendet den WKWebView-
        // Prozess einer Standalone-Homescreen-App zwischen den Aufrufen
        // komplett (kein dauerhaft laufender Hintergrundprozess wie bei
        // einer nativen App) - jeder Start baut die Verbindung zur lokalen
        // Geraete-IP darum tatsaechlich JEDES Mal neu auf, nicht nur beim
        // ersten Mal. Das ist weiterhin eine iOS-Netzwerk-Eigenheit, kein
        // Server-/Fetch-Bug - der Fix hier repariert die Ursache nicht,
        // sorgt aber dafuer, dass die Seite trotzdem zuegig etwas anzeigt,
        // sobald ein Huellen-Cache vom letzten Aufruf vorliegt, statt
        // unbegrenzt auf die langsame Verbindung zu warten.
        js += "const NETWORK_TIMEOUT_MS=4000;";
        js += "self.addEventListener('fetch',function(event){";
        js += "var url=new URL(event.request.url);";
        // Nur GET-Anfragen auf genau die drei Huellen-URLs abfangen - alles
        // andere (insbesondere /radar.json) unangetastet an den Browser
        // durchreichen (kein respondWith() = normales Netzwerkverhalten).
        js += "if(event.request.method!=='GET'||SHELL_URLS.indexOf(url.pathname)===-1){return;}";
        js += "var networkPromise=fetch(event.request).then(function(response){";
        js += "var copy=response.clone();";
        js += "caches.open(CACHE_NAME).then(function(cache){cache.put(event.request,copy);});";
        js += "return response;";
        js += "});";
        // Promise.race() gegen einen kurzen Timeout - laeuft der Timeout
        // zuerst ab (Netzwerk noch nicht fertig), wird SOFORT auf den Cache
        // zurueckgegriffen, statt weiter zu warten. networkPromise laeuft im
        // Hintergrund einfach weiter (fuellt bei Erfolg trotzdem noch den
        // Cache fuer den naechsten Aufruf) - wird aber, sobald ein Cache-
        // Treffer vorliegt, NICHT mehr fuer die aktuelle Antwort abgewartet.
        // Ohne vorhandenen Cache-Eintrag (z.B. beim allerersten Aufruf
        // ueberhaupt) bleibt networkPromise selbst der Fallback - besser als
        // gar keine Antwort. Bei einem echten Fetch-FEHLSCHLAG (Ablehnung,
        // nicht nur Langsamkeit) entscheidet Promise.race() sofort zugunsten
        // der Ablehnung (ein abgelehntes Promise "gewinnt" das Rennen genau
        // wie ein erfuelltes) - der aeussere catch() greift dann direkt auf
        // den Cache zurueck, der ungenutzte timeoutPromise verfaellt einfach
        // folgenlos im Hintergrund.
        js += "var timeoutPromise=new Promise(function(resolve){setTimeout(function(){resolve(null);},NETWORK_TIMEOUT_MS);});";
        js += "event.respondWith(Promise.race([networkPromise,timeoutPromise]).then(function(result){";
        js += "if(result)return result;";
        js += "return caches.match(event.request).then(function(cached){return cached||networkPromise;});";
        js += "}).catch(function(){return caches.match(event.request);}));";
        js += "});";
        server.send(200, "application/javascript", js);
    }

    void handleNotFound() {
        server.send(404, "text/plain", "Not found");
    }
}

void begin() {
    server.on("/", handleRoot);
    server.on("/manifest.json", handleManifest);
    server.on("/icon.png", handleIcon);
    server.on("/avatar.png", handleAvatar);
    server.on("/sw.js", handleServiceWorker);
    server.on("/radar.json", handleRadarJson);
    server.on("/export.csv", handleExportCsv);
    server.on("/csv", HTTP_GET, handleCsvDownload);
    server.on("/logbook/delete", HTTP_POST, handleLogbookDelete);
    server.on("/lists", handleLists);
    server.on("/lists/airlines/add", HTTP_POST, handleAirlineAdd);
    server.on("/lists/airlines/delete", HTTP_POST, handleAirlineDelete);
    server.on("/lists/watchlist/add", HTTP_POST, handleWatchlistAdd);
    server.on("/lists/watchlist/delete", HTTP_POST, handleWatchlistDelete);
    server.on("/control/range", HTTP_POST, handleControlRange);
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
