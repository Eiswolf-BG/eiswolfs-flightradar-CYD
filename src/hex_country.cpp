#include "hex_country.h"
#include <cstdlib>
#include <cstring>

namespace HexCountry {

namespace {
    struct Range {
        uint32_t start;  // inklusiv
        uint32_t end;    // inklusiv
        const char* name;
    };

    // ICAO-24-Bit-Adressbloecke pro Staat (ICAO Annex 10, Vol. III) -
    // bewusst NUR eine kompakte Auswahl gaengiger Laender/Regionen statt
    // vollstaendiger Weltabdeckung (Alex' Vorgabe angesichts der Flash-
    // Groesse), Schwerpunkt Europa (Zielgruppe des Projekts) plus grosse
    // Luftfahrtnationen weltweit. Kleinere/seltener gesichtete Staaten
    // (v.a. Afrika, Pazifik, Zentralasien) bewusst ausgelassen - dort zeigt
    // das Detail-Panel dann einfach kein Land an, statt eine falsche
    // Angabe zu raten (siehe lookup()). Aufsteigend nach "start" sortiert.
    constexpr Range RANGES[] = {
        {0x008000, 0x00FFFF, "South Africa"},
        {0x010000, 0x017FFF, "Egypt"},
        {0x020000, 0x027FFF, "Morocco"},
        {0x06A000, 0x06A3FF, "Qatar"},
        {0x0AC000, 0x0ACFFF, "Colombia"},
        {0x0D0000, 0x0D7FFF, "Mexico"},
        {0x0D8000, 0x0DFFFF, "Venezuela"},
        {0x100000, 0x1FFFFF, "Russia"},
        {0x300000, 0x33FFFF, "Italy"},
        {0x340000, 0x37FFFF, "Spain"},
        {0x380000, 0x3BFFFF, "France"},
        {0x3C0000, 0x3FFFFF, "Germany"},
        {0x400000, 0x43FFFF, "United Kingdom"},
        {0x440000, 0x447FFF, "Austria"},
        {0x448000, 0x44FFFF, "Belgium"},
        {0x450000, 0x457FFF, "Bulgaria"},
        {0x458000, 0x45FFFF, "Denmark"},
        {0x460000, 0x467FFF, "Finland"},
        {0x468000, 0x46FFFF, "Greece"},
        {0x470000, 0x477FFF, "Hungary"},
        {0x478000, 0x47FFFF, "Norway"},
        {0x480000, 0x487FFF, "Netherlands"},
        {0x488000, 0x48FFFF, "Poland"},
        {0x490000, 0x497FFF, "Portugal"},
        {0x498000, 0x49FFFF, "Czech Republic"},
        {0x4A0000, 0x4A7FFF, "Romania"},
        {0x4A8000, 0x4AFFFF, "Sweden"},
        {0x4B0000, 0x4B7FFF, "Switzerland"},
        {0x4B8000, 0x4BFFFF, "Turkey"},
        {0x4C0000, 0x4C7FFF, "Serbia"},
        {0x4C8000, 0x4C83FF, "Cyprus"},
        {0x4CA000, 0x4CAFFF, "Ireland"},
        {0x4CC000, 0x4CCFFF, "Iceland"},
        {0x4D0000, 0x4D03FF, "Luxembourg"},
        {0x4D2000, 0x4D23FF, "Malta"},
        {0x4D4000, 0x4D43FF, "Monaco"},
        {0x500000, 0x5004FF, "San Marino"},
        {0x501000, 0x5013FF, "Albania"},
        {0x501C00, 0x501FFF, "Croatia"},
        {0x502C00, 0x502FFF, "Latvia"},
        {0x503C00, 0x503FFF, "Lithuania"},
        {0x504C00, 0x504FFF, "Moldova"},
        {0x505C00, 0x505FFF, "Slovakia"},
        {0x506C00, 0x506FFF, "Slovenia"},
        {0x508000, 0x50FFFF, "Ukraine"},
        {0x510000, 0x5103FF, "Belarus"},
        {0x511000, 0x5113FF, "Estonia"},
        {0x512000, 0x5123FF, "North Macedonia"},
        {0x513000, 0x5133FF, "Bosnia and Herzegovina"},
        {0x514000, 0x5143FF, "Georgia"},
        {0x710000, 0x717FFF, "Saudi Arabia"},
        {0x718000, 0x71FFFF, "South Korea"},
        {0x728000, 0x72FFFF, "Iraq"},
        {0x730000, 0x737FFF, "Iran"},
        {0x738000, 0x73FFFF, "Israel"},
        {0x740000, 0x747FFF, "Jordan"},
        {0x748000, 0x74FFFF, "Lebanon"},
        {0x750000, 0x757FFF, "Malaysia"},
        {0x758000, 0x75FFFF, "Philippines"},
        {0x760000, 0x767FFF, "Pakistan"},
        {0x768000, 0x76FFFF, "Singapore"},
        {0x770000, 0x777FFF, "Sri Lanka"},
        {0x778000, 0x77FFFF, "Syria"},
        {0x780000, 0x7BFFFF, "China"},
        {0x7C0000, 0x7FFFFF, "Australia"},
        {0x800000, 0x83FFFF, "India"},
        {0x840000, 0x87FFFF, "Japan"},
        {0x880000, 0x887FFF, "Thailand"},
        {0x888000, 0x88FFFF, "Vietnam"},
        {0x896000, 0x896FFF, "United Arab Emirates"},
        {0x8A0000, 0x8A7FFF, "Indonesia"},
        {0xA00000, 0xAFFFFF, "United States"},
        {0xC00000, 0xC3FFFF, "Canada"},
        {0xC80000, 0xC87FFF, "New Zealand"},
        {0xE00000, 0xE3FFFF, "Argentina"},
        {0xE40000, 0xE7FFFF, "Brazil"},
        {0xE80000, 0xE80FFF, "Chile"},
        {0xE8C000, 0xE8CFFF, "Peru"},
    };
    constexpr size_t RANGE_COUNT = sizeof(RANGES) / sizeof(RANGES[0]);
}

const char* lookup(const char* hex) {
    if (!hex || !hex[0]) return nullptr;
    // strtoul() statt sscanf(): toleriert auch einen kuerzeren String
    // (sollte nicht vorkommen, Aircraft::hex ist immer 6-stellig) und
    // ignoriert Gross-/Kleinschreibung von selbst.
    uint32_t value = (uint32_t)strtoul(hex, nullptr, 16);
    if (value == 0) return nullptr;

    // Einfache lineare Bereichspruefung statt Binaersuche (Alex' Vorgabe
    // erlaubt beides) - bei ~80 Eintraegen und nur einem Aufruf pro
    // Detail-Panel-Neuzeichnung voellig unkritisch performant, bewusst
    // nicht ueberoptimiert (gleiches Prinzip wie findFarthest() in
    // adsb_client.cpp).
    for (size_t i = 0; i < RANGE_COUNT; i++) {
        if (value >= RANGES[i].start && value <= RANGES[i].end) {
            return RANGES[i].name;
        }
    }
    return nullptr;
}

}
