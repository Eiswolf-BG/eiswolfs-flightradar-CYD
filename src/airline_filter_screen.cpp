#include "airline_filter_screen.h"
#include "airline_filter.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "menu_screen.h"
#include "config.h"
#include "settings_store.h"
#include "i18n.h"
#include "ui_theme.h"

namespace AirlineFilterScreen {

namespace {
    // Kleine ICAO<->IATA-Tabelle (haeufige internationale/europaeische
    // Carrier, bewusst nicht vollstaendig) - dieselbe Datenquelle wie die
    // JS-Tabelle fuers Airline-Logo-Feature der Webseite
    // (web_export_server.cpp::"ICAO_TO_IATA"), hier fuer den Geraete-
    // Eingabe-Screen dupliziert statt geteilt (CLAUDE.md "jeder Screen
    // unabhaengig lauffaehig") und in Eingaberichtung gedreht (IATA->ICAO,
    // da der Filter intern immer mit ICAO arbeitet - das Callsign in den
    // ADS-B-Rohdaten beginnt immer mit dem 3-stelligen ICAO-Praefix).
    // Alex' Wunsch: statt sich nach der Geraete-IATA/ICAO-Einstellung zu
    // richten (die eigentlich fuer Flugnummern/Flughafencodes gedacht ist,
    // nicht fuer diese Eingabe), werden hier IMMER beide Formate anhand
    // der Laenge automatisch erkannt (IATA=2, ICAO=3 Zeichen) - robuster
    // und unabhaengig von einer Einstellung, die inhaltlich nichts mit
    // diesem Screen zu tun hat.
    struct IataIcaoPair { const char* icao; const char* iata; };
    constexpr IataIcaoPair IATA_ICAO_TABLE[] = {
        {"DLH","LH"}, {"BAW","BA"}, {"AFR","AF"}, {"KLM","KL"}, {"SWR","LX"}, {"AUA","OS"},
        {"IBE","IB"}, {"TAP","TP"}, {"SAS","SK"}, {"FIN","AY"}, {"THY","TK"}, {"AEE","A3"},
        {"RYR","FR"}, {"EZY","U2"}, {"WZZ","W6"}, {"VLG","VY"}, {"EWG","EW"}, {"NAX","DY"},
        {"IBS","I2"}, {"TRA","HV"}, {"PGT","PC"}, {"BEL","SN"}, {"CFG","DE"}, {"EXS","LS"},
        {"TOM","BY"}, {"LGL","LG"}, {"BTI","BT"}, {"LOT","LO"}, {"CSA","OK"}, {"ROT","RO"},
        {"AFL","SU"}, {"UAE","EK"}, {"QTR","QR"}, {"ETD","EY"}, {"SVA","SV"}, {"MSR","MS"},
        {"RJA","RJ"}, {"ELY","LY"}, {"GFA","GF"}, {"KAC","KU"}, {"OMA","WY"}, {"MEA","ME"},
        {"RAM","AT"}, {"TUN","TU"}, {"ETH","ET"}, {"SAA","SA"}, {"KQA","KQ"}, {"DAH","AH"},
        {"ICE","FI"}, {"UAL","UA"}, {"AAL","AA"}, {"DAL","DL"}, {"SWA","WN"}, {"JBU","B6"},
        {"ASA","AS"}, {"FFT","F9"}, {"NKS","NK"}, {"ACA","AC"}, {"WJA","WS"}, {"CPA","CX"},
        {"SIA","SQ"}, {"ANA","NH"}, {"JAL","JL"}, {"KAL","KE"}, {"AAR","OZ"}, {"CCA","CA"},
        {"CES","MU"}, {"CSN","CZ"}, {"THA","TG"}, {"MAS","MH"}, {"GIA","GA"}, {"PAL","PR"},
        {"CAL","CI"}, {"EVA","BR"}, {"AIC","AI"}, {"IGO","6E"}, {"QFA","QF"}, {"ANZ","NZ"},
        {"VOZ","VA"}, {"PIA","PK"}, {"LAN","LA"}, {"TAM","JJ"}, {"ARG","AR"}, {"AVA","AV"},
        {"CMP","CM"}, {"AMX","AM"}, {"GLO","G3"}, {"AZU","AD"}, {"FDX","FX"}, {"UPS","5X"},
        {"GTI","5Y"}, {"CLX","CV"}, {"ITY","AZ"}, {"EIN","EI"}, {"CRL","SS"}, {"TSC","TS"},
    };
    constexpr uint8_t IATA_ICAO_TABLE_COUNT = sizeof(IATA_ICAO_TABLE) / sizeof(IATA_ICAO_TABLE[0]);

