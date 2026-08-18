#include "ota_update.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
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
    bool lastUpdateAvailable = false;
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
            lastUpdateAvailable = true;
            strncpy(lastAvailableVersion, info.latestVersion, sizeof(lastAvailableVersion) - 1);
            lastAvailableVersion[sizeof(lastAvailableVersion) - 1] = 0;
        } else if (info.result == CheckResult::UpToDate) {
            lastUpdateAvailable = false;
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
    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError jsonErr = deserializeJson(doc, body);
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
    return lastUpdateAvailable;
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
    // erneut raten zu muessen.
    Serial.printf("[OTA] Start: url=%s freeHeap=%u RSSI=%ddBm\n", url,
                  (unsigned)ESP.getFreeHeap(), WiFi.RSSI());

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
    httpUpdate.onProgress([onProgress](int cur, int total) {
        if (onProgress && total > 0) {
            onProgress((uint8_t)((cur * 100) / total));
        }
        // Nur gelegentlich loggen (alle ~10%), sonst quillt der Seriell-
        // Monitor bei grossen Dateien mit hunderten Zeilen ueber.
        static int8_t lastLoggedPercent = -1;
        if (total > 0) {
            int8_t pct = (int8_t)((cur * 100) / total);
            if (pct != lastLoggedPercent && pct % 10 == 0) {
                lastLoggedPercent = pct;
                Serial.printf("[OTA] Fortschritt: %d%% (%d/%d Bytes) freeHeap=%u\n", pct, cur, total,
                              (unsigned)ESP.getFreeHeap());
            }
        }
    });

    t_httpUpdate_return result = httpUpdate.update(client, url);

    if (result != HTTP_UPDATE_OK) {
        Serial.printf("[OTA] Fehlgeschlagen: result=%d error=%d (%s) freeHeap=%u\n", (int)result,
                      httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str(),
                      (unsigned)ESP.getFreeHeap());
    } else {
        Serial.println("[OTA] Erfolgreich heruntergeladen und geflasht.");
    }

    return result == HTTP_UPDATE_OK;
}

}
