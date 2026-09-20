#include "route_watchlist_screen.h"
#include "route_watchlist.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "settings_store.h"
#include "i18n.h"
#include "ui_theme.h"
#include "menu_screen.h"

namespace RouteWatchlistScreen {

namespace {
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

    // Checkbox-Zeile fuer den Ein/Aus-Schalter oben - gleiches Muster wie
    // ntfy_push_screen.cpp::drawCheckboxRow() (dort dupliziert statt
    // geteilt, CLAUDE.md "jeder Screen unabhaengig lauffaehig").
    void drawCheckboxRow(TFT_eSPI& tft, const Rect& r, const String& label, bool checked) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));

        constexpr int16_t BOX_SIZE = 20;
        int16_t boxX = r.x + 10;
        int16_t boxY = (int16_t)(r.y + (r.h - BOX_SIZE) / 2);
        if (checked) {
            tft.fillRoundRect(boxX, boxY, BOX_SIZE, BOX_SIZE, 3, UiTheme::accentColor(tft));
        } else {
            tft.drawRoundRect(boxX, boxY, BOX_SIZE, BOX_SIZE, 3, UiTheme::accentColor(tft));
        }

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.drawString(label, (int16_t)(boxX + BOX_SIZE + 10), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    constexpr int16_t ROW_INFO_BTN_SIZE = 20;
    constexpr int16_t ROW_INFO_BTN_PAD = 6;

    Rect rowInfoBtnRect(const Rect& row) {
        return {(int16_t)(row.x + row.w - ROW_INFO_BTN_SIZE - ROW_INFO_BTN_PAD),
                (int16_t)(row.y + (row.h - ROW_INFO_BTN_SIZE) / 2),
                ROW_INFO_BTN_SIZE, ROW_INFO_BTN_SIZE};
    }

    void drawRowInfoButton(TFT_eSPI& tft, const Rect& row) {
        drawButton(tft, rowInfoBtnRect(row), "?");
    }

    // ICAO-Tastatur (Ziffern + Grossbuchstaben, kein Kleinschreibungs-
    // Umschalter) fuer je EIN Feld (Start ODER Ziel) - gleiches Layout-
    // Muster wie TypeWatchlistScreen::runTypeKeypad(), zusaetzlich mit
    // einem "Leer lassen"-Knopf (Alex' Wunsch: beide Felder sind optional),
    // den die anderen Wachlisten-Tastaturen nicht brauchen (dort ist das
    // Feld immer Pflicht). 'cancelled' wird auf true gesetzt, wenn der
    // GESAMTE Hinzufuegen-Vorgang abgebrochen werden soll (nicht nur dieses
    // eine Feld) - der Aufrufer bricht dann auch den zweiten Schritt ab.
    String runIcaoKeypad(TFT_eSPI& tft, const char* title, bool& cancelled) {
        MenuStars::reset();
        constexpr const char* DIGITS = "1234567890";
        constexpr const char* ROW1 = "QWERTYUIOP";
        constexpr const char* ROW2 = "ASDFGHJKL";
        constexpr const char* ROW3 = "ZXCVBNM";

        char buf[5] = {0};
        uint8_t len = 0;

        constexpr int16_t KEY_H = 30;
        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t ROW0_Y = 78;

        auto layoutRow = [&](const char* row, int16_t y, Rect* outRects, uint8_t n) {
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect digitRects[10], row1Rects[10], row2Rects[9], row3Rects[7];
        layoutRow(DIGITS, ROW0_Y, digitRects, 10);
        layoutRow(ROW1, ROW0_Y + (KEY_H + KEY_GAP), row1Rects, 10);
        layoutRow(ROW2, ROW0_Y + 2 * (KEY_H + KEY_GAP), row2Rects, 9);
        layoutRow(ROW3, ROW0_Y + 3 * (KEY_H + KEY_GAP), row3Rects, 7);

        constexpr int16_t ROW4_Y = ROW0_Y + 4 * (KEY_H + KEY_GAP);
        Rect backspaceBtn = {4, ROW4_Y, 76, KEY_H};
        Rect skipBtn       = {(int16_t)(4 + 76 + KEY_GAP), ROW4_Y, 76, KEY_H};
        Rect cancelBtn     = {(int16_t)(Config::SCREEN_WIDTH - 4 - 76), ROW4_Y, 76, KEY_H};
        Rect confirmBtn    = {4, (int16_t)(ROW4_Y + KEY_H + KEY_GAP), (int16_t)(Config::SCREEN_WIDTH - 8), KEY_H};

        bool done = false;
        bool confirmed = false;
        bool skipped = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(title);

            tft.fillRect(8, 40, Config::SCREEN_WIDTH - 16, 34, TFT_BLACK);
            tft.drawRect(8, 40, Config::SCREEN_WIDTH - 16, 34, UiTheme::accentColor(tft));
            tft.setTextSize(2);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(14, 66);
            tft.print(buf);
            tft.setTextSize(1);

            for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(DIGITS[i]));
            for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(ROW1[i]));
            for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(ROW2[i]));
            for (uint8_t i = 0; i < 7; i++) drawButton(tft, row3Rects[i], String(ROW3[i]));

            drawButton(tft, backspaceBtn, "<-");
            drawButton(tft, skipBtn, I18n::t(StringId::ROUTE_WATCH_SKIP));
            drawButton(tft, cancelBtn, I18n::t(StringId::CANCEL), true);
            drawButton(tft, confirmBtn, I18n::t(StringId::ADD));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (digitRects[i].contains(tap.x, tap.y) && len < 4) { buf[len++] = DIGITS[i]; handled = true; }
            }
            for (uint8_t i = 0; i < 10 && !handled; i++) {
                if (row1Rects[i].contains(tap.x, tap.y) && len < 4) { buf[len++] = ROW1[i]; handled = true; }
            }
            for (uint8_t i = 0; i < 9 && !handled; i++) {
                if (row2Rects[i].contains(tap.x, tap.y) && len < 4) { buf[len++] = ROW2[i]; handled = true; }
            }
            for (uint8_t i = 0; i < 7 && !handled; i++) {
                if (row3Rects[i].contains(tap.x, tap.y) && len < 4) { buf[len++] = ROW3[i]; handled = true; }
            }
            if (!handled && backspaceBtn.contains(tap.x, tap.y)) {
                if (len > 0) { len--; buf[len] = 0; }
                handled = true;
            }
            if (!handled && skipBtn.contains(tap.x, tap.y)) {
                done = true;
                confirmed = true;
                skipped = true;
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

        if (!confirmed) { cancelled = true; return String(); }
        return skipped ? String() : String(buf);
    }

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

    void runInfoScreen(TFT_eSPI& tft, StringId titleId, StringId para1Id, StringId para2Id) {
        MenuStars::reset();

        constexpr int16_t textMaxWidth = Config::SCREEN_WIDTH - 20;
        constexpr int16_t LINE_H = 16;
        int16_t titleEndY = layoutWrapped(tft, 10, 14, textMaxWidth, LINE_H, I18n::t(titleId), 0, 0, 0, false);
        const int16_t VIEW_TOP = titleEndY + 4;
        constexpr int16_t VIEW_BOTTOM = Config::SCREEN_HEIGHT - 60;

        int16_t totalH = VIEW_TOP;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(para1Id), 0, 0, 0, false);
        totalH += 8;
        totalH = layoutWrapped(tft, 10, totalH, textMaxWidth, LINE_H, I18n::t(para2Id), 0, 0, 0, false);

        int16_t maxScroll = totalH - VIEW_BOTTOM;
        if (maxScroll < 0) maxScroll = 0;
        bool scrollable = maxScroll > 0;
        int16_t scrollY = 0;

        Rect backBtn = scrollable
            ? Rect{10, (int16_t)(Config::SCREEN_HEIGHT - 50), 130, 40}
            : Rect{10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect upBtn   = {146, (int16_t)(Config::SCREEN_HEIGHT - 50), 38, 40};
        Rect downBtn = {190, (int16_t)(Config::SCREEN_HEIGHT - 50), 38, 40};
        constexpr int16_t SCROLL_STEP = 48;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            layoutWrapped(tft, 10, 14, textMaxWidth, LINE_H, I18n::t(titleId), 0, 0, Config::SCREEN_HEIGHT, true);

            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            int16_t y = VIEW_TOP;
            y = layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(para1Id), scrollY, VIEW_TOP, VIEW_BOTTOM, true);
            y += 8;
            layoutWrapped(tft, 10, y, textMaxWidth, LINE_H, I18n::t(para2Id), scrollY, VIEW_TOP, VIEW_BOTTOM, true);

            drawButton(tft, backBtn, I18n::t(StringId::BACK));
            if (scrollable) {
                drawButton(tft, upBtn, "^");
                drawButton(tft, downBtn, "v");
            }
        };

        redraw();

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (backBtn.contains(tap.x, tap.y)) return;
                if (scrollable && upBtn.contains(tap.x, tap.y) && scrollY > 0) {
                    scrollY -= SCROLL_STEP;
                    if (scrollY < 0) scrollY = 0;
                    redraw();
                } else if (scrollable && downBtn.contains(tap.x, tap.y) && scrollY < maxScroll) {
                    scrollY += SCROLL_STEP;
                    if (scrollY > maxScroll) scrollY = maxScroll;
                    redraw();
                }
            }
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) return;
            MenuStars::update(tft);
            delay(20);
        }
    }

    // Zeilen-Beschriftung fuer einen Eintrag - "*" steht fuer ein leeres
    // (= egal welcher) Feld, z.B. "EDDS -> *" (nur Start gesetzt) oder
    // "* -> LWSK" (nur Ziel gesetzt).
    String routeLabel(const String& origin, const String& dest) {
        String o = origin.length() ? origin : String("*");
        String d = dest.length() ? dest : String("*");
        return o + " -> " + d;
    }
}

