#include "i18n.h"
#include "i18n_en.h"
#include "settings_store.h"
#include "config.h"
#include "sd_storage.h"
#include "sd_mutex.h"
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <cstring>
#include <cstdlib>
#include <atomic>

// SD-basierte Sprachauslagerung (Alex' Wunsch, Flash-Sparziel Teil 2 nach
// der Flughafendatenbank/dem GitHub-Logo) - NUR Englisch (I18N_EN, siehe
// i18n_en.h) bleibt fest im Flash einkompiliert. Die anderen 7 Sprachen
// (i18n_de.h/fr.h/tr.h/es.h/it.h/pt.h/nl.h) EXISTIEREN weiterhin als
// Quelldateien im Repo - sie sind die von Menschen gepflegte "Wahrheit" fuer
// jede Uebersetzung UND die Vorlage fuer den Python-Generator (siehe unten),
// werden aber ABSICHTLICH NICHT MEHR HIER EINGEBUNDEN, damit ihr Inhalt
// nicht mehr mit ins Flash-Image kompiliert wird. Stattdessen liegen sie als
// assets/lang_XX.bin auf GitHub, werden einmalig auf die SD-Karte
// heruntergeladen und bei Bedarf von dort in den RAM geladen.
//
// Dateiformat lang_XX.bin (siehe scratchpad-Generatorskript
// gen_lang_bin.py, das dieses Format erzeugt - ausserhalb des Firmware-
// Builds, wird von Karl bei Bedarf manuell erneut ausgefuehrt, wenn sich
// uebersetzter Text aendert):
//   Offset 0:  Magic "LNG2" (4 Byte)
//   Offset 4:  Format-Version (1 Byte) = 1
//   Offset 5:  Sprachindex (1 Byte, EN=0..NL=7, siehe TABLES-Reihenfolge
//              unten)
//   Offset 6:  Inhalts-Version (uint16 LE) = Config::I18N_CONTENT_VERSION
//              zum Erzeugungszeitpunkt (siehe dortiger Kommentar - bewusst
//              NICHT an APP_VERSION gekoppelt)
//   Offset 8:  Anzahl Strings (uint16 LE) - MUSS zu StringId::COUNT passen
//   Offset 10: [Anzahl] x uint16 LE Byte-Laengen, eine je String in
//              StringId-Reihenfolge
//   Offset 10+Anzahl*2: alle Strings aneinandergereiht als UTF-8-Bytes,
//              OHNE Null-Terminierung (wird beim Laden in den RAM ergaenzt)
//
// Jede der drei Kennungen (Format-Version, Sprachindex, Anzahl) UND die
// Inhalts-Version werden beim Laden/bei der Aktualitaetspruefung
// UNBEDINGT gegengeprueft (siehe langFileUpToDate()/loadLangTable() unten)
// - jede Abweichung (z.B. nach einem Firmware-Update, das neue StringIds
// hinzugefuegt hat) gilt als "veraltet" und loest automatisch einen
// erneuten Download aus, OHNE dass die anderen, noch aktuellen
// Sprachdateien angefasst werden muessen.
namespace I18n {

namespace {
    const char* const LANGUAGE_NAMES[LANG_COUNT] = {
        "English", "Deutsch", "Français", "Türkçe", "Español", "Italiano", "Português", "Nederlands"
    };

    // Kurzcode je Sprache fuer Dateinamen (lang_XX.bin) UND die GitHub-Asset-
    // URLs - MUSS zur TABLES-Reihenfolge und zum Generator-Skript passen.
    const char* const LANG_FILE_SUFFIX[LANG_COUNT] = {
        "en", "de", "fr", "tr", "es", "it", "pt", "nl"
    };

    constexpr uint8_t LANG_FILE_MAGIC[4] = {'L', 'N', 'G', '2'};
    constexpr uint8_t LANG_FILE_FORMAT_VERSION = 1;
    constexpr size_t LANG_FILE_HEADER_SIZE = 10; // Magic(4)+FormatVer(1)+LangIdx(1)+ContentVer(2)+Count(2)

