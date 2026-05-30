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
#pragma once
#include <Arduino.h>
#include "../hal/hal.h"
#include "../comm/drone_protocol.h"
#include "../imu/accelerometer.h"
#include "../control/flight_controller.h"
#include "../control/operation_mode.h"
#include "../bt/ble_gamepad.h"

/**
 * @brief Pure renderer for the 128×128 AtomS3 LCD.
 *
 * Receives pre-computed values and draws them — it never reads from
 * hardware sources directly. Three distinct screens are supported:
 *
 *   1. **Mode selection** (drawModeSelect) — boot-time menu.
 *   2. **BT status**      (drawBtStatus)   — gamepad scan/connect screen.
 *   3. **Flight HUD**     (update)         — live control bars and status.
 *
 * Partial redraw is used where possible; markDirty() forces a full
 * screen clear on the next update() call (used when switching screens).
 */
class Display {
public:
    /** @param hal  Display hardware abstraction (injected, not owned). */
    explicit Display(const DisplayHal& hal);

    /** @brief Initialise the display and clear to black. Call once in setup(). */
    void begin();

    /** @brief Force a full screen clear and label redraw on the next update() call. */
    void markDirty();

    /** @brief Turn off the backlight (screen goes dark; state is preserved). */
    void sleep();

    /** @brief Restore backlight and force a full redraw on the next update() call. */
    void wake();

    /**
     * @brief Render the flight HUD (throttle bar, roll/yaw/pitch bars, status bar).
     *
     * Performs a full redraw on the first call after markDirty() or begin(),
     * then redraws only the dynamic regions on subsequent calls.
     *
     * @param wifiConnected  True when drone WiFi AP is associated.
     * @param flightState    Current flight phase (displayed as text in status bar).
     * @param drone          Latest drone control state (axis bar values).
     * @param imu            Latest IMU sample (reserved — not yet rendered).
     * @param batteryLevel   IP5306 level: 25/50/75/100 %, or -1 if unavailable.
     * @param charging       True when VBUS charging is active.
     */
    void update(bool wifiConnected, FlightState flightState,
                const DroneState& drone, const ImuData& imu,
                int batteryLevel, bool charging);

    /**
     * @brief Render the boot-time mode selection menu.
     *
     * Draws a static title on the first call; redraws only the dynamic
     * option highlight and countdown on subsequent calls.
     *
     * @param selected    Currently highlighted mode.
     * @param secondsLeft Remaining countdown seconds (0–3).
     */
    void drawModeSelect(OperationMode selected, int secondsLeft);

    /**
     * @brief Render the BLE gamepad status screen.
     *
     * Draws a static title on the first call; redraws only dynamic regions
     * (status text, help text, WiFi/battery, animated bar) on subsequent calls.
     *
     * @param status       Current BLE connection state.
     * @param wifiOk       True when drone WiFi AP is associated.
     * @param batteryLevel IP5306 level: 25/50/75/100 %, or -1 if unavailable.
     * @param charging     True when VBUS charging is active.
     */
    void drawBtStatus(BleStatus status, bool wifiOk, int batteryLevel, bool charging);

private:
    /**
     * @brief Render the top status bar (WiFi indicator, flight state, battery).
     * @param wifiConnected  True for green "WiFi", false for red "----".
     * @param flightState    State name drawn in the state-colour.
     * @param batteryLevel   Percentage or -1.
     * @param charging       Colours the battery indicator yellow when true.
     */
    void drawStatusBar(bool wifiConnected, FlightState flightState,
                       int batteryLevel, bool charging);

    /**
     * @brief Render the throttle and axis bars using current DroneState values.
     * @param drone  Axis values to display; active flag controls bar colour.
     */
    void drawControlBars(const DroneState& drone);

    /** @brief Reserved for future IMU visualisation. */
    void drawImu(const ImuData& imu);

    /**
     * @brief Draw a short text label.
     * @param x, y   Top-left pixel position.
     * @param txt    Null-terminated string.
     * @param color  Text colour (default DarkGrey).
     */
    void label(int x, int y, const char* txt, uint16_t color = Rgb565::DarkGrey);

    /**
     * @brief Draw a horizontal fill bar with a border.
     * @param x, y   Top-left of the bounding box.
     * @param w, h   Width and height in pixels.
     * @param value  Fill level [0, 254], maps linearly to [0, w-2].
     * @param color  Fill colour.
     */
    void drawBar(int x, int y, int w, int h, uint8_t value, uint16_t color);

    /**
     * @brief Draw a vertical fill bar (fills bottom-to-top) with a border.
     * @param x, y   Top-left of the bounding box.
     * @param w, h   Width and height in pixels.
     * @param value  Fill level [0, 254]; 0 = empty, 254 = full.
     * @param color  Fill colour.
     */
    void drawBarV(int x, int y, int w, int h, uint8_t value, uint16_t color);

    const DisplayHal& _hal;
    bool _needsFullRedraw = true;  ///< Cleared after the first full draw; set again by markDirty().
    bool _modeSelectReady = false; ///< True after the mode-select static title is drawn.
    bool _btScreenReady   = false; ///< True after the BT status static title is drawn.
};
