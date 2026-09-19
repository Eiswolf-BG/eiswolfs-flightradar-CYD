#include "sd_storage.h"
#include "config.h"
#include "sd_mutex.h"
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <cstring>

namespace SdStorage {

namespace {
    bool mounted = false;
    SPIClass sdSpi(HSPI);
    const char* kDefaultAirlinesCsv =
        "icao,name\n"
        "BAW,British Airways\n"
        "SAA,South African Airways\n"
        "CAW,Comair\n"
        "FLY,Safair (FlySafair)\n"
        "KLM,KLM Royal Dutch Airlines\n"
        "DLH,Lufthansa\n"
        "UAE,Emirates\n"
        "QTR,Qatar Airways\n"
        "ETH,Ethiopian Airlines\n"
        "AFR,Air France\n"
        "SWR,Swiss International\n"
        "BAW,British Airways\n"
        "MSR,EgyptAir\n"
        "KQA,Kenya Airways\n"
        "UAL,United Airlines\n"
        "DAL,Delta Air Lines\n"
        "AAL,American Airlines\n";

    const char* kDefaultAircraftTypesCsv =
        "type,seats\n"
        "A320,180\n"
        "A321,220\n"
        "A319,140\n"
        "A332,278\n"
        "A333,277\n"
        "A359,314\n"
        "A388,469\n"
        "B738,189\n"
        "B737,148\n"
        "B739,180\n"
        "B77W,365\n"
        "B788,242\n"
        "B789,296\n"
        "E190,100\n"
        "CRJ2,50\n"
        "CRJ9,90\n"
        "DH8D,78\n";

    // Kopfzeile des aktuellen Binaerformats fuer die Flughafen-Datei (siehe
    // airport_lookup.cpp fuer den Parser): 4 Byte Magic "APR2" + 2 Byte
    // Datensatz-Anzahl (uint16 LE), danach je 12 Byte pro Flughafen (4 Byte
    // ICAO-ASCII + int32 LE Breitengrad + int32 LE Laengengrad, beide in
    // Mikrograd). Frueher lag hier eine reine Text-CSV
    // ("icao,name,lat,lon\n...") mit nur 34 handkuratierten Hubs - eine
    // Datei im alten Format beginnt nie mit diesen 4 Magic-Bytes, weshalb
    // die Pruefung unten ein Fehlen/Nicht-Uebereinstimmen dieser Kennung
    // zuverlässig als "muss durch die neue, weltweite Datenbank ersetzt
    // werden" erkennt - auch auf bereits eingerichteten Geraeten, ohne
    // Werksreset.
    //
    // BUGFIX (Alex' Wunsch, Flash-Sparziel): Die ~5025 Datensaetze (~60KB)
    // lagen bisher als kAirportsBin fest im Flash (airports_data.cpp/.h,
    // JETZT ENTFERNT) und dienten nur dazu, diese SD-Datei einmalig beim
    // allerersten Boot zu befuellen - der eigentliche Lookup (siehe
    // airport_lookup.cpp::findNearest()) liest ohnehin schon in kleinen
    // Bloecken direkt von der SD-Karte, NIE die komplette Tabelle ins RAM.
    // Die Befuellung passiert jetzt per einmaligem HTTPS-Download
    // (downloadAirportsToSd() unten) statt aus dem Flash - GENAU EIN
    // Download im gesamten Geraeteleben (nicht bei jedem Neustart wie beim
    // bewusst weiterhin unangetasteten Sprachsystem), danach bleibt die
    // Datei bestehen und wird nie erneut heruntergeladen. Gleiches
    // Download-/Tmp-Datei-Umbenennungs-Muster wie i18n.cpp::
    // downloadLanguageToSd() (inkl. delay(1)-Watchdog-Fix und exakter
    // Content-Length-Pruefung, siehe dortige Bugfix-Kommentare fuer die
    // volle Herleitung).
    constexpr uint8_t AIRPORTS_MAGIC[4] = {'A', 'P', 'R', '2'};

    bool ensureDir(const char* path) {
        if (SD.exists(path)) return true;
        return SD.mkdir(path);
    }

