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
    _needsFullRedraw = true;
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