    // Die im RAM aktive, von der SD geladene Sprachtabelle - GENAU EINE
    // gleichzeitig (die aktuell in den Einstellungen gewaehlte Sprache).
    // Englisch braucht diese Struktur nie (i18n_en.h liegt ja bereits
    // kompiliert vor) - aktiv ist sie NUR fuer languageIdx 1..7, und auch
    // dann nur, wenn das Laden tatsaechlich erfolgreich war. Ausschliesslich
    // von Core 1 (UI) beschrieben - siehe loadActiveLanguageFromSd()/
    // onLanguageChanged()/consumeReloadPending() - net_task.cpp (Core 0)
    // fasst NUR die SD-Dateien selbst an, nie diese RAM-Struktur, damit hier
    // keine Cross-Core-Absicherung noetig ist.
    struct SdTable {
        bool loaded = false;
        uint8_t langIndex = 0;
        char** pointers = nullptr; // [count] Zeiger in blob hinein
        char* blob = nullptr;      // ein grosser, Null-terminierter Puffer
        uint16_t count = 0;
    };
    SdTable activeSdTable;

    // Von retryLanguageDownloadsIfNeeded() (Core 0) gesetzt, wenn gerade die
    // AKTUELL aktive Sprache frisch heruntergeladen wurde - reines
    // Bool-Flag, kein Zugriff auf die RAM-Tabelle selbst, deshalb genuegt
    // std::atomic<bool> ohne zusaetzlichen Mutex.
    std::atomic<bool> reloadPending{false};

    void freeSdTable(SdTable& t) {
        if (t.pointers) { free(t.pointers); t.pointers = nullptr; }
        if (t.blob) { free(t.blob); t.blob = nullptr; }
        t.loaded = false;
        t.count = 0;
    }

    void buildLangPath(uint8_t langIndex, char* out, size_t outSize) {
        snprintf(out, outSize, "%s/lang_%s.bin", Config::SD_ROOT_DIR, LANG_FILE_SUFFIX[langIndex]);
    }

    // Liest NUR die Kopfzeile (10 Byte) und prueft Magic/Format-Version/
    // Sprachindex/Inhalts-Version/Anzahl gegen die aktuell laufende
    // Firmware - schneller Check ohne die eigentlichen Textdaten zu lesen,
    // fuer den haeufigen "ist alles schon aktuell?"-Fall in
    // retryLanguageDownloadsIfNeeded().
    bool langFileUpToDate(uint8_t langIndex) {
        char path[48];
        buildLangPath(langIndex, path, sizeof(path));

        SdMutex::Guard guard;
        if (!SD.exists(path)) return false;
        File f = SD.open(path, FILE_READ);
        if (!f) return false;
        uint8_t header[LANG_FILE_HEADER_SIZE];
        size_t n = f.read(header, sizeof(header));
        f.close();
        if (n != sizeof(header)) {
            Serial.printf("[I18n] '%s' veraltet: nur %u/%u Kopfzeilen-Byte gelesen\n",
                          LANG_FILE_SUFFIX[langIndex], (unsigned)n, (unsigned)sizeof(header));
            return false;
        }
        if (memcmp(header, LANG_FILE_MAGIC, 4) != 0) {
            Serial.printf("[I18n] '%s' veraltet: falsches Magic %02x%02x%02x%02x\n",
                          LANG_FILE_SUFFIX[langIndex], header[0], header[1], header[2], header[3]);
            return false;
        }
        if (header[4] != LANG_FILE_FORMAT_VERSION) {
            Serial.printf("[I18n] '%s' veraltet: Format-Version %u statt %u\n",
                          LANG_FILE_SUFFIX[langIndex], header[4], LANG_FILE_FORMAT_VERSION);
            return false;
        }
        if (header[5] != langIndex) {
            Serial.printf("[I18n] '%s' veraltet: Datei-Sprachindex %u statt %u\n",
                          LANG_FILE_SUFFIX[langIndex], header[5], langIndex);
            return false;
        }
        uint16_t contentVersion = (uint16_t)header[6] | ((uint16_t)header[7] << 8);
        uint16_t count = (uint16_t)header[8] | ((uint16_t)header[9] << 8);
        if (contentVersion != Config::I18N_CONTENT_VERSION) {
            Serial.printf("[I18n] '%s' veraltet: Inhalts-Version %u statt %u\n",
                          LANG_FILE_SUFFIX[langIndex], contentVersion, Config::I18N_CONTENT_VERSION);
            return false;
        }
        if (count != (uint16_t)StringId::COUNT) {
            Serial.printf("[I18n] '%s' veraltet: %u Strings statt %u\n",
                          LANG_FILE_SUFFIX[langIndex], count, (unsigned)StringId::COUNT);
            return false;
        }
        return true;
    }