    void writeIfAbsent(const char* path, const char* contents) {
        if (SD.exists(path)) return;
        File f = SD.open(path, FILE_WRITE);
        if (!f) return;
        f.print(contents);
        f.close();
    }

    // Prueft, ob die vorhandene Flughafen-Datei bereits mit dem aktuellen
    // AIRPORTS_MAGIC beginnt. Liefert false sowohl bei fehlender Datei als
    // auch bei einer Datei im alten Text-CSV-Format oder einer verkuerzten/
    // beschaedigten Kopfzeile - all das soll ueberschrieben werden.
    bool airportsFileUpToDate() {
        if (!SD.exists(Config::SD_AIRPORTS_CSV)) return false;
        File f = SD.open(Config::SD_AIRPORTS_CSV, FILE_READ);
        if (!f) return false;
        uint8_t header[4] = {0};
        size_t n = f.read(header, sizeof(header));
        f.close();
        return n == sizeof(header) && memcmp(header, AIRPORTS_MAGIC, sizeof(AIRPORTS_MAGIC)) == 0;
    }

    // Laedt die aktuelle Flughafendatenbank von GitHub herunter und
    // ersetzt die SD-Datei nur bei VOLLSTAENDIGEM Erfolg (Tmp-Datei-dann-
    // Umbenennen-Muster, identisch zu i18n.cpp::downloadLanguageToSd() -
    // siehe dortige Bugfix-Kommentare fuer delay(1)-Watchdog-Fix und
    // exakte Content-Length-Pruefung, hier 1:1 uebernommen).
    bool downloadAirportsToSd() {
        if (WiFi.status() != WL_CONNECTED) return false;

        try {
            constexpr const char* URL =
                "https://raw.githubusercontent.com/Eiswolf-BG/eiswolfs-flightradar-CYD/main/assets/airports_data.bin";
            Serial.printf("[SD] Lade Flughafendatenbank herunter: %s\n", URL);

            WiFiClientSecure client;
            client.setInsecure();
            HTTPClient http;
            if (!http.begin(client, URL)) return false;

            constexpr uint32_t TIMEOUT_MS = 20000;
            http.setTimeout(TIMEOUT_MS);
            http.setUserAgent("EiswolfsFlightradarCYD-Airports/1.0 (+https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD)");

            int code = http.GET();
            if (code != HTTP_CODE_OK) {
                http.end();
                return false;
            }

            constexpr const char* TMP_PATH = "/Flightradar_cyd/airports.tmp";
            File out;
            {
                SdMutex::Guard guard;
                if (SD.exists(TMP_PATH)) SD.remove(TMP_PATH);
                out = SD.open(TMP_PATH, FILE_WRITE);
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

            bool complete = (contentLen >= 0) ? (written == contentLen) : (written >= 1000);
            if (timedOut || !complete) {
                SdMutex::Guard guard;
                SD.remove(TMP_PATH);
                return false;
            }

            SdMutex::Guard guard;
            if (SD.exists(Config::SD_AIRPORTS_CSV)) SD.remove(Config::SD_AIRPORTS_CSV);
            bool renamed = SD.rename(TMP_PATH, Config::SD_AIRPORTS_CSV);
            Serial.printf("[SD] Flughafendatenbank-Download %s (%d Bytes)\n",
                          renamed ? "erfolgreich" : "fehlgeschlagen (Umbenennen)", written);
            return renamed;
        } catch (...) {
            Serial.printf("[SD] Flughafendatenbank: Exception abgefangen, freeHeap=%u maxAlloc=%u\n",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
            return false;
        }
    }

    uint32_t lastAirportsRetryMs = 0;
    // Gleiche Groessenordnung wie LANG_RETRY_INTERVAL_MS in i18n.cpp -
    // verhindert, dass ein fehlgeschlagener Download (WLAN gerade erst
    // verbunden, GitHub kurzzeitig nicht erreichbar) bei jeder
    // NetTask-Schleifeniteration sofort erneut versucht wird. Wird nach
    // dem EINEN erfolgreichen Download im Geraeteleben nie wieder erreicht
    // (airportsFileUpToDate() liefert dann sofort true, kein Netzwerk-
    // zugriff mehr noetig).
    constexpr uint32_t AIRPORTS_RETRY_INTERVAL_MS = 30000;
}

bool init() {
    SdMutex::init();
    SdMutex::Guard guard;

    sdSpi.begin(Config::SD_SPI_CLK_PIN, Config::SD_SPI_MISO_PIN,
                Config::SD_SPI_MOSI_PIN, Config::SD_SPI_CS_PIN);
    mounted = SD.begin(Config::SD_SPI_CS_PIN, sdSpi, 4000000);
    return mounted;
}

bool isMounted() { return mounted; }

void createStructure() {
    if (!mounted) return;
    SdMutex::Guard guard;
    ensureDir(Config::SD_ROOT_DIR);
    ensureDir(Config::SD_LOG_DIR);
    ensureDir(Config::SD_SCREENSHOT_DIR);
}

void seedDefaultDataFiles() {
    if (!mounted) return;
    SdMutex::Guard guard;
    writeIfAbsent(Config::SD_AIRLINES_CSV, kDefaultAirlinesCsv);
    writeIfAbsent(Config::SD_AIRCRAFT_TYPES_CSV, kDefaultAircraftTypesCsv);
    // Flughafendatenbank NICHT mehr hier synchron aus dem Flash geseedet
    // (kein WLAN zu diesem fruehen Boot-Zeitpunkt verfuegbar, siehe
    // downloadAirportsToSd()-Kommentar oben) - retryAirportsDownloadIfNeeded()
    // unten uebernimmt das jetzt asynchron, sobald WLAN verbunden ist.
}

// Von NetTask (net_task.cpp) bei JEDER Schleifeniteration aufgerufen,
// intern selbst gedrosselt (AIRPORTS_RETRY_INTERVAL_MS) - gleiches Prinzip
// wie I18n::retryActiveLanguageDownloadIfNeeded(). Kehrt sofort zurueck,
// sobald airportsFileUpToDate() einmal true liefert (der Normalfall nach
// dem allerersten erfolgreichen Download im Geraeteleben) - kein
// wiederkehrender Netzwerkzugriff danach.
void retryAirportsDownloadIfNeeded() {
    if (!mounted) return;
    bool upToDate;
    {
        SdMutex::Guard guard;
        upToDate = airportsFileUpToDate();
    }
    if (upToDate) return;
    if (WiFi.status() != WL_CONNECTED) return;
    uint32_t now = millis();
    if (lastAirportsRetryMs != 0 && now - lastAirportsRetryMs < AIRPORTS_RETRY_INTERVAL_MS) return;
    lastAirportsRetryMs = now;
    downloadAirportsToSd();
}

void logEvent(const char* csvLine) {
    if (!mounted) return;
    SdMutex::Guard guard;

    time_t now = time(nullptr);
    struct tm tmNow;
    localtime_r(&now, &tmNow);
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/%04d-%02d-%02d.csv",
             Config::SD_LOG_DIR, tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);

    File f = SD.open(filename, FILE_APPEND);
    if (!f) return;
    f.println(csvLine);
    f.close();
}

bool deleteDirectoryRecursive(const char* path) {
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return false;
    }

    // Gleiches vorsichtiges "entry.name() koennte relativ ODER absolut
    // sein"-Muster wie in flight_logbook.cpp::resetAllData() - anders als
    // dort aber mit echter Rekursion in Unterordner (statt sie zu
    // ueberspringen), da der Flightradar-Ordner welche enthaelt (logs/,
    // screenshots/).
    File entry = dir.openNextFile();
    while (entry) {
        bool isDir = entry.isDirectory();
        String name = String(entry.name());
        entry.close();

        String fullPath = name.startsWith("/") ? name : String(path) + "/" + name;
        if (isDir) {
            deleteDirectoryRecursive(fullPath.c_str());
        } else {
            SD.remove(fullPath);
        }
        entry = dir.openNextFile();
    }
    dir.close();

    return SD.rmdir(path);
}

}