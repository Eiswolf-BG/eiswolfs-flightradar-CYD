#include "dev_file_manager_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "settings_store.h"
#include "sd_mutex.h"
#include "config.h"
#include "ui_theme.h"
#include <SD.h>
#include <cstring>

// Reines Entwickler-Werkzeug, siehe Header-Kommentar - bewusst schlicht und
// eigenstaendig gehalten (kein i18n, keine besonders huebsche Optik), da nur
// Alex selbst ihn je zu sehen bekommt.
namespace DevFileManagerScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, uint16_t accent = TFT_LIGHTGREY) {
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Einfacher Vollbild-Ja/Nein-Dialog fuer die Loeschbestaetigung - kein
    // gemeinsamer Helfer im Projekt oeffentlich verfuegbar
    // (confirmWarningScreen() in menu_screen.cpp ist anonymous-namespace-
    // intern), daher hier bewusst minimal dupliziert statt exportiert.
    bool confirmDelete(TFT_eSPI& tft, const String& name) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(2);
        tft.drawString("Delete?", Config::SCREEN_WIDTH / 2, 60);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        String shown = name;
        if (shown.length() > 34) shown = shown.substring(0, 31) + "...";
        tft.drawString(shown, Config::SCREEN_WIDTH / 2, 100);
        tft.setTextDatum(TL_DATUM);