    // Wandelt eine Nutzereingabe in den fuers Filtern noetigen ICAO-Code
    // um - 3 Zeichen gelten als bereits-ICAO (unveraendert durchgereicht,
    // deckt auch unbekannte/nicht in der Tabelle gelistete Airlines ab),
    // 2 Zeichen werden als IATA interpretiert und per Tabelle aufgeloest.
    // Leerer String = kein Treffer (unbekannter IATA-Code), Aufrufer soll
    // dann nichts hinzufuegen statt einen falschen/nutzlosen Eintrag zu
    // speichern.
    String resolveToIcao(const String& input) {
        if (input.length() == 2) {
            for (uint8_t i = 0; i < IATA_ICAO_TABLE_COUNT; i++) {
                if (input.equalsIgnoreCase(IATA_ICAO_TABLE[i].iata)) {
                    return String(IATA_ICAO_TABLE[i].icao);
                }
            }
            return String();
        }
        return input;
    }

    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool danger = false) {
        uint16_t accent = danger ? TFT_RED : UiTheme::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    String runLetterKeypad(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr const char* ROW1 = "QWERTYUIOP";
        constexpr const char* ROW2 = "ASDFGHJKL";
        constexpr const char* ROW3 = "ZXCVBNM";

        char buf[4] = {0};
        uint8_t len = 0;

        constexpr int16_t KEY_H = 32;
        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t ROW0_Y = 90;

        auto layoutRow = [&](const char* row, int16_t y, Rect* outRects, uint8_t n) {
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect row1Rects[10], row2Rects[9], row3Rects[7];
        layoutRow(ROW1, ROW0_Y, row1Rects, 10);
        layoutRow(ROW2, ROW0_Y + KEY_H + KEY_GAP, row2Rects, 9);
        layoutRow(ROW3, ROW0_Y + 2 * (KEY_H + KEY_GAP), row3Rects, 7);

        Rect backspaceBtn = {4, (int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), 100, KEY_H};
        Rect cancelBtn     = {(int16_t)(Config::SCREEN_WIDTH - 100), (int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), 96, KEY_H};
        Rect confirmBtn    = {4, (int16_t)(ROW0_Y + 4 * (KEY_H + KEY_GAP)), (int16_t)(Config::SCREEN_WIDTH - 8), KEY_H};

        bool done = false;
        bool confirmed = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::AIRLINE_ADD_TITLE));

            tft.fillRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_BLACK);
            tft.drawRect(8, 40, Config::SCREEN_WIDTH - 16, 34, UiTheme::accentColor(tft));
            tft.setTextSize(2);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(14, 66);
            tft.print(buf);
            tft.setTextSize(1);

