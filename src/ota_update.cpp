#include "ota_update.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace OtaUpdate {

namespace {
    constexpr const char* RELEASES_API_URL =
        "https://api.github.com/repos/Eiswolf-BG/eiswolfs-flightradar-CYD/releases/latest";
    // GitHub verlangt bei API-Anfragen einen aussagekraeftigen User-Agent
    // (sonst HTTP 403) - gleiches Prinzip wie Config::NOMINATIM_USER_AGENT
    // fuer die Adresssuche.
    constexpr const char* USER_AGENT =
        "EiswolfsFlightradarCYD-OTA/1.0 (+https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD)";

    bool parseVersion(const char* s, int& major, int& minor, int& patch) {
        major = minor = patch = 0;
        if (!s || !s[0]) return false;
        if (s[0] == 'v' || s[0] == 'V') s++;
        return sscanf(s, "%d.%d.%d", &major, &minor, &patch) == 3;
    }

    // > 0 wenn a neuer als b, 0 wenn gleich, < 0 wenn a aelter als b.
    // Bewusst eine echte numerische Versionsvergleich statt eines simplen
    // String-Vergleichs (der wuerde z.B. "3.10.0" faelschlich als "kleiner"
    // als "3.9.0" einordnen).
    int compareVersions(const char* a, const char* b) {
        int aMaj, aMin, aPat, bMaj, bMin, bPat;
        if (!parseVersion(a, aMaj, aMin, aPat) || !parseVersion(b, bMaj, bMin, bPat)) return 0;
        if (aMaj != bMaj) return aMaj - bMaj;
        if (aMin != bMin) return aMin - bMin;
        return aPat - bPat;
    }

    // Zuletzt bekannter Stand - wird von JEDER erfolgreich abgeschlossenen
    // Pruefung aktualisiert (manueller Button-Check UND Hintergrund-Check,
    // siehe applyResult() unten), damit z.B. nach einem manuellen Check per
    // Button sofort auch die Badges (Menue-Button, "System"-Kachel,
    // Ruhebildschirm) den aktuellen Stand zeigen, ohne auf den naechsten
    // Hintergrund-Check warten zu muessen.
    // std::atomic statt einfachem bool: wird von checkForUpdate() aus dem
    // Hintergrund-Check (net_task.cpp::pollBackground(), Core 0) geschrieben
    // und von isUpdateAvailable() u.a. aus LedAlert::update() (Core 1, siehe
    // radar_screen.cpp::updateProximityAlert()) gelesen - echter Cross-Core-
    // Zugriff. memory_order_relaxed genuegt, da es sich um ein reines
    // Zustandsflag ohne Abhaengigkeit zu anderem Speicher handelt (gleiches
    // Prinzip wie heartbeatStartMs in led_alert.cpp).
    std::atomic<bool> lastUpdateAvailable{false};
    char lastAvailableVersion[16] = {0};
    uint32_t lastBackgroundCheckMs = 0;

    // Uebernimmt das Ergebnis einer abgeschlossenen Pruefung in den
    // geteilten Stand - bei CheckResult::Error bewusst NICHTS aendern (der
    // vorherige, zuletzt tatsaechlich bekannte Stand bleibt gueltig, bis
    // eine erfolgreiche Pruefung ihn ersetzt; ein einzelner fehlgeschlagener
    // Hintergrund-Check - z.B. ein kurzer WLAN-Hakler - soll nicht einfach
    // einen bereits bekannten Update-Hinweis verschwinden lassen).
    void applyResult(const CheckInfo& info) {
        if (info.result == CheckResult::UpdateAvailable) {
            lastUpdateAvailable.store(true, std::memory_order_relaxed);
            strncpy(lastAvailableVersion, info.latestVersion, sizeof(lastAvailableVersion) - 1);
            lastAvailableVersion[sizeof(lastAvailableVersion) - 1] = 0;
        } else if (info.result == CheckResult::UpToDate) {
            lastUpdateAvailable.store(false, std::memory_order_relaxed);
            lastAvailableVersion[0] = 0;
        }
    }
}