        Rect yesBtn = {10, 160, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
        Rect noBtn  = {10, 214, (int16_t)(Config::SCREEN_WIDTH - 20), 44};
        drawButton(tft, yesBtn, "Yes, delete", TFT_RED);
        drawButton(tft, noBtn, "Cancel", TFT_LIGHTGREY);

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (yesBtn.contains(tap.x, tap.y)) return true;
                if (noBtn.contains(tap.x, tap.y)) return false;
            }
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) return false;
            MenuStars::update(tft);
            delay(20);
        }
    }

    struct Entry {
        char name[64];
        bool isDir;
        uint32_t size;
    };

    // BUGFIX (Linker-Fehler "DRAM segment does not fit" nach Hinzufuegen
    // des Flight-Stories-Features an anderer Stelle) - von 64 auf 24
    // reduziert, spart ~2,9KB im knappen .bss-Speicherbereich. 24 reicht
    // dank der bereits vorhandenen Paginierung (Prev/Next) weiterhin
    // bequem aus, dieser Screen ist ohnehin ein reines Entwicklerwerkzeug.
    constexpr uint8_t MAX_ENTRIES = 24;

    // Liest den Inhalt von `path` komplett in `entries` ein (bis maximal
    // MAX_ENTRIES) - synchron/blockierend, kann bei vielen Dateien (z.B.
    // Logbuch-Exporte) spuerbar dauern, deshalb der "lauscht nicht sofort"-
    // Effekt direkt nach den 6 Tipps auf den QR-Code (Alex' eigene
    // Beobachtung, bewusst so belassen statt eine Ladeanzeige zu bauen -
    // reines Entwicklerwerkzeug).
    uint8_t listDir(const char* path, Entry* entries) {
        uint8_t count = 0;
        SdMutex::Guard guard;
        File dir = SD.open(path);
        if (!dir || !dir.isDirectory()) {
            if (dir) dir.close();
            return 0;
        }
        File f = dir.openNextFile();
        while (f && count < MAX_ENTRIES) {
            const char* nm = f.name();
            // SD.open()/File::name() liefert je nach Kern mal den vollen
            // Pfad, mal nur den Basisnamen - hier robust auf den Teil NACH
            // dem letzten '/' reduzieren, damit die Liste in jedem Fall nur
            // den Dateinamen zeigt, nicht den ganzen Pfad.
            const char* base = strrchr(nm, '/');
            base = base ? base + 1 : nm;
            strncpy(entries[count].name, base, sizeof(entries[count].name) - 1);
            entries[count].name[sizeof(entries[count].name) - 1] = 0;
            entries[count].isDir = f.isDirectory();
            entries[count].size = entries[count].isDir ? 0 : (uint32_t)f.size();
            count++;
            f.close();
            f = dir.openNextFile();
        }
        if (f) f.close();
        dir.close();
        return count;
    }

    String formatSize(uint32_t bytes) {
        if (bytes < 1024) return String(bytes) + " B";
        if (bytes < 1024UL * 1024UL) return String(bytes / 1024) + " KB";
        return String(bytes / (1024UL * 1024UL)) + " MB";
    }
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();
    // BUGFIX (Alex' Meldung: zeigte "/" statt unseres eigenen Ordners, dabei
    // fremde ~256MB-Dateien einer frueheren SD-Kartennutzung sichtbar) -
    // startet jetzt direkt in Config::SD_ROOT_DIR ("/Flightradar_cyd")
    // statt am Karten-Wurzelverzeichnis. ROOT_LEN merkt sich dessen Laenge,
    // damit "Up" (siehe unten) niemals darueber hinaus in andere Bereiche
    // der Karte navigieren kann.
    char path[160];
    strncpy(path, Config::SD_ROOT_DIR, sizeof(path) - 1);
    path[sizeof(path) - 1] = 0;
    const size_t ROOT_LEN = strlen(Config::SD_ROOT_DIR);
    uint8_t scrollTop = 0;

    static Entry entries[MAX_ENTRIES];
    uint8_t count = 0;
    bool needReload = true;

    constexpr int16_t ROW_H = 26;
    constexpr int16_t LIST_TOP = 34;
    constexpr int16_t FOOTER_H = 40;
    constexpr int16_t VISIBLE_ROWS = (Config::SCREEN_HEIGHT - LIST_TOP - FOOTER_H - 6) / ROW_H;
    constexpr int16_t DEL_BTN_W = 26;

    Rect upBtn   = {6, 6, 60, 22};
    Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - FOOTER_H + 4), 100, 30};
    Rect prevBtn = {(int16_t)(Config::SCREEN_WIDTH - 210), (int16_t)(Config::SCREEN_HEIGHT - FOOTER_H + 4), 95, 30};
    Rect nextBtn = {(int16_t)(Config::SCREEN_WIDTH - 108), (int16_t)(Config::SCREEN_HEIGHT - FOOTER_H + 4), 95, 30};

    Rect rowRects[MAX_ENTRIES];
    Rect delRects[MAX_ENTRIES];

    bool done = false;
    while (!done) {
        if (needReload) {
            count = listDir(path, entries);
            scrollTop = 0;
            needReload = false;
        }

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        String shownPath(path);
        if (shownPath.length() > 26) shownPath = "..." + shownPath.substring(shownPath.length() - 23);
        tft.setCursor(72, 22);
        tft.print(shownPath);
        bool atRoot = strlen(path) <= ROOT_LEN;
        if (!atRoot) drawButton(tft, upBtn, "Up");

        uint8_t visibleEnd = (uint8_t)min((int)count, (int)(scrollTop + VISIBLE_ROWS));
        for (uint8_t i = scrollTop; i < visibleEnd; i++) {
            int16_t rowY = (int16_t)(LIST_TOP + (i - scrollTop) * ROW_H);
            rowRects[i] = {0, rowY, (int16_t)(Config::SCREEN_WIDTH - DEL_BTN_W - 4), ROW_H - 2};
            delRects[i] = {(int16_t)(Config::SCREEN_WIDTH - DEL_BTN_W), rowY, DEL_BTN_W, ROW_H - 2};

            uint16_t nameColor = entries[i].isDir ? UiTheme::accentColor(tft) : TFT_WHITE;
            tft.setTextColor(nameColor, TFT_BLACK);
            String label = entries[i].name;
            if (entries[i].isDir) label += "/";
            while (tft.textWidth(label) > rowRects[i].w - 70 && label.length() > 4) {
                label = label.substring(0, label.length() - 4) + "...";
            }
            tft.setCursor(4, (int16_t)(rowY + ROW_H - 8));
            tft.print(label);

            if (!entries[i].isDir) {
                tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
                tft.setTextDatum(TR_DATUM);
                tft.drawString(formatSize(entries[i].size), (int16_t)(rowRects[i].x + rowRects[i].w - 2),
                                (int16_t)(rowY + ROW_H - 8));
                tft.setTextDatum(TL_DATUM);
            }

            tft.drawRoundRect(delRects[i].x, delRects[i].y, delRects[i].w, delRects[i].h, 3, TFT_RED);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("X", delRects[i].x + delRects[i].w / 2, delRects[i].y + delRects[i].h / 2);
            tft.setTextDatum(TL_DATUM);
        }
        if (count == 0) {
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setCursor(4, LIST_TOP + 16);
            tft.print("(empty)");
        }

        bool hasPrev = scrollTop > 0;
        bool hasNext = visibleEnd < count;
        if (hasPrev) drawButton(tft, prevBtn, "< Prev");
        if (hasNext) drawButton(tft, nextBtn, "Next >");
        drawButton(tft, backBtn, "Exit");

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }
        if (done) break;

        if (backBtn.contains(tap.x, tap.y)) {
            done = true;
            continue;
        }
        if (upBtn.contains(tap.x, tap.y)) {
            // Niemals ueber Config::SD_ROOT_DIR hinaus - reine
            // Sicherheitsgrenze, damit dieses Entwicklerwerkzeug nicht
            // versehentlich in fremde Bereiche der SD-Karte navigiert
            // (Alex' Wunsch).
            String p(path);
            if (p.length() > ROOT_LEN) {
                int lastSlash = p.lastIndexOf('/');
                if (lastSlash < (int)ROOT_LEN) p = Config::SD_ROOT_DIR;
                else p = p.substring(0, lastSlash);
                strncpy(path, p.c_str(), sizeof(path) - 1);
                path[sizeof(path) - 1] = 0;
                needReload = true;
            }
            continue;
        }
        if (hasPrev && prevBtn.contains(tap.x, tap.y)) {
            scrollTop = (uint8_t)max(0, scrollTop - VISIBLE_ROWS);
            continue;
        }
        if (hasNext && nextBtn.contains(tap.x, tap.y)) {
            scrollTop = visibleEnd;
            continue;
        }

        bool handledRow = false;
        for (uint8_t i = scrollTop; i < visibleEnd && !handledRow; i++) {
            if (delRects[i].contains(tap.x, tap.y)) {
                handledRow = true;
                String full(path);
                if (!full.endsWith("/")) full += "/";
                full += entries[i].name;
                if (confirmDelete(tft, entries[i].name)) {
                    SdMutex::Guard guard;
                    if (entries[i].isDir) {
                        SD.rmdir(full.c_str());
                    } else {
                        SD.remove(full.c_str());
                    }
                }
                needReload = true;
            } else if (rowRects[i].contains(tap.x, tap.y)) {
                handledRow = true;
                if (entries[i].isDir) {
                    String full(path);
                    if (!full.endsWith("/")) full += "/";
                    full += entries[i].name;
                    strncpy(path, full.c_str(), sizeof(path) - 1);
                    path[sizeof(path) - 1] = 0;
                    needReload = true;
                }
            }
        }
    }
}

}
