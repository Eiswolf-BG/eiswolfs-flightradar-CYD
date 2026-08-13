#include "ota_update.h"
#include "config.h"
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
}

CheckInfo checkForUpdate() {
    CheckInfo info;

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(8000);

    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(client, RELEASES_API_URL)) return info;
    http.addHeader("User-Agent", USER_AGENT);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return info;
    }
    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) return info;

    const char* tag = doc["tag_name"] | "";
    if (!tag[0]) return info;

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

    if (!info.downloadUrl[0]) return info; // Release ohne firmware.bin-Anhang

    int cmp = compareVersions(info.latestVersion, Config::APP_VERSION);
    info.result = (cmp > 0) ? CheckResult::UpdateAvailable : CheckResult::UpToDate;
    return info;
}

bool performUpdate(const char* url, void (*onProgress)(uint8_t percent)) {
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
    });

    t_httpUpdate_return result = httpUpdate.update(client, url);
    return result == HTTP_UPDATE_OK;
}

}