            for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(ROW1[i]));
            for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(ROW2[i]));
            for (uint8_t i = 0; i < 7; i++) drawButton(tft, row3Rects[i], String(ROW3[i]));

            drawButton(tft, backspaceBtn, "<-");
            drawButton(tft, cancelBtn, I18n::t(StringId::CANCEL), true);
            drawButton(tft, confirmBtn, I18n::t(StringId::ADD));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (row1Rects[i].contains(tap.x, tap.y) && len < 3) { buf[len++] = ROW1[i]; handled = true; }
            }
            for (uint8_t i = 0; i < 9 && !handled; i++) {
                if (row2Rects[i].contains(tap.x, tap.y) && len < 3) { buf[len++] = ROW2[i]; handled = true; }
            }
            for (uint8_t i = 0; i < 7 && !handled; i++) {
                if (row3Rects[i].contains(tap.x, tap.y) && len < 3) { buf[len++] = ROW3[i]; handled = true; }
            }
            if (!handled && backspaceBtn.contains(tap.x, tap.y)) {
                if (len > 0) { len--; buf[len] = 0; }
                handled = true;
            }
            if (!handled && cancelBtn.contains(tap.x, tap.y)) {
                done = true;
                confirmed = false;
                handled = true;
            }
            if (!handled && confirmBtn.contains(tap.x, tap.y) && len > 0) {
                done = true;
                confirmed = true;
                handled = true;
            }

            if (handled) redraw();
        }

        return confirmed ? String(buf) : String();
    }

    // Wortweiser Zeilenumbruch anhand echter Pixelbreite (CLAUDE.md-Pflicht
    // fuer Text variabler Laenge) - dupliziert statt geteilt, gleiches
    // Muster wie in aircraft_watchlist_screen.cpp/location_presets_screen.cpp
    // etc. Hier ohne Scroll-Bedarf genutzt (viewTop/viewBottom grosszuegig
    // bemessen), nur fuer den wortweisen Umbruch selbst.
    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                          int16_t lineHeight, const String& text, int16_t scrollY,
                          int16_t viewTop, int16_t viewBottom, bool draw) {
        int16_t y = startY;
        int32_t start = 0;
        int32_t len = text.length();
        while (start < len) {
            while (start < len && text[start] == ' ') start++;
            if (start >= len) break;

            String line = text.substring(start, len);
            while (tft.textWidth(line) > maxWidth) {
                int32_t lastSpace = line.lastIndexOf(' ');
                if (lastSpace <= 0) break;
                line = line.substring(0, lastSpace);
            }

            if (draw) {
                int16_t screenY = y - scrollY;
                if (screenY >= viewTop && screenY <= viewBottom) {
                    tft.setCursor(x, screenY);
                    tft.print(line);
                }
            }
            y += lineHeight;
            start += line.length();
        }
        return y;
    }
}

