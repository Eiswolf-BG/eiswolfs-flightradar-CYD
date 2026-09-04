#include "sd_storage.h"
#include "config.h"
#include "sd_mutex.h"
#include "airports_data.h"
#include <SD.h>
#include <SPI.h>
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
    // airports_data.h/.cpp fuer die eingebetteten Rohdaten und
    // airport_lookup.cpp fuer den Parser): 4 Byte Magic "APR2" + 2 Byte
    // Datensatz-Anzahl (uint16 LE), danach je 12 Byte pro Flughafen (4 Byte
    // ICAO-ASCII + int32 LE Breitengrad + int32 LE Laengengrad, beide in
    // Mikrograd). Frueher lag hier eine reine Text-CSV
    // ("icao,name,lat,lon\n...") mit nur 34 handkuratierten Hubs - eine
    // Datei im alten Format beginnt nie mit diesen 4 Magic-Bytes, weshalb
    // seedAirportsFile() ein Fehlen/Nicht-Uebereinstimmen dieser Kennung
    // zuverlässig als "muss durch die neue, weltweite Datenbank ersetzt
    // werden" erkennt - auch auf bereits eingerichteten Geraeten, ohne
    // Werksreset.
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

    void seedAirportsFile() {
        if (airportsFileUpToDate()) return;
        // FILE_WRITE oeffnet vorhandene Dateien im Anhaenge-Modus - eine
        // veraltete (kuerzere) alte CSV-Datei muss daher vorher entfernt
        // werden, sonst blieben ihre Reste hinter den neuen Binaerdaten
        // haengen.
        if (SD.exists(Config::SD_AIRPORTS_CSV)) SD.remove(Config::SD_AIRPORTS_CSV);
        File f = SD.open(Config::SD_AIRPORTS_CSV, FILE_WRITE);
        if (!f) return;
        f.write(kAirportsBin, kAirportsBinLen);
        f.close();
    }
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
    seedAirportsFile();
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