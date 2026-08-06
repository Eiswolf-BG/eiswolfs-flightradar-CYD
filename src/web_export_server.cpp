#include "web_export_server.h"
#include "config.h"
#include "flight_logbook.h"
#include "sd_mutex.h"
#include <WebServer.h>
#include <SD.h>

namespace WebExportServer {

namespace {
    constexpr uint8_t MAX_DAYS_QUERIED = 31;

    WebServer server(80);

    void handleRoot() {
        FlightLogbook::DayEntry days[MAX_DAYS_QUERIED];
        uint8_t count = FlightLogbook::listDays(days, MAX_DAYS_QUERIED);

        String html;
        html += "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
        html += "<title>Eiswolfs Flightradar - Logbook Export</title>";
        html += "<style>";
        html += "body{background-color:#0a0f0d;color:#39ff14;font-family:'Courier New',Courier,monospace;padding:20px;}";
        html += "h1{font-size:20px;}";
        html += "table{border-collapse:collapse;margin-top:10px;}";
        html += "td,th{padding:4px 12px;text-align:left;border-bottom:1px solid #1f3a2b;}";
        html += "a{color:#39ff14;}";
        html += "</style></head><body>";
        html += "<h1>Eiswolfs Flightradar - Logbook Export</h1>";

        if (count == 0) {
            html += "<p>No logbook entries yet.</p>";
        } else {
            html += "<table><tr><th>Date</th><th>Aircraft</th></tr>";
            for (uint8_t i = 0; i < count; i++) {
                html += "<tr><td>" + String(days[i].date) + "</td><td>" + String(days[i].count) + "</td></tr>";
            }
            html += "</table>";
            html += "<p><a href=\"/export.csv\">Download merged CSV (all days)</a></p>";
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
            }
            f.close();
        }
    }

    void handleNotFound() {
        server.send(404, "text/plain", "Not found");
    }
}

void begin() {
    server.on("/", handleRoot);
    server.on("/export.csv", handleExportCsv);
    server.onNotFound(handleNotFound);
    server.begin();
}

void update() {
    server.handleClient();
}

}