CheckInfo checkForUpdate() {
    CheckInfo info;

    Serial.printf("[OTA] Pruefe auf Update: url=%s freeHeap=%u RSSI=%ddBm\n", RELEASES_API_URL,
                  (unsigned)ESP.getFreeHeap(), WiFi.RSSI());

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(8000);

    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(client, RELEASES_API_URL)) {
        Serial.println("[OTA] Pruefung fehlgeschlagen: http.begin() lieferte false.");
        return info;
    }
    http.addHeader("User-Agent", USER_AGENT);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        if (code < 0) {
            Serial.printf("[OTA] Pruefung fehlgeschlagen: HTTP-Fehler=%d (%s)\n", code,
                          HTTPClient::errorToString(code).c_str());
        } else {
            Serial.printf("[OTA] Pruefung fehlgeschlagen: HTTP-Status=%d\n", code);
        }
        http.end();
        return info;
    }
    // Nur die Felder einlesen, die wir wirklich brauchen (tag_name,
    // assets[].name, assets[].browser_download_url), statt der kompletten
    // GitHub-Release-Antwort (die u.a. Beschreibungstext, Autor-Info,
    // Uploader-Avatare etc. enthaelt - fuer ein Release mit langen
    // Release-Notes durchaus mehrere KB). Direkt vom Stream geparst statt
    // erst per http.getString() komplett in einen String zu laden - beides
    // zusammen senkt den Speicherbedarf dieser Pruefung deutlich. Wichtig
    // seit pollBackground() (siehe oben) alle paar Minuten im Hintergrund
    // laeuft: ein kurzzeitig knapper Heap durch eine unnoetig grosse
    // Zwischenkopie auf Core 0 kann gleichzeitige Speicher-/String-Arbeit
    // auf Core 1 (z.B. den Ruhebildschirm-Text) beeintraechtigen - genau das
    // hat Alex nach Einfuehrung der Hintergrund-Pruefung als gelegentlich
    // "zusammengeschobene" Ziffern auf dem Ruhebildschirm gemeldet.
    JsonDocument filter;
    filter["tag_name"] = true;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;

    JsonDocument doc;
    DeserializationError jsonErr =
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (jsonErr) {
        Serial.printf("[OTA] Pruefung fehlgeschlagen: JSON-Fehler (%s)\n", jsonErr.c_str());
        return info;
    }

    const char* tag = doc["tag_name"] | "";
    if (!tag[0]) {
        Serial.println("[OTA] Pruefung fehlgeschlagen: kein tag_name im Release-JSON.");
        return info;
    }

    strncpy(info.latestVersion, (tag[0] == 'v' || tag[0] == 'V') ? tag + 1 : tag,
            sizeof(info.latestVersion) - 1);

    JsonArray assets = doc["assets"];
    for (JsonObject asset : assets) {
        const char* name = asset["name"] | "";
        if (strcmp(name, "firmware.bin") == 0) {
            const char* url = asset["browser_download_url"] | "";
            strncpy(info.downloadUrl, url, sizeof(info.downloadUrl) - 1);
            break;
        }
    }

    if (!info.downloadUrl[0]) {
        Serial.printf("[OTA] Pruefung fehlgeschlagen: Release v%s hat keinen firmware.bin-Anhang.\n",
                      info.latestVersion);
        return info; // Release ohne firmware.bin-Anhang
    }

    int cmp = compareVersions(info.latestVersion, Config::APP_VERSION);
    info.result = (cmp > 0) ? CheckResult::UpdateAvailable : CheckResult::UpToDate;
    Serial.printf("[OTA] Pruefung erfolgreich: installiert=v%s neuestes=v%s -> %s\n", Config::APP_VERSION,
                  info.latestVersion,
                  info.result == CheckResult::UpdateAvailable ? "Update verfuegbar" : "bereits aktuell");
    applyResult(info);
    return info;
}

void pollBackground() {
    uint32_t now = millis();
    if (now - lastBackgroundCheckMs < Config::OTA_BACKGROUND_CHECK_INTERVAL_MS) return;
    lastBackgroundCheckMs = now;

    // Kein WLAN -> gar nicht erst versuchen, einfach beim naechsten
    // Intervall wieder pruefen (kein Fehlerfall, passiert z.B. regelmaessig
    // kurz nach dem Booten, bevor WifiMgr verbunden hat).
    if (WiFi.status() != WL_CONNECTED) return;

    checkForUpdate(); // aktualisiert lastUpdateAvailable/lastAvailableVersion via applyResult()
}

bool isUpdateAvailable() {
    return lastUpdateAvailable.load(std::memory_order_relaxed);
}

const char* availableVersion() {
    return lastAvailableVersion;
}

