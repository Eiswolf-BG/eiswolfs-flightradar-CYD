#include "wifi_setup_screen.h"
#include "touch_input.h"
#include "wifi_manager.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "ui_theme.h"

namespace WifiSetupScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    enum class KeyType { Char, Shift, Backspace, TogglePage, Space, Connect };

    struct KeyDef {
        KeyType type;
        char ch;
        const char* label;
    };

    constexpr KeyDef ROW_LETTERS_A[] = {
        {KeyType::Char,'q',nullptr},{KeyType::Char,'w',nullptr},{KeyType::Char,'e',nullptr},
        {KeyType::Char,'r',nullptr},{KeyType::Char,'t',nullptr},{KeyType::Char,'y',nullptr},
        {KeyType::Char,'u',nullptr},{KeyType::Char,'i',nullptr},{KeyType::Char,'o',nullptr},
        {KeyType::Char,'p',nullptr},
    };
    constexpr KeyDef ROW_LETTERS_B[] = {
        {KeyType::Char,'a',nullptr},{KeyType::Char,'s',nullptr},{KeyType::Char,'d',nullptr},
        {KeyType::Char,'f',nullptr},{KeyType::Char,'g',nullptr},{KeyType::Char,'h',nullptr},
        {KeyType::Char,'j',nullptr},{KeyType::Char,'k',nullptr},{KeyType::Char,'l',nullptr},
    };
    constexpr KeyDef ROW_LETTERS_C[] = {
        {KeyType::TogglePage,0,"123"},{KeyType::Shift,0,"^"},
        {KeyType::Char,'z',nullptr},{KeyType::Char,'x',nullptr},{KeyType::Char,'c',nullptr},
        {KeyType::Char,'v',nullptr},{KeyType::Char,'b',nullptr},{KeyType::Char,'n',nullptr},
        {KeyType::Char,'m',nullptr},{KeyType::Backspace,0,"<-"},
    };

    constexpr KeyDef ROW_SYMBOLS_A[] = {
        {KeyType::Char,'1',nullptr},{KeyType::Char,'2',nullptr},{KeyType::Char,'3',nullptr},
        {KeyType::Char,'4',nullptr},{KeyType::Char,'5',nullptr},{KeyType::Char,'6',nullptr},
        {KeyType::Char,'7',nullptr},{KeyType::Char,'8',nullptr},{KeyType::Char,'9',nullptr},
        {KeyType::Char,'0',nullptr},
    };
    constexpr KeyDef ROW_SYMBOLS_B[] = {
        {KeyType::Char,'-',nullptr},{KeyType::Char,'_',nullptr},{KeyType::Char,'=',nullptr},
        {KeyType::Char,'+',nullptr},{KeyType::Char,':',nullptr},{KeyType::Char,';',nullptr},
        {KeyType::Char,'\'',nullptr},{KeyType::Char,'"',nullptr},{KeyType::Char,'?',nullptr},
        {KeyType::Char,'!',nullptr},
    };
    constexpr KeyDef ROW_SYMBOLS_C[] = {
        {KeyType::TogglePage,0,"ABC"},
        {KeyType::Char,'@',nullptr},{KeyType::Char,'#',nullptr},{KeyType::Char,'$',nullptr},
        {KeyType::Char,'%',nullptr},{KeyType::Char,'&',nullptr},{KeyType::Char,'*',nullptr},
        {KeyType::Char,'(',nullptr},{KeyType::Char,')',nullptr},{KeyType::Backspace,0,"<-"},
    };

    constexpr int16_t KB_TOP    = 132;
    constexpr int16_t ROW_H     = 34;
    constexpr int16_t ROW_GAP   = 4;
    constexpr int16_t KEY_GAP   = 3;
    constexpr int16_t SIDE_MARGIN = 4;

    // EnterSsid: manuelle SSID-Eingabe fuer versteckte/nicht ausgestrahlte
    // Netzwerke (Alex' Wunsch) - nutzt DIESELBE Tastatur wie EnterPassword
    // (siehe activeTextBuf/switchActiveText() unten), nur mit anderem
    // Zielpuffer (manualSsidBuf statt passwordBuf) und anderer Beschriftung/
    // anderem Folgeschritt (fuehrt zu EnterPassword statt direkt zu
    // verbinden).
    enum class Stage { Scanning, PickSsid, EnterSsid, EnterPassword, Connecting, Done };
    Stage stage = Stage::Scanning;

    constexpr uint8_t MAX_LIST = 16;
    // Von 7 auf 6 reduziert, um unten Platz fuer den neuen "Andere/
    // versteckte SSID"-Button zu schaffen, ohne mit dem Scroll-Pfeil
    // (downBtn, siehe unten) zu kollidieren - bei 7 reichte downBtn bereits
    // bis y=294, nur noch 26px Rest bis zum Bildschirmrand (320).
    constexpr uint8_t VISIBLE_ITEMS = 6;
    String ssidList[MAX_LIST];
    uint8_t ssidCount = 0;
    int8_t selectedIndex = -1;
    uint8_t scrollOffset = 0;

    // Tatsaechliche SSID, mit der am Ende verbunden/gespeichert wird -
    // entweder aus der Scan-Liste uebernommen (ssidList[selectedIndex]) oder
    // manuell eingetippt (manualSsidBuf) - EIN gemeinsamer String statt
    // Sonderfall-Logik weiter unten (Alex' ausdruecklicher Wunsch: "kein
    // Sonderfall in der Speicherlogik noetig, nur der Eingabeweg
    // unterscheidet sich").
    String pendingSsid;

    char passwordBuf[64] = {0};
    uint8_t passwordLen = 0;
    // Max. SSID-Laenge lt. 802.11-Standard: 32 Bytes + Nullterminator.
    char manualSsidBuf[33] = {0};
    uint8_t manualSsidLen = 0;

    // Zeigt auf den GERADE aktiven Eingabepuffer fuer die gemeinsam genutzte
    // Tastatur (handleRowTap()/renderRow() unten) - passwordBuf waehrend
    // Stage::EnterPassword, manualSsidBuf waehrend Stage::EnterSsid. Per
    // switchActiveText() umgeschaltet, siehe dort.
    char* activeTextBuf = passwordBuf;
    uint8_t* activeTextLen = &passwordLen;
    size_t activeTextMaxLen = sizeof(passwordBuf) - 1;

    enum class ShiftState { Off, OneShot, Locked };
    ShiftState shiftState = ShiftState::Off;
    uint32_t lastShiftTapMs = 0;
    constexpr uint32_t SHIFT_DOUBLE_TAP_MS = 400;
    uint8_t page = 0;

    bool skipped = false;
    bool connectSucceeded = false;
    bool needsRedraw = true;

    Rect cancelBtn = {Config::SCREEN_WIDTH - 34, 4, 30, 24};

    // "Andere/versteckte SSID"-Button unterhalb der Netzwerkliste (siehe
    // PickSsid-Stage) - feste Position, unabhaengig von ssidCount, direkt
    // unter dem tiefstmoeglichen Scroll-Pfeil (downBtn bei VISIBLE_ITEMS=6
    // reicht bis y=260, siehe dort) mit 6px Abstand, damit auch bei voller
    // Liste keine Ueberlappung entsteht.
    constexpr int16_t MANUAL_SSID_BTN_Y = 266;
    constexpr int16_t MANUAL_SSID_BTN_H = 32;
    Rect manualSsidBtn = {10, MANUAL_SSID_BTN_Y, (int16_t)(Config::SCREEN_WIDTH - 20), MANUAL_SSID_BTN_H};

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool highlighted = false) {
        uint16_t bg = highlighted ? UiTheme::accentColor(tft) : TFT_BLACK;
        uint16_t fg = highlighted ? TFT_BLACK : UiTheme::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, UiTheme::accentColor(tft));
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    template <size_t N>
    void layoutRow(const KeyDef (&row)[N], int16_t y, Rect outRects[N]) {
        int16_t usableW = Config::SCREEN_WIDTH - 2 * SIDE_MARGIN;
        int16_t keyW = (usableW - (int16_t)(N - 1) * KEY_GAP) / (int16_t)N;
        int16_t x = SIDE_MARGIN;
        for (size_t i = 0; i < N; i++) {
            outRects[i] = {x, y, keyW, ROW_H};
            x += keyW + KEY_GAP;
        }
    }

    String keyLabel(const KeyDef& k) {
        if (k.label) return String(k.label);
        char c = (shiftState != ShiftState::Off) ? (char)toupper(k.ch) : k.ch;
        return String(c);
    }

    template <size_t N>
    bool handleRowTap(const KeyDef (&row)[N], int16_t y, int16_t tx, int16_t ty) {
        Rect rects[N];
        layoutRow(row, y, rects);
        for (size_t i = 0; i < N; i++) {
            if (!rects[i].contains(tx, ty)) continue;
            const KeyDef& k = row[i];
            switch (k.type) {
                case KeyType::Char:
                    if (*activeTextLen < activeTextMaxLen) {
                        bool upper = (shiftState != ShiftState::Off);
                        activeTextBuf[(*activeTextLen)++] = upper ? (char)toupper(k.ch) : k.ch;
                        activeTextBuf[*activeTextLen] = 0;
                        if (shiftState == ShiftState::OneShot) shiftState = ShiftState::Off;
                    }
                    break;
                case KeyType::Shift: {
                    uint32_t nowMs = millis();
                    bool isDoubleTap = (nowMs - lastShiftTapMs) <= SHIFT_DOUBLE_TAP_MS;
                    lastShiftTapMs = nowMs;
                    if (isDoubleTap) {
                        shiftState = (shiftState == ShiftState::Locked) ? ShiftState::Off : ShiftState::Locked;
                    } else if (shiftState == ShiftState::Off) {
                        shiftState = ShiftState::OneShot;
                    } else {
                        shiftState = ShiftState::Off;
                    }
                    break;
                }
                case KeyType::Backspace:
                    if (*activeTextLen > 0) {
                        (*activeTextLen)--;
                        activeTextBuf[*activeTextLen] = 0;
                    }
                    break;
                case KeyType::TogglePage:
                    page = page == 0 ? 1 : 0;
                    break;
                default:
                    break;
            }
            return true;
        }
        return false;
    }

    template <size_t N>
    void renderRow(TFT_eSPI& tft, const KeyDef (&row)[N], int16_t y) {
        Rect rects[N];
        layoutRow(row, y, rects);
        for (size_t i = 0; i < N; i++) {
            bool hl = (row[i].type == KeyType::Shift && shiftState != ShiftState::Off);
            drawButton(tft, rects[i], keyLabel(row[i]), hl);
        }
    }

    // Setzt BEIDE Texteingabe-Puffer zurueck (auch wenn nur einer davon
    // gerade aktiv ist - kostet praktisch nichts, verhindert aber, dass
    // beim naechsten Wechsel zwischen SSID-/Passwort-Eingabe alte Reste
    // vom vorherigen Versuch stehen bleiben) und laesst activeText* danach
    // auf passwordBuf zeigen (Standardfall: Passwort-Eingabe fuer ein aus
    // der Liste gewaehltes Netzwerk). Fuer die manuelle SSID-Eingabe direkt
    // danach switchActiveText() aufrufen, siehe dort.
    void resetKeyboardState() {
        passwordLen = 0;
        passwordBuf[0] = 0;
        manualSsidLen = 0;
        manualSsidBuf[0] = 0;
        shiftState = ShiftState::Off;
        page = 0;
        activeTextBuf = passwordBuf;
        activeTextLen = &passwordLen;
        activeTextMaxLen = sizeof(passwordBuf) - 1;
    }

    // Schaltet die gemeinsam genutzte Tastatur (handleRowTap()/renderRow())
    // auf einen anderen Zielpuffer um - aktuell nur fuer den Wechsel zur
    // manuellen SSID-Eingabe gebraucht (siehe PickSsid-Tap-Handler unten).
    void switchActiveText(char* buf, uint8_t* len, size_t maxLen) {
        activeTextBuf = buf;
        activeTextLen = len;
        activeTextMaxLen = maxLen;
    }

    void drawCancelButton(TFT_eSPI& tft) {
        drawButton(tft, cancelBtn, "X");
    }

    // Small loading indicator ("."/".."/"...") during the WiFi scan (which
    // can take several seconds) - without visible feedback the screen
    // looked "frozen" during that time and people tapped impatiently,
    // which (see comment at the end of the Scanning->PickSsid transition
    // below) could cause mistaps in the freshly shown network list.
    uint8_t scanDotPhase = 0;
    uint32_t lastScanDotMs = 0;
    constexpr uint32_t SCAN_DOT_INTERVAL_MS = 350;

    void updateScanningDots(TFT_eSPI& tft) {
        uint32_t now = millis();
        if (now - lastScanDotMs < SCAN_DOT_INTERVAL_MS) return;
        lastScanDotMs = now;
        scanDotPhase = (uint8_t)((scanDotPhase + 1) % 4);
        tft.fillRect(10, 26, 40, 12, TFT_BLACK);
        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 26);
        for (uint8_t i = 0; i < scanDotPhase; i++) tft.print(".");
    }
}

