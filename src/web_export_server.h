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

    // True (und setzt sich dabei zurueck), wenn seit dem letzten Aufruf ein
    // Fernsteuerungsbefehl (Farbschema, Reichweite, Mode-Checkboxen) via
    // Web-UI tatsaechlich angewendet wurde - von main.cpp::loop() (Core 1)
    // genutzt, um sofort ein forceRedraw auszuloesen, statt auf den
    // naechsten erfolgreichen ADS-B-Zyklus zu warten (siehe Bugfix-
    // Kommentar bei remoteSettingsChanged in web_export_server.cpp).
    bool consumeRemoteSettingsChanged();
}