bool performUpdate(const char* url, void (*onProgress)(uint8_t percent)) {
    // Diagnose-Logging (nur ueber USB-Seriell sichtbar, kein Einfluss auf
    // die UI) - vorher wurde bei einem Fehlschlag nur ein simples "true/
    // false" nach aussen gegeben, ohne den eigentlichen Grund (Timeout,
    // TLS-Fehler, HTTP-Statuscode...) festzuhalten. Damit laesst sich ein
    // fehlgeschlagener OTA-Versuch am Seriell-Monitor nachvollziehen, statt
    // erneut raten zu muessen. maxAlloc (groesster zusammenhaengender freier
    // Speicherblock) bewusst dauerhaft mitgeloggt, nicht nur testweise
    // (Alex' Meldung zu haengenden/fehlschlagenden Updates, siehe Diagnose
    // im Chat): Update.begin() braucht selbst nur SPI_FLASH_SEC_SIZE
    // (4096 Byte, siehe Updater.cpp) - ein "malloc failed" dort trotz
    // gesund aussehendem freeHeap waere ein Fragmentierungs-Indiz (gleiche
    // Bugklasse wie die bereits behobenen SSL-Speicherfehler bei Weather/
    // ADS-B). Kostet nichts, spart aber bei einem kuenftigen Wiederauftreten
    // dieses SELTENEN Fehlerbilds einen erneuten Diagnose-Umweg (extra
    // Serial-Mitschnitt), da der Wert dann schon im ganz normalen Log steht.
    Serial.printf("[OTA] Start: url=%s freeHeap=%u maxAlloc=%u RSSI=%ddBm\n", url,
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), WiFi.RSSI());

    // Diagnose-Fund (Alex' Meldung): wiederkehrende 600-1600ms-Aussetzer
    // alle ~16KB waehrend des firmware.bin-Downloads, dazwischen laeuft er
    // fluessig - ein direkter curl-Download derselben Datei vom PC aus war
    // dagegen durchgehend schnell, also kein Netzwerk-/Server-Problem. Das
    // Muster passt zum ESP32-WLAN-Modem-Sleep (Arduino/PlatformIO-Default
    // an): das WLAN-Modul schlaeft zwischen Beacon-Intervallen periodisch
    // kurz ein, um Strom zu sparen - bei einem am Stueck laufenden Download
    // genau die beobachteten wiederkehrenden Pausen. Fuer die Dauer des
    // eigentlichen Downloads/Flashens deshalb abgeschaltet, der zuvor
    // aktive Zustand wird unten (nach httpUpdate.update(), unabhaengig vom
    // Ergebnis) wiederhergestellt - ein fehlgeschlagenes Update soll nicht
    // dauerhaft mit abgeschaltetem Stromsparmodus weiterlaufen.
    bool prevWifiSleep = WiFi.getSleep();
    WiFi.setSleep(false);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15000);

    // WICHTIG: GitHubs "browser_download_url" fuer Release-Assets ist KEIN
    // direkter Download-Link, sondern liefert erst ein HTTP 301/302-Redirect
    // auf eine signierte objects.githubusercontent.com-URL. HTTPUpdate folgt
    // Redirects standardmaessig NICHT (HTTPC_DISABLE_FOLLOW_REDIRECTS ist der
    // Default) - ohne diese Zeile bricht der Download mit HTTP_UPDATE_FAILED
    // ab, weil statt der .bin-Datei nur die Redirect-Antwort ankommt. Siehe
    // z.B. espressif/arduino-esp32#3020. HTTPC_STRICT_FOLLOW_REDIRECTS
    // reicht, da wir nur GET verwenden.
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    // Wir zeigen nach erfolgreicher Installation selbst noch eine kurze
    // Erfolgsmeldung an, bevor das Geraet neu startet - siehe
    // menu_screen.cpp::runOtaUpdateScreen().
    httpUpdate.rebootOnUpdate(false);

    // Feingranulare Fortschritts-Zeitmessung (Alex' Wunsch, Diagnose der
    // extrem langsamen Downloads/Aussetzer im Chat) - loggt Zeitstempel
    // relativ zum Download-Start alle ~100KB, unabhaengig vom UI-Callback
    // (der nur die grobe Prozentzahl fuers Display braucht). Damit laesst
    // sich am Seriell-Monitor objektiv sehen, ob ein Download gleichmaessig
    // durchlaeuft oder irgendwo stockt/aussetzt, ohne dafuer extra einen
    // neuen Diagnose-Durchgang aufsetzen zu muessen. Lambda faengt
    // progressStartMs/lastLoggedStep per Referenz - unkritisch, da
    // httpUpdate.update() unten blockierend im selben Stack-Frame laeuft,
    // die Lambda also nie ueber das Ende dieser Funktion hinaus existiert.
    constexpr int PROGRESS_LOG_STEP_BYTES = 100 * 1024;
    uint32_t progressStartMs = millis();
    int lastLoggedStep = -1;
    httpUpdate.onProgress([onProgress, progressStartMs, &lastLoggedStep](int cur, int total) {
        int step = cur / PROGRESS_LOG_STEP_BYTES;
        if (step != lastLoggedStep) {
            lastLoggedStep = step;
            Serial.printf("[OTA] Fortschritt: %d/%d Bytes bei %ums seit Start\n", cur, total,
                          (unsigned)(millis() - progressStartMs));
        }
        if (onProgress && total > 0) onProgress((uint8_t)((cur * 100) / total));
    });

    t_httpUpdate_return result = httpUpdate.update(client, url);

    // Modem-Sleep wieder auf den Zustand von vor dem Download zuruecksetzen
    // (siehe Kommentar bei WiFi.setSleep(false) oben) - unabhaengig vom
    // Ergebnis, damit ein fehlgeschlagenes Update nicht dauerhaft mit
    // abgeschaltetem Stromsparmodus weiterlaeuft.
    WiFi.setSleep(prevWifiSleep);

    if (result != HTTP_UPDATE_OK) {
        Serial.printf("[OTA] Fehlgeschlagen: result=%d error=%d (%s) freeHeap=%u maxAlloc=%u\n", (int)result,
                      httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str(),
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    } else {
        Serial.println("[OTA] Erfolgreich heruntergeladen und geflasht.");
    }

    return result == HTTP_UPDATE_OK;
}

}