    // Laedt eine bereits als aktuell bekannte lang_XX.bin komplett von der
    // SD in den RAM (Core 1 - siehe SdTable-Kommentar oben). Gibt bei
    // JEDEM Fehler (fehlende Datei, kaputte Struktur, Speichermangel)
    // sauber false zurueck und raeumt bereits allozierten Speicher wieder
    // auf - der Aufrufer bleibt in diesem Fall einfach bei Englisch.
    bool loadLangTable(uint8_t langIndex, SdTable& out) {
        char path[48];
        buildLangPath(langIndex, path, sizeof(path));

        SdMutex::Guard guard;
        if (!SD.exists(path)) return false;
        File f = SD.open(path, FILE_READ);
        if (!f) return false;

        uint8_t header[LANG_FILE_HEADER_SIZE];
        if (f.read(header, sizeof(header)) != sizeof(header)) { f.close(); return false; }
        if (memcmp(header, LANG_FILE_MAGIC, 4) != 0 || header[4] != LANG_FILE_FORMAT_VERSION ||
            header[5] != langIndex) {
            f.close();
            return false;
        }
        uint16_t contentVersion = (uint16_t)header[6] | ((uint16_t)header[7] << 8);
        uint16_t count = (uint16_t)header[8] | ((uint16_t)header[9] << 8);
        if (contentVersion != Config::I18N_CONTENT_VERSION || count != (uint16_t)StringId::COUNT) {
            f.close();
            return false;
        }

        uint16_t* lengths = (uint16_t*)malloc((size_t)count * sizeof(uint16_t));
        if (!lengths) { f.close(); return false; }
        if (f.read((uint8_t*)lengths, (size_t)count * 2) != (size_t)count * 2) {
            free(lengths);
            f.close();
            return false;
        }

        uint32_t totalBytes = 0;
        for (uint16_t i = 0; i < count; i++) totalBytes += lengths[i];

        // Rohe Textdaten in EINEM Rutsch lesen (statt bis zu ~530 einzelner
        // kleiner SD-Reads) - kurzlebiger temporaerer Puffer, wird direkt
        // im Anschluss wieder freigegeben.
        uint8_t* raw = (uint8_t*)malloc(totalBytes);
        if (!raw && totalBytes > 0) { free(lengths); f.close(); return false; }
        if (totalBytes > 0 && f.read(raw, totalBytes) != totalBytes) {
            free(lengths);
            free(raw);
            f.close();
            return false;
        }
        f.close();

        // Ziel-Puffer MIT einem Null-Byte je String (fuer gueltige
        // C-Strings, die Datei selbst speichert keine Terminatoren).
        char* blob = (char*)malloc((size_t)totalBytes + count);
        char** pointers = (char**)malloc((size_t)count * sizeof(char*));
        if (!blob || !pointers) {
            free(lengths);
            free(raw);
            if (blob) free(blob);
            if (pointers) free(pointers);
            return false;
        }

        uint32_t rawPos = 0;
        uint32_t blobPos = 0;
        for (uint16_t i = 0; i < count; i++) {
            memcpy(blob + blobPos, raw + rawPos, lengths[i]);
            blob[blobPos + lengths[i]] = 0;
            pointers[i] = blob + blobPos;
            rawPos += lengths[i];
            blobPos += lengths[i] + 1;
        }
        free(lengths);
        free(raw);

        out.loaded = true;
        out.langIndex = langIndex;
        out.blob = blob;
        out.pointers = pointers;
        out.count = count;
        return true;
    }