bool run(TFT_eSPI& tft) {
    MenuStars::reset();
    stage = Stage::Scanning;
    skipped = false;
    connectSucceeded = false;
    ssidCount = 0;
    selectedIndex = -1;
    scrollOffset = 0;
    pendingSsid = "";
    resetKeyboardState();
    needsRedraw = true;
    scanDotPhase = 0;
    lastScanDotMs = 0;

    WifiMgr::beginScan();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::WIFI_SCANNING));
    drawCancelButton(tft);

    while (!skipped) {
        TouchInput::Point tap;
        bool tapped = TouchInput::wasTapped(tap);

        if (tapped && cancelBtn.contains(tap.x, tap.y)) {
            skipped = true;
            break;
        }

        if (stage == Stage::Scanning && WifiMgr::isScanComplete()) {
            ssidCount = (uint8_t)min((int)WifiMgr::getScanResultCount(), (int)MAX_LIST);
            for (uint8_t i = 0; i < ssidCount; i++) ssidList[i] = WifiMgr::getScanResultSSID(i);
            stage = Stage::PickSsid;
            needsRedraw = true;

            tft.fillScreen(TFT_BLACK);
            tft.setCursor(10, 14);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.println(ssidCount == 0 ? I18n::t(StringId::WIFI_NO_NETWORKS) : I18n::t(StringId::WIFI_SELECT));
            drawCancelButton(tft);

            // "tapped"/"tap" were captured for the previous state
            // (Stage::Scanning) - without discarding it, a tap that
            // happened to land exactly in the frame the scan finished
            // would immediately be evaluated below against the
            // BRAND-NEW (and, to the user, not yet visible) network list
            // - at a position the user never intentionally tapped. That
            // is what let impatient repeated tapping during the scan
            // eventually pick a "random" wrong network.
            tapped = false;
        }

        if (stage == Stage::Connecting) {
            WifiMgr::update();
            if (WifiMgr::getState() == WifiMgr::State::Connected) {
                connectSucceeded = true;
                WifiMgr::addNetwork(pendingSsid.c_str(), passwordBuf);
                stage = Stage::Done;
                tft.fillScreen(TFT_BLACK);
                tft.setCursor(10, 14);
                tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
                tft.println(I18n::t(StringId::WIFI_CONNECTED_BANG));
                delay(900);
                return true;
            } else if (WifiMgr::getState() == WifiMgr::State::Failed) {
                stage = Stage::Done;
                tft.fillScreen(TFT_BLACK);
                tft.setCursor(10, 14);
                tft.setTextColor(TFT_RED, TFT_BLACK);
                tft.println(I18n::t(StringId::WIFI_CONNECTION_FAILED));
                tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
                tft.setCursor(10, 30);
                tft.println(I18n::t(StringId::WIFI_BACK_TO_LIST_MSG));
                delay(1400);
                stage = Stage::PickSsid;
                WifiMgr::beginScan();
                stage = Stage::Scanning;
            }
        }

        if (tapped && stage == Stage::PickSsid) {
            if (ssidCount > 0) {
                for (uint8_t row = 0; row < VISIBLE_ITEMS; row++) {
                    uint8_t idx = scrollOffset + row;
                    if (idx >= ssidCount) break;
                    Rect r = {10, (int16_t)(34 + row * 34), (int16_t)(Config::SCREEN_WIDTH - 20), 30};
                    if (r.contains(tap.x, tap.y)) {
                        selectedIndex = idx;
                        pendingSsid = ssidList[idx];
                        resetKeyboardState();
                        stage = Stage::EnterPassword;
                        needsRedraw = true;
                        // Prevents this same tap (on a network name) from
                        // also being evaluated against the password
                        // keyboard further down in the same pass - "stage"
                        // is already EnterPassword at this point, and the
                        // network list rows and keyboard key rows
                        // partially overlap in Y, so without this a
                        // network selection could immediately also type a
                        // stray character.
                        tapped = false;
                    }
                }
                if (ssidCount > VISIBLE_ITEMS) {
                    Rect upBtn   = {Config::SCREEN_WIDTH - 34, 34, 30, 26};
                    Rect downBtn = {Config::SCREEN_WIDTH - 34, 64 + (VISIBLE_ITEMS - 1) * 34, 30, 26};
                    if (upBtn.contains(tap.x, tap.y) && scrollOffset > 0) { scrollOffset--; needsRedraw = true; }
                    if (downBtn.contains(tap.x, tap.y) && scrollOffset + VISIBLE_ITEMS < ssidCount) { scrollOffset++; needsRedraw = true; }
                }
            }
            // "Andere/versteckte SSID"-Button - IMMER sichtbar/antippbar,
            // auch wenn ssidCount==0 (der Sinn hinter versteckten Netzwerken
            // ist ja gerade, dass sie im Scan gar nicht erst auftauchen).
            // Fuehrt zur selben Tastatur wie die Passwort-Eingabe, nur mit
            // manualSsidBuf als Ziel (switchActiveText()) und Stage::EnterSsid
            // statt Stage::EnterPassword als naechstem Schritt.
            if (manualSsidBtn.contains(tap.x, tap.y)) {
                resetKeyboardState();
                switchActiveText(manualSsidBuf, &manualSsidLen, sizeof(manualSsidBuf) - 1);
                stage = Stage::EnterSsid;
                needsRedraw = true;
            }
        }

        if (tapped && (stage == Stage::EnterSsid || stage == Stage::EnterPassword)) {
            int16_t rowY0 = KB_TOP;
            int16_t rowY1 = KB_TOP + (ROW_H + ROW_GAP);
            int16_t rowY2 = KB_TOP + 2 * (ROW_H + ROW_GAP);
            int16_t rowYFn = KB_TOP + 3 * (ROW_H + ROW_GAP);

            bool handled = false;
            if (page == 0) {
                handled = handleRowTap(ROW_LETTERS_A, rowY0, tap.x, tap.y) ||
                          handleRowTap(ROW_LETTERS_B, rowY1, tap.x, tap.y) ||
                          handleRowTap(ROW_LETTERS_C, rowY2, tap.x, tap.y);
            } else {
                handled = handleRowTap(ROW_SYMBOLS_A, rowY0, tap.x, tap.y) ||
                          handleRowTap(ROW_SYMBOLS_B, rowY1, tap.x, tap.y) ||
                          handleRowTap(ROW_SYMBOLS_C, rowY2, tap.x, tap.y);
            }

            if (!handled) {
                Rect spaceBtn    = {SIDE_MARGIN, rowYFn, 150, ROW_H};
                Rect submitBtn   = {SIDE_MARGIN + 154, rowYFn, Config::SCREEN_WIDTH - 2*SIDE_MARGIN - 154, ROW_H};
                Rect backBtn     = {SIDE_MARGIN, (int16_t)(rowYFn + ROW_H + ROW_GAP), (int16_t)(Config::SCREEN_WIDTH - 2*SIDE_MARGIN), 28};

                if (spaceBtn.contains(tap.x, tap.y)) {
                    if (*activeTextLen < activeTextMaxLen) {
                        activeTextBuf[(*activeTextLen)++] = ' ';
                        activeTextBuf[*activeTextLen] = 0;
                    }
                } else if (submitBtn.contains(tap.x, tap.y)) {
                    if (stage == Stage::EnterSsid) {
                        // Leere SSID ergibt keinen Sinn - Tap wird einfach
                        // ignoriert (kein Fehlertext noetig, der Nutzer
                        // sieht ja direkt, dass sich nichts tut).
                        if (manualSsidLen > 0) {
                            pendingSsid = String(manualSsidBuf);
                            resetKeyboardState();
                            stage = Stage::EnterPassword;
                            needsRedraw = true;
                        }
                    } else {
                        if (WifiMgr::networkCount() >= Config::MAX_WIFI_NETWORKS) {
                            tft.fillScreen(TFT_BLACK);
                            tft.setCursor(10, 14);
                            tft.setTextColor(TFT_RED, TFT_BLACK);
                            tft.println(I18n::t(StringId::WIFI_ALREADY_3));
                            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
                            tft.setCursor(10, 30);
                            tft.println(I18n::t(StringId::WIFI_REMOVE_ONE_FIRST));
                            delay(1600);
                            skipped = true;
                            break;
                        }
                        WifiMgr::connectTo(pendingSsid.c_str(), passwordBuf);
                        stage = Stage::Connecting;
                        tft.fillScreen(TFT_BLACK);
                        tft.setCursor(10, 14);
                        tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
                        tft.println(I18n::t(StringId::WIFI_CONNECTING));
                    }
                } else if (backBtn.contains(tap.x, tap.y)) {
                    stage = Stage::PickSsid;
                    needsRedraw = true;
                }
            }
            needsRedraw = true;
        }

        if (!needsRedraw) {
            if (stage == Stage::Scanning) updateScanningDots(tft);
            MenuStars::update(tft);
            delay(20);
            continue;
        }
        needsRedraw = false;

        if (stage == Stage::PickSsid) {
            tft.fillRect(0, 30, Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT - 30, TFT_BLACK);
            for (uint8_t row = 0; row < VISIBLE_ITEMS; row++) {
                uint8_t idx = scrollOffset + row;
                if (idx >= ssidCount) break;
                Rect r = {10, (int16_t)(34 + row * 34), (int16_t)(Config::SCREEN_WIDTH - 20), 30};
                drawButton(tft, r, ssidList[idx]);
            }
            if (ssidCount > VISIBLE_ITEMS) {
                Rect upBtn   = {Config::SCREEN_WIDTH - 34, 34, 30, 26};
                Rect downBtn = {Config::SCREEN_WIDTH - 34, 64 + (VISIBLE_ITEMS - 1) * 34, 30, 26};
                drawButton(tft, upBtn, "^");
                drawButton(tft, downBtn, "v");
            }
            drawButton(tft, manualSsidBtn, I18n::t(StringId::WIFI_MANUAL_SSID));
            drawCancelButton(tft);
        } else if (stage == Stage::EnterSsid || stage == Stage::EnterPassword) {
            bool isSsidStage = (stage == Stage::EnterSsid);
            tft.fillRect(0, 0, Config::SCREEN_WIDTH, KB_TOP - 4, TFT_BLACK);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            if (isSsidStage) {
                tft.println(I18n::t(StringId::WIFI_ENTER_SSID_LABEL));
            } else {
                tft.printf("%s%s", I18n::t(StringId::WIFI_LABEL_PREFIX), pendingSsid.c_str());
                tft.setCursor(10, 22);
                tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
                tft.println(I18n::t(StringId::WIFI_PASSWORD_LABEL));
            }
            tft.fillRect(8, 38, Config::SCREEN_WIDTH - 16, 22, TFT_BLACK);
            tft.drawRect(8, 38, Config::SCREEN_WIDTH - 16, 22, UiTheme::accentColor(tft));
            tft.setCursor(12, 56);
            tft.setTextColor(UiTheme::accentColor(tft), TFT_BLACK);
            tft.print(activeTextBuf);

            int16_t rowY0 = KB_TOP;
            int16_t rowY1 = KB_TOP + (ROW_H + ROW_GAP);
            int16_t rowY2 = KB_TOP + 2 * (ROW_H + ROW_GAP);
            int16_t rowYFn = KB_TOP + 3 * (ROW_H + ROW_GAP);

            if (page == 0) {
                renderRow(tft, ROW_LETTERS_A, rowY0);
                renderRow(tft, ROW_LETTERS_B, rowY1);
                renderRow(tft, ROW_LETTERS_C, rowY2);
            } else {
                renderRow(tft, ROW_SYMBOLS_A, rowY0);
                renderRow(tft, ROW_SYMBOLS_B, rowY1);
                renderRow(tft, ROW_SYMBOLS_C, rowY2);
            }

            Rect spaceBtn  = {SIDE_MARGIN, rowYFn, 150, ROW_H};
            Rect submitBtn = {SIDE_MARGIN + 154, rowYFn, Config::SCREEN_WIDTH - 2*SIDE_MARGIN - 154, ROW_H};
            Rect backBtn   = {SIDE_MARGIN, (int16_t)(rowYFn + ROW_H + ROW_GAP), (int16_t)(Config::SCREEN_WIDTH - 2*SIDE_MARGIN), 28};
            drawButton(tft, spaceBtn, I18n::t(StringId::WIFI_SPACE));
            drawButton(tft, submitBtn, isSsidStage ? I18n::t(StringId::WIFI_NEXT) : I18n::t(StringId::WIFI_CONNECT));
            drawButton(tft, backBtn, I18n::t(StringId::WIFI_BACK_TO_LIST_BTN));
        }

        delay(20);
    }

    return false;
}

}