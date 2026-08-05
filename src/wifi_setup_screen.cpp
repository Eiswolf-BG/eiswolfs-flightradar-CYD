#include "wifi_setup_screen.h"
#include "touch_input.h"
#include "wifi_manager.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

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

    enum class Stage { Scanning, PickSsid, EnterPassword, Connecting, Done };
    Stage stage = Stage::Scanning;

    constexpr uint8_t MAX_LIST = 16;
    constexpr uint8_t VISIBLE_ITEMS = 7;
    String ssidList[MAX_LIST];
    uint8_t ssidCount = 0;
    int8_t selectedIndex = -1;
    uint8_t scrollOffset = 0;

    char passwordBuf[64] = {0};
    uint8_t passwordLen = 0;

    enum class ShiftState { Off, OneShot, Locked };
    ShiftState shiftState = ShiftState::Off;
    uint32_t lastShiftTapMs = 0;
    constexpr uint32_t SHIFT_DOUBLE_TAP_MS = 400;
    uint8_t page = 0;

    bool skipped = false;
    bool connectSucceeded = false;
    bool needsRedraw = true;

    Rect cancelBtn = {Config::SCREEN_WIDTH - 34, 4, 30, 24};

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool highlighted = false) {
        uint16_t bg = highlighted ? TFT_GREEN : TFT_BLACK;
        uint16_t fg = highlighted ? TFT_BLACK : TFT_GREEN;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
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
                    if (passwordLen < sizeof(passwordBuf) - 1) {
                        bool upper = (shiftState != ShiftState::Off);
                        passwordBuf[passwordLen++] = upper ? (char)toupper(k.ch) : k.ch;
                        passwordBuf[passwordLen] = 0;
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
                    if (passwordLen > 0) {
                        passwordLen--;
                        passwordBuf[passwordLen] = 0;
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

    void resetKeyboardState() {
        passwordLen = 0;
        passwordBuf[0] = 0;
        shiftState = ShiftState::Off;
        page = 0;
    }

    void drawCancelButton(TFT_eSPI& tft) {
        drawButton(tft, cancelBtn, "X");
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
    resetKeyboardState();
    needsRedraw = true;

    WifiMgr::beginScan();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
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
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.println(ssidCount == 0 ? I18n::t(StringId::WIFI_NO_NETWORKS) : I18n::t(StringId::WIFI_SELECT));
            drawCancelButton(tft);
        }

        if (stage == Stage::Connecting) {
            WifiMgr::update();
            if (WifiMgr::getState() == WifiMgr::State::Connected) {
                connectSucceeded = true;
                WifiMgr::addNetwork(ssidList[selectedIndex].c_str(), passwordBuf);
                stage = Stage::Done;
                tft.fillScreen(TFT_BLACK);
                tft.setCursor(10, 14);
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.println(I18n::t(StringId::WIFI_CONNECTED_BANG));
                delay(900);
                return true;
            } else if (WifiMgr::getState() == WifiMgr::State::Failed) {
                stage = Stage::Done;
                tft.fillScreen(TFT_BLACK);
                tft.setCursor(10, 14);
                tft.setTextColor(TFT_RED, TFT_BLACK);
                tft.println(I18n::t(StringId::WIFI_CONNECTION_FAILED));
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
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
                        resetKeyboardState();
                        stage = Stage::EnterPassword;
                        needsRedraw = true;
                    }
                }
                if (ssidCount > VISIBLE_ITEMS) {
                    Rect upBtn   = {Config::SCREEN_WIDTH - 34, 34, 30, 26};
                    Rect downBtn = {Config::SCREEN_WIDTH - 34, 64 + (VISIBLE_ITEMS - 1) * 34, 30, 26};
                    if (upBtn.contains(tap.x, tap.y) && scrollOffset > 0) { scrollOffset--; needsRedraw = true; }
                    if (downBtn.contains(tap.x, tap.y) && scrollOffset + VISIBLE_ITEMS < ssidCount) { scrollOffset++; needsRedraw = true; }
                }
            }
        }

        if (tapped && stage == Stage::EnterPassword) {
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
                Rect spaceBtn   = {SIDE_MARGIN, rowYFn, 150, ROW_H};
                Rect connectBtn = {SIDE_MARGIN + 154, rowYFn, Config::SCREEN_WIDTH - 2*SIDE_MARGIN - 154, ROW_H};
                Rect backBtn    = {SIDE_MARGIN, (int16_t)(rowYFn + ROW_H + ROW_GAP), (int16_t)(Config::SCREEN_WIDTH - 2*SIDE_MARGIN), 28};

                if (spaceBtn.contains(tap.x, tap.y)) {
                    if (passwordLen < sizeof(passwordBuf) - 1) {
                        passwordBuf[passwordLen++] = ' ';
                        passwordBuf[passwordLen] = 0;
                    }
                } else if (connectBtn.contains(tap.x, tap.y)) {
                    if (WifiMgr::networkCount() >= Config::MAX_WIFI_NETWORKS) {
                        tft.fillScreen(TFT_BLACK);
                        tft.setCursor(10, 14);
                        tft.setTextColor(TFT_RED, TFT_BLACK);
                        tft.println(I18n::t(StringId::WIFI_ALREADY_3));
                        tft.setTextColor(TFT_GREEN, TFT_BLACK);
                        tft.setCursor(10, 30);
                        tft.println(I18n::t(StringId::WIFI_REMOVE_ONE_FIRST));
                        delay(1600);
                        skipped = true;
                        break;
                    }
                    WifiMgr::connectTo(ssidList[selectedIndex].c_str(), passwordBuf);
                    stage = Stage::Connecting;
                    tft.fillScreen(TFT_BLACK);
                    tft.setCursor(10, 14);
                    tft.setTextColor(TFT_GREEN, TFT_BLACK);
                    tft.println(I18n::t(StringId::WIFI_CONNECTING));
                } else if (backBtn.contains(tap.x, tap.y)) {
                    stage = Stage::PickSsid;
                    needsRedraw = true;
                }
            }
            needsRedraw = true;
        }

        if (!needsRedraw) {
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
            drawCancelButton(tft);
        } else if (stage == Stage::EnterPassword) {
            tft.fillRect(0, 0, Config::SCREEN_WIDTH, KB_TOP - 4, TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.printf("%s%s", I18n::t(StringId::WIFI_LABEL_PREFIX), ssidList[selectedIndex].c_str());
            tft.setCursor(10, 22);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.println(I18n::t(StringId::WIFI_PASSWORD_LABEL));
            tft.fillRect(8, 38, Config::SCREEN_WIDTH - 16, 22, TFT_BLACK);
            tft.drawRect(8, 38, Config::SCREEN_WIDTH - 16, 22, TFT_GREEN);
            tft.setCursor(12, 56);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.print(passwordBuf);

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

            Rect spaceBtn   = {SIDE_MARGIN, rowYFn, 150, ROW_H};
            Rect connectBtn = {SIDE_MARGIN + 154, rowYFn, Config::SCREEN_WIDTH - 2*SIDE_MARGIN - 154, ROW_H};
            Rect backBtn    = {SIDE_MARGIN, (int16_t)(rowYFn + ROW_H + ROW_GAP), (int16_t)(Config::SCREEN_WIDTH - 2*SIDE_MARGIN), 28};
            drawButton(tft, spaceBtn, I18n::t(StringId::WIFI_SPACE));
            drawButton(tft, connectBtn, I18n::t(StringId::WIFI_CONNECT));
            drawButton(tft, backBtn, I18n::t(StringId::WIFI_BACK_TO_LIST_BTN));
        }

        delay(20);
    }

    return false;
}

}