    // Blockierender HTTPS-Download EINER Sprachdatei - identisches Tmp-
    // Datei-dann-Umbenennen-Muster wie SdStorage::downloadAirportsToSd()
    // (inkl. delay(1)-Watchdog-Fix und exakter Content-Length-Pruefung,
    // siehe dortige Bugfix-Kommentare fuer die volle Herleitung). NUR aus
    // einem Core-0/Hintergrund-Kontext aufrufen.
    bool downloadLanguageToSd(uint8_t langIndex) {
        if (WiFi.status() != WL_CONNECTED) return false;

        try {
            char url[160];
            snprintf(url, sizeof(url),
                     "https://raw.githubusercontent.com/Eiswolf-BG/eiswolfs-flightradar-CYD/main/assets/lang_%s.bin",
                     LANG_FILE_SUFFIX[langIndex]);
            Serial.printf("[I18n] Lade Sprachdatei herunter: %s\n", url);

            WiFiClientSecure client;
            client.setInsecure();
            HTTPClient http;
            if (!http.begin(client, url)) return false;

            constexpr uint32_t TIMEOUT_MS = 20000;
            http.setTimeout(TIMEOUT_MS);
            http.setUserAgent("EiswolfsFlightradarCYD-Lang/1.0 (+https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD)");

            int code = http.GET();
            if (code != HTTP_CODE_OK) {
                Serial.printf("[I18n] Sprachdatei '%s' Download fehlgeschlagen: HTTP-Status=%d\n",
                              LANG_FILE_SUFFIX[langIndex], code);
                http.end();
                return false;
            }

            char tmpPath[48];
            snprintf(tmpPath, sizeof(tmpPath), "%s/lang_%s.tmp", Config::SD_ROOT_DIR, LANG_FILE_SUFFIX[langIndex]);
            char finalPath[48];
            buildLangPath(langIndex, finalPath, sizeof(finalPath));

            File out;
            {
                SdMutex::Guard guard;
                if (SD.exists(tmpPath)) SD.remove(tmpPath);
                out = SD.open(tmpPath, FILE_WRITE);
            }
            if (!out) {
                http.end();
                return false;
            }

            Stream& stream = http.getStream();
            int contentLen = http.getSize();
            uint8_t buf[512];
            int32_t written = 0;
            uint32_t startMs = millis();
            bool timedOut = false;
            while (http.connected() && (contentLen < 0 || written < contentLen)) {
                if (millis() - startMs > TIMEOUT_MS) {
                    timedOut = true;
                    break;
                }
                size_t avail = stream.available();
                if (!avail) {
                    delay(5);
                    continue;
                }
                size_t toRead = avail > sizeof(buf) ? sizeof(buf) : avail;
                int n = stream.readBytes(buf, toRead);
                if (n <= 0) break;
                {
                    SdMutex::Guard guard;
                    out.write(buf, n);
                }
                written += n;
                if (contentLen >= 0 && written >= contentLen) break;
                delay(1);
            }
            {
                SdMutex::Guard guard;
                out.close();
            }
            http.end();

            bool complete = (contentLen >= 0) ? (written == contentLen) : (written >= 100);
            if (timedOut || !complete) {
                Serial.printf("[I18n] Sprachdatei '%s' Download unvollstaendig (timedOut=%d, written=%d, "
                              "contentLen=%d) - verworfen\n",
                              LANG_FILE_SUFFIX[langIndex], (int)timedOut, (int)written, contentLen);
                SdMutex::Guard guard;
                SD.remove(tmpPath);
                return false;
            }

            SdMutex::Guard guard;
            if (SD.exists(finalPath)) SD.remove(finalPath);
            bool renamed = SD.rename(tmpPath, finalPath);
            Serial.printf("[I18n] Sprachdatei '%s' Download %s (%d Bytes)\n", LANG_FILE_SUFFIX[langIndex],
                          renamed ? "erfolgreich" : "fehlgeschlagen (Umbenennen)", written);
            return renamed;
        } catch (...) {
            Serial.printf("[I18n] Sprachdatei-Download: Exception abgefangen, freeHeap=%u maxAlloc=%u\n",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
            return false;
        }
    }

    uint32_t lastLangRetryMs = 0;
    // Gleiche Groessenordnung wie AIRPORTS_RETRY_INTERVAL_MS in
    // sd_storage.cpp - verhindert, dass ein fehlgeschlagener Download bei
    // jeder NetTask-Schleifeniteration sofort erneut versucht wird.
    constexpr uint32_t LANG_RETRY_INTERVAL_MS = 30000;

    // Rundlauf-Position fuer retryLanguageDownloadsIfNeeded() unten - LIVE
    // AM GERAET GEFUNDENER BUG (Alex' Testauftrag): ohne dieses Rundlaufen
    // wuerde die Suche nach einem noch fehlenden Slot IMMER bei Index 0
    // (Englisch) neu beginnen: ist genau DIESE eine Datei dauerhaft nicht
    // erreichbar (z.B. HTTP 404, live beobachtet - assets/lang_en.bin lag
    // zum Testzeitpunkt noch gar nicht auf GitHub), wuerden die anderen 7
    // tatsaechlich erreichbaren Sprachen NIE auch nur versucht. Wird direkt
    // NACH dem Auffinden eines Kandidaten weitergeschaltet (nicht erst nach
    // erfolgreichem Download) - ein dauerhaft fehlschlagender Slot blockiert
    // dadurch nie die anderen, wird aber nach einem vollen Rundlauf
    // trotzdem wieder mit versucht.
    uint8_t nextCheckSlot = 0;
}

const char* t(StringId id) {
    uint16_t idx = (uint16_t)id;
    if (idx >= (uint16_t)StringId::COUNT) return "?";

    // Aktive SD-Sprache (falls erfolgreich geladen) hat Vorrang - deckt
    // Sprachindex 1..7 ab. Englisch (Index 0) nutzt IMMER direkt die
    // kompilierte Tabelle, nie den SD-Umweg (waere nur redundanter
    // RAM-Verbrauch fuer identischen Inhalt).
    if (activeSdTable.loaded && idx < activeSdTable.count) {
        return activeSdTable.pointers[idx];
    }
    return I18N_EN[idx];
}

const char* languageName(uint8_t index) {
    if (index >= LANG_COUNT) return "?";
    return LANGUAGE_NAMES[index];
}

void loadActiveLanguageFromSd() {
    uint8_t lang = SettingsStore::language();
    if (lang >= LANG_COUNT) lang = 0;

    // Bereits aktiv geladene Tabelle (z.B. ein erneuter Aufruf nach einem
    // Sprachwechsel zurueck zur vorherigen Sprache) zuerst freigeben.
    freeSdTable(activeSdTable);

    if (lang == 0) return; // Englisch braucht keine SD-Tabelle.
    if (!SdStorage::isMounted()) return;
    if (!langFileUpToDate(lang)) return;

    SdTable fresh;
    if (loadLangTable(lang, fresh)) {
        activeSdTable = fresh;
        Serial.printf("[I18n] Sprache '%s' von SD geladen (%u Strings)\n", LANG_FILE_SUFFIX[lang],
                      (unsigned)fresh.count);
    }
    // Bei Fehlschlag bleibt activeSdTable.loaded=false - t() faellt dann
    // automatisch auf die kompilierte Englisch-Tabelle zurueck.
}

void onLanguageChanged() {
    loadActiveLanguageFromSd();
}

void retryLanguageDownloadsIfNeeded() {
    if (!SdStorage::isMounted()) return;

    // BUGFIX (live am Geraet gefunden, Alex' Testauftrag): der Drossel-
    // Check MUSS vor der Rundlauf-Suche stehen, nicht danach - sonst wuerde
    // die Suche (und das Weiterschalten von nextCheckSlot) bei JEDER
    // NetTask-Schleifeniteration laufen, auch waehrend der 30s-Sperrzeit
    // zwischen zwei echten Downloads. Da eine Schleifeniteration nur ein
    // paar Millisekunden dauert, haette das nextCheckSlot in dieser Zeit
    // tausendfach durch alle 8 Slots rotieren lassen - beim naechsten
    // TATSAECHLICHEN Downloadversuch waere dadurch praktisch ein
    // zufaelliger Slot drangekommen statt eines geordneten Rundlaufs von
    // "einer pro ~30 Sekunden".
    uint32_t now = millis();
    if (lastLangRetryMs != 0 && now - lastLangRetryMs < LANG_RETRY_INTERVAL_MS) return;

    // Rundlauf-Suche (siehe nextCheckSlot-Kommentar oben) statt immer bei
    // Index 0 neu zu beginnen - guenstige, rein lokale SD-Kopfzeilen-
    // Pruefung pro Slot, kein Netzwerkzugriff in dieser Schleife.
    int8_t needsDownload = -1;
    for (uint8_t step = 0; step < LANG_COUNT; step++) {
        uint8_t slot = (uint8_t)((nextCheckSlot + step) % LANG_COUNT);
        if (!langFileUpToDate(slot)) {
            needsDownload = (int8_t)slot;
            break;
        }
    }
    if (needsDownload < 0) return; // alle 8 bereits aktuell

    if (WiFi.status() != WL_CONNECTED) return;
    lastLangRetryMs = now;
    // Erst HIER weiterschalten (nicht schon waehrend der Suche oben) -
    // genau EIN Slot-Fortschritt pro tatsaechlichem Downloadversuch.
    nextCheckSlot = (uint8_t)((needsDownload + 1) % LANG_COUNT);

    bool ok = downloadLanguageToSd((uint8_t)needsDownload);
    if (ok && (uint8_t)needsDownload == SettingsStore::language()) {
        reloadPending.store(true, std::memory_order_relaxed);
    }
}

bool consumeReloadPending() {
    return reloadPending.exchange(false, std::memory_order_relaxed);
}

}
