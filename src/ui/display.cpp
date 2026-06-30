/*
 *  Copyright (C) since 2026 by PopolonY2k and Leidson Campos Alves Ferreira.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <Arduino.h>
#include "display.h"
#if defined(BOARD_STICKC_PLUS2)
#include <M5Unified.h>
#include "../resources/popolon_png.h"
static constexpr int LOGO_Y     = 115; ///< Top of the logo area (below HUD).
static constexpr int LOGO_SIZE  = 112; ///< Scale image to fit portrait lower area.
#endif

// Layout width: AtomS3 is a 128x128 square panel; M5StickC Plus2 is 240x135
// landscape (after rotation) — Y-positions below were tuned for 128px height
// but fit comfortably within 135px too, so only W needs to vary for now.
// First-pass layout for the StickC Plus2 — expect to need visual tuning once
// physical hardware is in hand (the existing AtomS3 layout is untouched).
#if defined(BOARD_STICKC_PLUS2)
static constexpr int W = 135;
#else
static constexpr int W = 128;
#endif

static constexpr int Y_STATUS     = 0;
static constexpr int STATUS_BAR_H = 12;
static constexpr int STATE_TEXT_X = 34;
static constexpr int BAT_TEXT_X   = 96;

static constexpr uint8_t DEFAULT_BRIGHTNESS = 128; ///< setBrightness() level on wake().
static constexpr uint8_t SLEEP_BRIGHTNESS   =   0;  ///< setBrightness() level on sleep().

// Battery percentage thresholds for colour-coding (levels are 0/25/50/75/100, see BoardHal).
static constexpr int BATTERY_GOOD_PCT = 75; ///< >= this → green, on the flight HUD.
static constexpr int BATTERY_LOW_PCT  = 25; ///< >= this → yellow, below → red, on the flight HUD.
static constexpr int BT_BATTERY_GOOD_PCT = 50; ///< >= this → green, below → red, on the BT status screen.

// Left column — vertical throttle bar
static constexpr int THR_LBL_X = 1;
static constexpr int THR_LBL_Y = 14;
static constexpr int THR_BAR_X = 1;
static constexpr int THR_BAR_Y = 25;
static constexpr int THR_BAR_W = 18;
static constexpr int THR_BAR_H = 96;

// Right column — horizontal bars
static constexpr int RHS_X     = 24;
static constexpr int RHS_BW    = W - RHS_X;  // 104px wide
static constexpr int BAR_H     = 8;
static constexpr int BAR_LBL_W = 24; ///< Width reserved for the ROL/YAW/PCH label text, left of each bar.
static constexpr int ROL_LBL_Y = 30;
static constexpr int ROL_BAR_Y = 41;
static constexpr int YAW_LBL_Y = 57;
static constexpr int YAW_BAR_Y = 68;
static constexpr int PCH_LBL_Y = 84;
static constexpr int PCH_BAR_Y = 95;

static uint16_t stateColor(FlightState s) {
    switch (s) {
        case FlightState::Idle:        return Rgb565::DarkGrey;
        case FlightState::Calibrating: return Rgb565::White;
        case FlightState::Arming:      return Rgb565::Yellow;
        case FlightState::Flying:      return Rgb565::Green;
        case FlightState::Landing:     return Rgb565::Orange;
        case FlightState::Emergency:   return Rgb565::Red;
        default:                       return Rgb565::White;
    }
}

Display::Display(const DisplayHal& hal) : _hal(hal) {}

void Display::label(int x, int y, const char* txt, uint16_t color) {
    _hal.setTextColor(color, Rgb565::Black);
    _hal.drawString(txt, x, y);
}

// Horizontal bar: fills left to right.
void Display::drawBar(int x, int y, int w, int h, uint8_t value, uint16_t color) {
    _hal.drawRect(x, y, w, h, Rgb565::DarkGrey);
    int fill = (int)value * (w - 2) / DroneAxis::MAX;
    _hal.fillRect(x + 1, y + 1, w - 2, h - 2, Rgb565::Black);
    _hal.fillRect(x + 1, y + 1, fill,   h - 2, color);
}

// Vertical bar: fills bottom to top (value 0 = empty, 254 = full).
void Display::drawBarV(int x, int y, int w, int h, uint8_t value, uint16_t color) {
    _hal.drawRect(x, y, w, h, Rgb565::DarkGrey);
    _hal.fillRect(x + 1, y + 1, w - 2, h - 2, Rgb565::Black);
    int fill = (int)value * (h - 2) / DroneAxis::MAX;
    if (fill > 0) {
        _hal.fillRect(x + 1, y + 1 + (h - 2) - fill, w - 2, fill, color);
    }
}

void Display::begin() {
    _hal.begin();
    _hal.fillScreen(Rgb565::Black);
    _needsFullRedraw  = true;
    _modeSelectReady  = false;
    _btScreenReady    = false;
}

void Display::markDirty() {
    _needsFullRedraw = true;
    _btScreenReady   = false;
}

void Display::sleep() {
    _hal.setBrightness(SLEEP_BRIGHTNESS);
}

void Display::wake() {
    _hal.setBrightness(DEFAULT_BRIGHTNESS);
    markDirty();
}

void Display::update(bool wifiConnected, FlightState flightState,
                     const DroneState& drone, const ImuData& imu,
                     int batteryLevel, bool charging) {
    if (_needsFullRedraw) {
        _hal.fillScreen(Rgb565::Black);
        label(THR_LBL_X, THR_LBL_Y, "THR");
        label(RHS_X, ROL_LBL_Y, "ROL");
        label(RHS_X, YAW_LBL_Y, "YAW");
        label(RHS_X, PCH_LBL_Y, "PCH", Rgb565::DarkGrey);
#if defined(BOARD_STICKC_PLUS2)
        M5.Display.drawPng(POPOLON_PNG, POPOLON_PNG_SIZE,
                           (W - LOGO_SIZE) / 2, LOGO_Y,
                           LOGO_SIZE, LOGO_SIZE,
                           0, 0,
                           (float)LOGO_SIZE / POPOLON_PNG_W,
                           (float)LOGO_SIZE / POPOLON_PNG_H);
#endif
        _needsFullRedraw = false;
    }

    drawStatusBar(wifiConnected, flightState, batteryLevel, charging);
    drawControlBars(drone);
}

void Display::drawStatusBar(bool wifiConnected, FlightState flightState,
                             int batteryLevel, bool charging) {
    _hal.fillRect(0, Y_STATUS, W, STATUS_BAR_H, Rgb565::Black);

    _hal.setTextColor(wifiConnected ? Rgb565::Green : Rgb565::Red, Rgb565::Black);
    _hal.drawString(wifiConnected ? "WiFi" : "----", 0, Y_STATUS);

    _hal.setTextColor(stateColor(flightState), Rgb565::Black);
    _hal.drawString(flightStateName(flightState), STATE_TEXT_X, Y_STATUS);

    char bat[5];
    uint16_t batColor;
    if (batteryLevel < 0) {
        if (charging) {
            strcpy(bat, "CHG");
            batColor = Rgb565::Yellow;
        } else {
            strcpy(bat, "---");
            batColor = Rgb565::DarkGrey;
        }
    } else {
        snprintf(bat, sizeof(bat), "%d%%", batteryLevel);
        batColor = charging                       ? Rgb565::Yellow :
                   batteryLevel >= BATTERY_GOOD_PCT ? Rgb565::Green  :
                   batteryLevel >= BATTERY_LOW_PCT  ? Rgb565::Yellow : Rgb565::Red;
    }
    _hal.setTextColor(batColor, Rgb565::Black);
    _hal.drawString(bat, BAT_TEXT_X, Y_STATUS);
}

void Display::drawControlBars(const DroneState& drone) {
    uint16_t axisColor = drone.active ? Rgb565::Cyan : Rgb565::DarkGrey;
    int barX = RHS_X + BAR_LBL_W;  // offset past the label text
    int barW = W - barX;

    drawBarV(THR_BAR_X, THR_BAR_Y, THR_BAR_W, THR_BAR_H, drone.throttle, Rgb565::Orange);
    drawBar(barX, ROL_BAR_Y, barW, BAR_H, drone.roll,     axisColor);
    drawBar(barX, YAW_BAR_Y, barW, BAR_H, drone.yaw,      axisColor);
    drawBar(barX, PCH_BAR_Y, barW, BAR_H, drone.pitch,    Rgb565::DarkGrey);
}

void Display::drawImu(const ImuData& imu) {
    (void)imu;  // reserved for future use
}

void Display::drawBtStatus(BleStatus status, bool wifiOk, int batteryLevel, bool charging) {
#if defined(BOARD_STICKC_PLUS2)
    static constexpr int W = 135;
#else
    static constexpr int W = 128;
#endif

    static constexpr int TITLE_X = 4,  TITLE_Y = 8;

    static constexpr int STATUS_BG_Y = 28, STATUS_BG_H = 16, STATUS_TEXT_Y = 30;
    static constexpr int CONNECTED_TEXT_X  = 22;
    static constexpr int CONNECTING_TEXT_X = 10;
    static constexpr int SCANNING_TEXT_X   = 10;

    static constexpr int HELP_BG_Y = 52, HELP_BG_H = 14, HELP_TEXT_Y = 54;
    static constexpr int HELP_CONNECTED_TEXT_X = 2;
    static constexpr int HELP_PAIRING_TEXT_X   = 4;

    static constexpr int WIFI_BG_Y = 78, WIFI_BG_H = 12, WIFI_TEXT_Y = 80;

    static constexpr int BAT_TEXT_X = 90;

    static constexpr int BAR_BG_Y      = 100, BAR_BG_H      =  16;
    static constexpr int BAR_BORDER_Y  = 102, BAR_BORDER_H  =  12, BAR_BORDER_X = 1;
    static constexpr int BAR_FILL_Y    = 103, BAR_FILL_H    =  10, BAR_FILL_X   = 2;
    static constexpr int PULSE_BLOCK_W = 24;  ///< Width of the moving ping-pong block.

    static constexpr uint32_t SCAN_DOT_PERIOD_MS = 400; ///< Animated "SCANNING..." dot cycle.
    static constexpr uint32_t PULSE_FRAME_MS     =  12; ///< Ping-pong pulse animation frame period.
    static constexpr uint32_t PULSE_CYCLE        = 200; ///< Full ping-pong cycle length (0→100→0).
    static constexpr int      PULSE_HALF_CYCLE   = 100; ///< Midpoint of the ping-pong cycle.

    if (!_btScreenReady) {
        _hal.fillScreen(Rgb565::Black);
        _hal.setTextColor(Rgb565::White, Rgb565::Black);
        _hal.drawString("== BT GAMEPAD ==", TITLE_X, TITLE_Y);
#if defined(BOARD_STICKC_PLUS2)
        M5.Display.drawPng(POPOLON_PNG, POPOLON_PNG_SIZE,
                           (W - LOGO_SIZE) / 2, LOGO_Y,
                           LOGO_SIZE, LOGO_SIZE,
                           0, 0,
                           (float)LOGO_SIZE / POPOLON_PNG_W,
                           (float)LOGO_SIZE / POPOLON_PNG_H);
#endif
        _btScreenReady = true;
    }

    // Status text
    _hal.fillRect(0, STATUS_BG_Y, W, STATUS_BG_H, Rgb565::Black);
    if (status == BleStatus::Connected) {
        _hal.setTextColor(Rgb565::Green, Rgb565::Black);
        _hal.drawString("CONNECTED!", CONNECTED_TEXT_X, STATUS_TEXT_Y);
    } else if (status == BleStatus::Connecting) {
        _hal.setTextColor(Rgb565::Yellow, Rgb565::Black);
        _hal.drawString("CONNECTING...", CONNECTING_TEXT_X, STATUS_TEXT_Y);
    } else {
        // Animated SCANNING dots
        const char* dots[] = { "", ".", "..", "..." };
        char buf[16];
        snprintf(buf, sizeof(buf), "SCANNING%s", dots[(millis() / SCAN_DOT_PERIOD_MS) % 4]);
        _hal.setTextColor(Rgb565::Cyan, Rgb565::Black);
        _hal.drawString(buf, SCANNING_TEXT_X, STATUS_TEXT_Y);
    }

    // Help text
    _hal.fillRect(0, HELP_BG_Y, W, HELP_BG_H, Rgb565::Black);
    _hal.setTextColor(Rgb565::DarkGrey, Rgb565::Black);
    if (status == BleStatus::Connected) {
        _hal.drawString("Dbl-click: arm+fly", HELP_CONNECTED_TEXT_X, HELP_TEXT_Y);
    } else {
        _hal.drawString("8BitDo: X + Start", HELP_PAIRING_TEXT_X, HELP_TEXT_Y);
    }

    // WiFi status
    _hal.fillRect(0, WIFI_BG_Y, W, WIFI_BG_H, Rgb565::Black);
    _hal.setTextColor(wifiOk ? Rgb565::Green : Rgb565::Red, Rgb565::Black);
    _hal.drawString(wifiOk ? "WiFi OK" : "No WiFi", 0, WIFI_TEXT_Y);

    // Battery
    char bat[6];
    uint16_t batColor;
    if (batteryLevel < 0) {
        strcpy(bat, charging ? "CHG" : "---");
        batColor = charging ? Rgb565::Yellow : Rgb565::DarkGrey;
    } else {
        snprintf(bat, sizeof(bat), "%d%%", batteryLevel);
        batColor = charging                          ? Rgb565::Yellow :
                   batteryLevel >= BT_BATTERY_GOOD_PCT ? Rgb565::Green  : Rgb565::Red;
    }
    _hal.setTextColor(batColor, Rgb565::Black);
    _hal.drawString(bat, BAT_TEXT_X, WIFI_TEXT_Y);

    // Animated bar at bottom
    _hal.fillRect(0, BAR_BG_Y, W, BAR_BG_H, Rgb565::Black);
    _hal.drawRect(BAR_BORDER_X, BAR_BORDER_Y, W - 2, BAR_BORDER_H, Rgb565::DarkGrey);
    if (status == BleStatus::Connected) {
        _hal.fillRect(BAR_FILL_X, BAR_FILL_Y, W - 4, BAR_FILL_H, Rgb565::Green);
    } else {
        // Ping-pong pulse
        uint32_t t   = (millis() / PULSE_FRAME_MS) % PULSE_CYCLE;
        int      pos = (t <= (uint32_t)PULSE_HALF_CYCLE) ? (int)t : (int)(PULSE_CYCLE - t);  // 0→100→0
        int      x   = BAR_FILL_X + pos * (W - 28) / PULSE_HALF_CYCLE;
        _hal.fillRect(x, BAR_FILL_Y, PULSE_BLOCK_W, BAR_FILL_H,
                      status == BleStatus::Connecting ? Rgb565::Yellow : Rgb565::Cyan);
    }
}

void Display::drawModeSelect(OperationMode selected, int secondsLeft) {
#if defined(BOARD_STICKC_PLUS2)
    static constexpr int W           = 135;
#else
    static constexpr int W           = 128;
#endif
    static constexpr int OPT_H       = 18;
    static constexpr int Y_TITLE     =   5;
    static constexpr int Y_OPT1      =  28;
    static constexpr int Y_OPT2      =  52;
    static constexpr int Y_AUTO      =  88;
    static constexpr int Y_BAR       = 108;
    static constexpr int BAR_H       =  10;
    static constexpr int TEXT_X      =   4;  ///< Left margin for title/option text.
    static constexpr int OPT_TEXT_DY =   4;  ///< Option text vertical offset within its row.
    static constexpr int AUTO_TEXT_X     =  20;
    static constexpr int BAR_BORDER_INSET =  1; ///< Border-to-bar-edge padding on each side.
    static constexpr int BAR_FILL_INSET   =  2; ///< Border-to-fill padding on each side.
    /// Countdown duration matching main.cpp's MODE_SELECT_MS (3000 ms = 3 s).
    static constexpr int MAX_COUNTDOWN_S = 3;

    if (!_modeSelectReady) {
        _hal.fillScreen(Rgb565::Black);
        _hal.setTextColor(Rgb565::White, Rgb565::Black);
        _hal.drawString("-- SELECT MODE --", TEXT_X, Y_TITLE);
        _modeSelectReady  = true;
        _needsFullRedraw  = true;  // ensure flight HUD redraws when mode starts
    }

    // Option 1: BT GAMEPAD
    bool btSelected = (selected == OperationMode::BluetoothControl);
    _hal.fillRect(0, Y_OPT1, W, OPT_H, btSelected ? Rgb565::Navy : Rgb565::Black);
    _hal.setTextColor(btSelected ? Rgb565::White : Rgb565::DarkGrey,
                      btSelected ? Rgb565::Navy  : Rgb565::Black);
    _hal.drawString(btSelected ? "> BT GAMEPAD" : "  BT GAMEPAD", TEXT_X, Y_OPT1 + OPT_TEXT_DY);

    // Option 2: ACCEL TILT
    bool acSelected = (selected == OperationMode::AccelControl);
    _hal.fillRect(0, Y_OPT2, W, OPT_H, acSelected ? Rgb565::Navy : Rgb565::Black);
    _hal.setTextColor(acSelected ? Rgb565::White : Rgb565::DarkGrey,
                      acSelected ? Rgb565::Navy  : Rgb565::Black);
    _hal.drawString(acSelected ? "> ACCEL TILT" : "  ACCEL TILT", TEXT_X, Y_OPT2 + OPT_TEXT_DY);

    // Countdown text
    char buf[16];
    snprintf(buf, sizeof(buf), "Auto in: %ds", secondsLeft);
    _hal.fillRect(0, Y_AUTO, W, STATUS_BAR_H, Rgb565::Black);
    _hal.setTextColor(Rgb565::Yellow, Rgb565::Black);
    _hal.drawString(buf, AUTO_TEXT_X, Y_AUTO);

    // Countdown bar — depletes right to left
    _hal.fillRect(0, Y_BAR, W, BAR_H, Rgb565::Black);
    _hal.drawRect(BAR_BORDER_INSET, Y_BAR, W - 2 * BAR_BORDER_INSET, BAR_H, Rgb565::DarkGrey);
    int fill = secondsLeft * (W - 2 * BAR_FILL_INSET) / MAX_COUNTDOWN_S;
    if (fill > 0) {
        _hal.fillRect(BAR_FILL_INSET, Y_BAR + 1, fill, BAR_H - 2, Rgb565::Yellow);
    }
}
