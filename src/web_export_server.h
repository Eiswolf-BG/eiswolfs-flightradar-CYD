#pragma once

namespace WebExportServer {
    void begin();
    void update();

    // True, solange die "/radar.json"-Route in letzter Zeit (Zeitfenster
    // siehe web_export_server.cpp) tatsaechlich abgefragt wurde - d.h.
    // jemand hat die WebUI-Startseite mit dem Live-Radar gerade offen. Von
    // NetTask genutzt, um die ADS-B-Abfrage-Reichweite nur dann auf die
    // maximale Config::RANGE_STEPS_KM-Stufe zu erweitern, wenn das WebUI
    // tatsaechlich aktiv genutzt wird - sonst bliebe die zusaetzliche
    // Netzwerk-/Speicherlast staendig bestehen, auch fuer Nutzer, die das
    // WebUI nie oeffnen.
    bool isRadarUiActive();
}
