#include <Arduino.h>
#include "display.h"

// Layout for 128x128 display
static constexpr int W = 128;

static constexpr int Y_STATUS = 0;

// Left column — vertical throttle bar
static constexpr int THR_LBL_X = 1;
static constexpr int THR_LBL_Y = 14;
static constexpr int THR_BAR_X = 1;
static constexpr int THR_BAR_Y = 25;
static constexpr int THR_BAR_W = 18;
static constexpr int THR_BAR_H = 96;

// Right column — horizontal bars
static constexpr int RHS_X    = 24;
static constexpr int RHS_BW   = W - RHS_X;  // 104px wide
static constexpr int BAR_H    = 8;
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
    int fill = (int)value * (w - 2) / 254;
    _hal.fillRect(x + 1, y + 1, w - 2, h - 2, Rgb565::Black);
    _hal.fillRect(x + 1, y + 1, fill,   h - 2, color);
}

// Vertical bar: fills bottom to top (value 0 = empty, 254 = full).
void Display::drawBarV(int x, int y, int w, int h, uint8_t value, uint16_t color) {
    _hal.drawRect(x, y, w, h, Rgb565::DarkGrey);
    _hal.fillRect(x + 1, y + 1, w - 2, h - 2, Rgb565::Black);
    int fill = (int)value * (h - 2) / 254;
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

void Display::update(bool wifiConnected, FlightState flightState,
                     const DroneState& drone, const ImuData& imu,
                     int batteryLevel, bool charging) {
    if (_needsFullRedraw) {
        _hal.fillScreen(Rgb565::Black);
        label(THR_LBL_X, THR_LBL_Y, "THR");
        label(RHS_X, ROL_LBL_Y, "ROL");
        label(RHS_X, YAW_LBL_Y, "YAW");
        label(RHS_X, PCH_LBL_Y, "PCH", Rgb565::DarkGrey);
        _needsFullRedraw = false;
    }

    drawStatusBar(wifiConnected, flightState, batteryLevel, charging);
    drawControlBars(drone);
}

void Display::drawStatusBar(bool wifiConnected, FlightState flightState,
                             int batteryLevel, bool charging) {
    _hal.fillRect(0, Y_STATUS, W, 12, Rgb565::Black);

    _hal.setTextColor(wifiConnected ? Rgb565::Green : Rgb565::Red, Rgb565::Black);
    _hal.drawString(wifiConnected ? "WiFi" : "----", 0, Y_STATUS);

    _hal.setTextColor(stateColor(flightState), Rgb565::Black);
    _hal.drawString(flightStateName(flightState), 34, Y_STATUS);

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
        batColor = charging          ? Rgb565::Yellow :
                   batteryLevel >= 75 ? Rgb565::Green  :
                   batteryLevel >= 25 ? Rgb565::Yellow : Rgb565::Red;
    }
    _hal.setTextColor(batColor, Rgb565::Black);
    _hal.drawString(bat, 96, Y_STATUS);
}