void run(TFT_eSPI& tft) {
    constexpr int16_t ROW_H = 32;
    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t REMOVE_BTN_W = 60;

    // Modus-Umschalter-Zeile (Alex' Wunsch: bidirektionaler Filter,
    // "Ausblenden"/"Nur anzeigen") - kompakte 22px-Buttonzeile, gleiches
    // "?"-Info-Button-Muster wie die Kaestchen-Zeilen in
    // radar_theme_screen.cpp (dort dupliziert, hier ebenfalls, siehe
    // CLAUDE.md "jeder Screen unabhaengig lauffaehig").
    constexpr int16_t MODE_ROW_Y = 20;
    constexpr int16_t MODE_ROW_H = 22;
    constexpr int16_t MODE_INFO_BTN_SIZE = 20;

    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::AIRLINE_FILTER_TITLE));

        bool showOnly = SettingsStore::airlineFilterShowOnlyMode();

        Rect modeRow = {10, MODE_ROW_Y, (int16_t)(Config::SCREEN_WIDTH - 20), MODE_ROW_H};
        drawButton(tft, modeRow, showOnly ? I18n::t(StringId::AIRLINE_FILTER_MODE_SHOW_ONLY)
                                           : I18n::t(StringId::AIRLINE_FILTER_MODE_HIDE));
        Rect modeInfoBtn = {(int16_t)(modeRow.x + modeRow.w - MODE_INFO_BTN_SIZE - 4),
                             (int16_t)(modeRow.y + (modeRow.h - MODE_INFO_BTN_SIZE) / 2),
                             MODE_INFO_BTN_SIZE, MODE_INFO_BTN_SIZE};
        drawButton(tft, modeInfoBtn, "?");

        // Beschreibung passend zum aktuellen Modus - ueber layoutWrapped()
        // statt der frueheren fest verdrahteten zwei Zeilen, da der neue
        // "Nur anzeigen"-Text in manchen Sprachen laenger ausfaellt als in
        // die bisherigen zwei Zeilen passt.
        constexpr int16_t DESC_LINE_H = 12;
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        int16_t descStartY = MODE_ROW_Y + MODE_ROW_H + 14;
        String desc = showOnly
            ? String(I18n::t(StringId::AIRLINE_FILTER_DESC_SHOWONLY))
            : String(I18n::t(StringId::AIRLINE_FILTER_DESC1)) + " " + I18n::t(StringId::AIRLINE_FILTER_DESC2);
        int16_t descEndY = layoutWrapped(tft, 10, descStartY, Config::SCREEN_WIDTH - 20, DESC_LINE_H,
                                          desc, 0, 0, Config::SCREEN_HEIGHT, true);

        uint8_t count = AirlineFilter::count();
        int16_t y = descEndY + 8;

        Rect rowRects[AirlineFilter::MAX_HIDDEN];
        Rect removeRects[AirlineFilter::MAX_HIDDEN];

        for (uint8_t i = 0; i < count; i++) {
            Rect rowRect = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20 - REMOVE_BTN_W - 6), ROW_H};
            Rect removeRect = {(int16_t)(Config::SCREEN_WIDTH - 10 - REMOVE_BTN_W), y, REMOVE_BTN_W, ROW_H};
            rowRects[i] = rowRect;
            removeRects[i] = removeRect;

            tft.fillRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_BLACK);
            tft.drawRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, UiTheme::accentColor(tft));
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.drawString(AirlineFilter::icaoAt(i), rowRect.x + rowRect.w / 2, rowRect.y + rowRect.h / 2);
            tft.setTextDatum(TL_DATUM);
            drawButton(tft, removeRect, "X", true);

            y += ROW_H + ROW_GAP;
        }

        Rect addBtn = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        bool canAdd = count < AirlineFilter::MAX_HIDDEN;
        if (canAdd) {
            drawButton(tft, addBtn, I18n::t(StringId::AIRLINE_FILTER_ADD));
            y += 40 + 10;
        }

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            // Inaktivitaets-Timeout - siehe SettingsStore::menuIdleTimeoutMs().
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }

        bool handled = false;
        // "?"-Info-Button zuerst pruefen (kleine Flaeche innerhalb der
        // Modus-Zeile) - sonst wuerde ein Tap darauf faelschlich als Tap
        // auf die ganze Zeile (Modus umschalten) gewertet.
        if (!handled && modeInfoBtn.contains(tap.x, tap.y)) {
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::AIRLINE_FILTER_MODE_INFO_TITLE),
                                        I18n::t(StringId::AIRLINE_FILTER_MODE_INFO_BODY), UiTheme::accentColor(tft),
                                        I18n::t(StringId::OK));
            handled = true;
        }
        if (!handled && modeRow.contains(tap.x, tap.y)) {
            SettingsStore::setAirlineFilterShowOnlyMode(!showOnly);
            handled = true;
        }
        for (uint8_t i = 0; i < count && !handled; i++) {
            if (removeRects[i].contains(tap.x, tap.y)) {
                AirlineFilter::removeHidden(i);
                handled = true;
            }
        }
        if (!handled && canAdd && addBtn.contains(tap.x, tap.y)) {
            String code = runLetterKeypad(tft);
            if (code.length() > 0) {
                // 2 Zeichen = IATA, per Tabelle auf ICAO aufgeloest (siehe
                // resolveToIcao() oben) - 3 Zeichen gelten als bereits-ICAO
                // und werden unveraendert durchgereicht. Kein Treffer (z.B.
                // unbekannter IATA-Code) fuegt bewusst nichts hinzu, statt
                // einen falschen ICAO-Eintrag zu speichern.
                String icao = resolveToIcao(code);
                if (icao.length() > 0) {
                    AirlineFilter::addHidden(icao.c_str());
                }
            }
            handled = true;
        }
        if (!handled && backBtn.contains(tap.x, tap.y)) {
            done = true;
        }
    }
}

}