void run(TFT_eSPI& tft) {
    bool done = false;
    MenuStars::reset();
    while (!done) {
        tft.fillScreen(TFT_BLACK);

        Rect infoBtn = {(int16_t)(Config::SCREEN_WIDTH - 40), 2, 30, 24};
        drawButton(tft, infoBtn, "?");

        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::ROUTE_WATCH_TITLE));

        // Eigener Ein/Aus-Schalter (anders als die anderen drei Listen) -
        // steuert sowohl den Alarm als auch, ob ueberhaupt ein Hintergrund-
        // Lookup ausgeloest wird (RouteWatchlist::pollBackground()).
        Rect enableRow = {10, 30, (int16_t)(Config::SCREEN_WIDTH - 20), 30};
        drawCheckboxRow(tft, enableRow, I18n::t(StringId::ROUTE_WATCH_ENABLE_LABEL),
                         SettingsStore::routeWatchlistAlertEnabled());
        drawRowInfoButton(tft, enableRow);

        // Erklaertext ueber layoutWrapped() statt rohem setCursor()/println()
        // (Alex' Meldung: TFT_eSPI's eingebautes Auto-Wrap liess den
        // laengeren deutschen Text auf eine dritte Zeile umbrechen, die dann
        // mit dem darunter fest positionierten "leere Liste"-Platzhalter
        // ueberlappte) - DESC1/DESC2 werden dafuer zu einem durchgehenden
        // Absatz zusammengefuegt, echter wortweiser Umbruch anhand der
        // tatsaechlichen Pixelbreite in JEDER Sprache, und die zurueck-
        // gegebene End-Y-Position bestimmt dynamisch, wo die Liste beginnt -
        // CLAUDE.md-Pflicht fuer variablen Text.
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        String descText = String(I18n::t(StringId::ROUTE_WATCH_DESC1)) + " " +
                           I18n::t(StringId::ROUTE_WATCH_DESC2);
        int16_t descEndY = layoutWrapped(tft, 10, 72, (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                                          descText, 0, 0, Config::SCREEN_HEIGHT, true);

        uint8_t count = RouteWatchlist::count();
        bool canAdd = count < RouteWatchlist::MAX_WATCHED;
        int16_t y = (int16_t)(descEndY + 14);

        // Zeilenhoehe/-abstand werden aus dem TATSAECHLICH noch verfuegbaren
        // Platz errechnet (Alex' Meldung: viel ungenutzter Platz unten bei
        // wenigen Eintraegen) statt fest verdrahtet zu sein - bei wenigen
        // Eintraegen wird jede Zeile grosszuegiger, bei einer vollen Liste
        // (bis zu MAX_WATCHED=5) automatisch kompakter, damit "Hinzufuegen"/
        // "Zurueck" auch dann garantiert noch auf den Bildschirm passen,
        // unabhaengig davon, wie lang der Erklaertext oben in der jeweiligen
        // Sprache ausgefallen ist (descEndY ist bereits das tatsaechliche
        // Messergebnis von layoutWrapped() oben, keine Schaetzung).
        constexpr int16_t ADD_BTN_H = 44;
        constexpr int16_t BACK_BTN_H = 40;
        constexpr int16_t BOTTOM_MARGIN = 10;
        int16_t reserved = (int16_t)((canAdd ? ADD_BTN_H + 10 : 0) + BACK_BTN_H + 10);
        int16_t availableForList = (int16_t)(Config::SCREEN_HEIGHT - BOTTOM_MARGIN - y - reserved);
        uint8_t rowSlots = count > 0 ? count : 1;
        int16_t rowTotal = availableForList / rowSlots;
        if (rowTotal > 46) rowTotal = 46; // bei wenig Eintraegen nicht uebertrieben hoch
        if (rowTotal < 20) rowTotal = 20; // Mindesthoehe fuer Lesbarkeit/Antippbarkeit
        int16_t ROW_GAP = (int16_t)(rowTotal / 5);
        if (ROW_GAP < 4) ROW_GAP = 4;
        if (ROW_GAP > 10) ROW_GAP = 10;
        int16_t ROW_H = (int16_t)(rowTotal - ROW_GAP);
        constexpr int16_t REMOVE_BTN_W = 60;

        Rect rowRects[RouteWatchlist::MAX_WATCHED];
        Rect removeRects[RouteWatchlist::MAX_WATCHED];

        if (count == 0) {
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.drawString(I18n::t(StringId::ROUTE_WATCH_EMPTY), Config::SCREEN_WIDTH / 2,
                            (int16_t)(y + ROW_H / 2));
            tft.setTextDatum(TL_DATUM);
            y += ROW_H;
        }

        for (uint8_t i = 0; i < count; i++) {
            Rect rowRect = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20 - REMOVE_BTN_W - 6), ROW_H};
            Rect removeRect = {(int16_t)(Config::SCREEN_WIDTH - 10 - REMOVE_BTN_W), y, REMOVE_BTN_W, ROW_H};
            rowRects[i] = rowRect;
            removeRects[i] = removeRect;

            tft.fillRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, TFT_BLACK);
            tft.drawRoundRect(rowRect.x, rowRect.y, rowRect.w, rowRect.h, 4, UiTheme::accentColor(tft));
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.drawString(routeLabel(RouteWatchlist::originAt(i), RouteWatchlist::destAt(i)),
                            rowRect.x + rowRect.w / 2, rowRect.y + rowRect.h / 2);
            tft.setTextDatum(TL_DATUM);
            drawButton(tft, removeRect, "X", true);

            y += ROW_H + ROW_GAP;
        }

        y += 10; // zusaetzlicher Luftabstand vor dem "Hinzufuegen"-Button
        Rect addBtn = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), ADD_BTN_H};
        if (canAdd) {
            drawButton(tft, addBtn, I18n::t(StringId::ROUTE_WATCH_ADD));
            y += ADD_BTN_H + 10;
        }

        // Dynamisch statt fester Y-Position (anders als bei den anderen
        // Wachlisten-Screens) - hier kommt zusaetzlich die Checkbox-Zeile
        // oben dazu, ein fester Wert haette bei voller Liste zu knapp
        // bemessenem/ueberlappendem Platz gefuehrt.
        Rect backBtn = {10, y, (int16_t)(Config::SCREEN_WIDTH - 20), BACK_BTN_H};
        drawButton(tft, backBtn, I18n::t(StringId::BACK));

        TouchInput::Point tap;
        while (true) {
            if (TouchInput::wasTapped(tap)) break;
            if (TouchInput::msSinceLastTap() >= SettingsStore::menuIdleTimeoutMs()) { done = true; break; }
            MenuStars::update(tft);
            delay(20);
        }
        if (done) break;

        bool handled = false;
        if (infoBtn.contains(tap.x, tap.y)) {
            runInfoScreen(tft, StringId::ROUTE_WATCH_INFO_TITLE, StringId::ROUTE_WATCH_INFO_PARA1,
                          StringId::ROUTE_WATCH_INFO_PARA2);
            handled = true;
        } else if (rowInfoBtnRect(enableRow).contains(tap.x, tap.y)) {
            // VOR dem Zeilen-Toggle direkt unten geprueft, sonst wuerde der
            // kleine "?"-Button von der groesseren Zeilen-Bounding-Box
            // geschluckt (gleiches Muster wie bei allen anderen
            // "?"-Buttons im Projekt).
            MenuScreen::showInfoScreen(tft, I18n::t(StringId::ROUTE_WATCH_ENABLE_INFO_TITLE),
                                        I18n::t(StringId::ROUTE_WATCH_ENABLE_INFO_BODY),
                                        UiTheme::accentColor(tft), I18n::t(StringId::OK));
            handled = true;
        } else if (enableRow.contains(tap.x, tap.y)) {
            SettingsStore::setRouteWatchlistAlertEnabled(!SettingsStore::routeWatchlistAlertEnabled());
            handled = true;
        }
        for (uint8_t i = 0; i < count && !handled; i++) {
            if (removeRects[i].contains(tap.x, tap.y)) {
                RouteWatchlist::removeWatched(i);
                handled = true;
            }
        }
        if (!handled && canAdd && addBtn.contains(tap.x, tap.y)) {
            bool cancelled = false;
            String origin = runIcaoKeypad(tft, I18n::t(StringId::ROUTE_WATCH_ADD_ORIGIN_TITLE), cancelled);
            if (!cancelled) {
                String dest = runIcaoKeypad(tft, I18n::t(StringId::ROUTE_WATCH_ADD_DEST_TITLE), cancelled);
                if (!cancelled && (origin.length() > 0 || dest.length() > 0)) {
                    RouteWatchlist::addWatched(origin.c_str(), dest.c_str());
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