void Display::drawControlBars(const DroneState& drone) {
    uint16_t axisColor = drone.active ? Rgb565::Cyan : Rgb565::DarkGrey;
    int barX = RHS_X + 24;  // offset past the label text
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
    static constexpr int W = 128;

    if (!_btScreenReady) {
        _hal.fillScreen(Rgb565::Black);
        _hal.setTextColor(Rgb565::White, Rgb565::Black);
        _hal.drawString("== BT GAMEPAD ==", 4, 8);
        _btScreenReady = true;
    }

    // Status text
    _hal.fillRect(0, 28, W, 16, Rgb565::Black);
    if (status == BleStatus::Connected) {
        _hal.setTextColor(Rgb565::Green, Rgb565::Black);
        _hal.drawString("CONNECTED!", 22, 30);
    } else if (status == BleStatus::Connecting) {
        _hal.setTextColor(Rgb565::Yellow, Rgb565::Black);
        _hal.drawString("CONNECTING...", 10, 30);
    } else {
        // Animated SCANNING dots
        const char* dots[] = { "", ".", "..", "..." };
        char buf[16];
        snprintf(buf, sizeof(buf), "SCANNING%s", dots[(millis() / 400) % 4]);
        _hal.setTextColor(Rgb565::Cyan, Rgb565::Black);
        _hal.drawString(buf, 10, 30);
    }

    // Help text
    _hal.fillRect(0, 52, W, 14, Rgb565::Black);
    _hal.setTextColor(Rgb565::DarkGrey, Rgb565::Black);
    if (status == BleStatus::Connected) {
        _hal.drawString("Dbl-click: arm+fly", 2, 54);
    } else {
        _hal.drawString("8BitDo: X + Start", 4, 54);
    }

    // WiFi status
    _hal.fillRect(0, 78, W, 12, Rgb565::Black);
    _hal.setTextColor(wifiOk ? Rgb565::Green : Rgb565::Red, Rgb565::Black);
    _hal.drawString(wifiOk ? "WiFi OK" : "No WiFi", 0, 80);

    // Battery
    char bat[6];
    uint16_t batColor;
    if (batteryLevel < 0) {
        strcpy(bat, charging ? "CHG" : "---");
        batColor = charging ? Rgb565::Yellow : Rgb565::DarkGrey;
    } else {
        snprintf(bat, sizeof(bat), "%d%%", batteryLevel);
        batColor = charging          ? Rgb565::Yellow :
                   batteryLevel >= 50 ? Rgb565::Green  : Rgb565::Red;
    }
    _hal.setTextColor(batColor, Rgb565::Black);
    _hal.drawString(bat, 90, 80);

    // Animated bar at bottom
    _hal.fillRect(0, 100, W, 16, Rgb565::Black);
    _hal.drawRect(1, 102, W - 2, 12, Rgb565::DarkGrey);
    if (status == BleStatus::Connected) {
        _hal.fillRect(2, 103, W - 4, 10, Rgb565::Green);
    } else {
        // Ping-pong pulse
        uint32_t t   = (millis() / 12) % 200;
        int      pos = (t <= 100) ? (int)t : (int)(200 - t);  // 0→100→0
        int      x   = 2 + pos * (W - 28) / 100;
        _hal.fillRect(x, 103, 24, 10,
                      status == BleStatus::Connecting ? Rgb565::Yellow : Rgb565::Cyan);
    }
}

void Display::drawModeSelect(OperationMode selected, int secondsLeft) {
    static constexpr int W         = 128;
    static constexpr int OPT_H     = 18;
    static constexpr int Y_TITLE   =   5;
    static constexpr int Y_OPT1    =  28;
    static constexpr int Y_OPT2    =  52;
    static constexpr int Y_AUTO    =  88;
    static constexpr int Y_BAR     = 108;
    static constexpr int BAR_H     =  10;

    if (!_modeSelectReady) {
        _hal.fillScreen(Rgb565::Black);
        _hal.setTextColor(Rgb565::White, Rgb565::Black);
        _hal.drawString("-- SELECT MODE --", 4, Y_TITLE);
        _modeSelectReady  = true;
        _needsFullRedraw  = true;  // ensure flight HUD redraws when mode starts
    }

    // Option 1: BT GAMEPAD
    bool btSelected = (selected == OperationMode::BluetoothControl);
    _hal.fillRect(0, Y_OPT1, W, OPT_H, btSelected ? Rgb565::Navy : Rgb565::Black);
    _hal.setTextColor(btSelected ? Rgb565::White : Rgb565::DarkGrey,
                      btSelected ? Rgb565::Navy  : Rgb565::Black);
    _hal.drawString(btSelected ? "> BT GAMEPAD" : "  BT GAMEPAD", 4, Y_OPT1 + 4);

    // Option 2: ACCEL TILT
    bool acSelected = (selected == OperationMode::AccelControl);
    _hal.fillRect(0, Y_OPT2, W, OPT_H, acSelected ? Rgb565::Navy : Rgb565::Black);
    _hal.setTextColor(acSelected ? Rgb565::White : Rgb565::DarkGrey,
                      acSelected ? Rgb565::Navy  : Rgb565::Black);
    _hal.drawString(acSelected ? "> ACCEL TILT" : "  ACCEL TILT", 4, Y_OPT2 + 4);

    // Countdown text
    char buf[16];
    snprintf(buf, sizeof(buf), "Auto in: %ds", secondsLeft);
    _hal.fillRect(0, Y_AUTO, W, 12, Rgb565::Black);
    _hal.setTextColor(Rgb565::Yellow, Rgb565::Black);
    _hal.drawString(buf, 20, Y_AUTO);

    // Countdown bar — depletes right to left
    _hal.fillRect(0, Y_BAR, W, BAR_H, Rgb565::Black);
    _hal.drawRect(1, Y_BAR, W - 2, BAR_H, Rgb565::DarkGrey);
    int fill = secondsLeft * (W - 4) / 3;
    if (fill > 0) {
        _hal.fillRect(2, Y_BAR + 1, fill, BAR_H - 2, Rgb565::Yellow);
    }
}
