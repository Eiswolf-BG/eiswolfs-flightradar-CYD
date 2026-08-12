#include "sd_storage.h"
#include "config.h"
#include "sd_mutex.h"
#include <SD.h>
#include <SPI.h>
#include <time.h>

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

    const char* kDefaultAirportsCsv =
        "icao,name,lat,lon\n"
        "EDDF,Frankfurt,50.0379,8.5622\n"
        "EDDM,Muenchen,48.3538,11.7861\n"
        "EDDB,Berlin Brandenburg,52.3667,13.5033\n"
        "EDDL,Duesseldorf,51.2895,6.7668\n"
        "EDDH,Hamburg,53.6304,9.9882\n"
        "EDDS,Stuttgart,48.6900,9.2219\n"
        "LFPG,Paris CDG,49.0097,2.5479\n"
        "EGLL,London Heathrow,51.4700,-0.4543\n"
        "LEMD,Madrid Barajas,40.4936,-3.5668\n"
        "LIRF,Roma Fiumicino,41.8003,12.2389\n"
        "EHAM,Amsterdam Schiphol,52.3086,4.7639\n"
        "LSZH,Zuerich,47.4647,8.5492\n"
        "LOWW,Wien,48.1103,16.5697\n"
        "EKCH,Kopenhagen,55.6180,12.6560\n"
        "ESSA,Stockholm Arlanda,59.6519,17.9186\n"
        "LTFM,Istanbul,41.2753,28.7519\n"
        "OMDB,Dubai,25.2532,55.3657\n"
        "OTHH,Doha Hamad,25.2609,51.6138\n"
        "KJFK,New York JFK,40.6413,-73.7781\n"
        "KLAX,Los Angeles,33.9416,-118.4085\n"
        "KORD,Chicago O'Hare,41.9742,-87.9073\n"
        "CYYZ,Toronto Pearson,43.6777,-79.6248\n"
        "RJTT,Tokyo Haneda,35.5494,139.7798\n"
        "RJAA,Tokyo Narita,35.7647,140.3864\n"
        "ZBAA,Beijing Capital,40.0801,116.5846\n"
        "VHHH,Hong Kong,22.3080,113.9185\n"
        "WSSS,Singapore Changi,1.3644,103.9915\n"
        "YSSY,Sydney,-33.9399,151.1753\n"
        "FAOR,Johannesburg OR Tambo,-26.1392,28.2460\n"
        "SBGR,Sao Paulo Guarulhos,-23.4356,-46.4731\n"
        "OMAA,Abu Dhabi,24.4330,54.6511\n"
        "LGAV,Athen,37.9364,23.9445\n"
        "LPPT,Lissabon,38.7756,-9.1354\n"
        "EPWA,Warschau Chopin,52.1657,20.9671\n";

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
}

bool init() {
    SdMutex::init();
    SdMutex::Guard guard;

    sdSpi.begin(Config::SD_SPI_CLK_PIN, Config::SD_SPI_MISO_PIN,
                Config::SD_SPI_MOSI_PIN, Config::SD_SPI_CS_PIN);
    mounted = SD.begin(Config::SD_SPI_CS_PIN, sdSpi, 4000000);
    if (!mounted) return false;

    ensureDir(Config::SD_ROOT_DIR);
    ensureDir(Config::SD_LOG_DIR);
    ensureDir(Config::SD_SCREENSHOT_DIR);
    return true;
}

bool isMounted() { return mounted; }

void seedDefaultDataFiles() {
    if (!mounted) return;
    SdMutex::Guard guard;
    writeIfAbsent(Config::SD_AIRLINES_CSV, kDefaultAirlinesCsv);
    writeIfAbsent(Config::SD_AIRCRAFT_TYPES_CSV, kDefaultAircraftTypesCsv);
    writeIfAbsent(Config::SD_AIRPORTS_CSV, kDefaultAirportsCsv);
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