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
/**
 * @file main.cpp
 * @brief Application entry point for the maritaca-e88-controller firmware.
 *
 * Wires all subsystems together and drives the main loop:
 *   - Boot: mode-selection screen (3 s countdown; BT GAMEPAD default).
 *   - Setup: WiFi, drone protocol, IMU, flight controller, optional BLE gamepad.
 *   - Loop (non-blocking, millis()-driven):
 *       - Board button polling via kBoard.update()
 *       - WiFi reconnection and app-mode activation on connect
 *       - BLE gamepad update (BT mode only)
 *       - Flight state machine tick
 *       - Drone UDP packet transmission
 *       - Display routing: BT status screen or flight HUD at 10 Hz
 */
#include <Arduino.h>
#include "hal/m5atoms3.h"
#include "control/operation_mode.h"
#include "control/flight_controller.h"
#include "comm/wifi_manager.h"
#include "comm/drone_protocol.h"
#include "comm/flow_wifi_protocol.h"
#include "imu/accelerometer.h"
#include "ui/display.h"
#include "bt/ble_gamepad.h"

// Known drone SSIDs — first match found during scan wins.
static const char* const DRONE_SSIDS[] = {
    "WIFI_8K_Wf48702",   // index 0 — black drone (WIFI_8K_ variant)
    "FLOW-WIFI-304BA",   // index 1 — grey drone  (FLOW-WIFI variant)
};
static constexpr int N_DRONES = 2;

static OperationModeManager modeManager;
static WifiManager          wifi;
static DroneProtocol        blackDrone;
static FlowWifiProtocol     greyDrone;
static DroneProtocolBase*   activeDrone = &blackDrone;  // resolved in setup()
static Accelerometer        imu;
static BleGamepad           gamepad;
static Display              display(kDisplay);

// FlightController is constructed in setup() after drone auto-detection.
static FlightController* flight = nullptr;

static uint32_t _lastDisplayMs = 0;
static constexpr uint32_t DISPLAY_INTERVAL_MS = 100;  // 10 Hz
static bool _prevWifiConnected = false;

static uint32_t _btConnectedMs    = 0;
static bool     _btWasConnected   = false;
static bool     _prevShowBtScreen = false;

static bool     _screenOff        = false;
static uint16_t _prevGpBtns       = 0;     // rising-edge tracking for main.cpp button events

static constexpr uint32_t MODE_SELECT_MS = 3000; ///< Boot menu auto-select timeout (ms).

/**
 * @brief Block in the boot mode-selection menu until the countdown expires.
 *
 * Renders the menu at 10 Hz. A button click cycles the selection and resets
 * the countdown. Sets the chosen mode on modeManager before returning.
 */
static void runModeSelection() {
    OperationMode sel       = OperationMode::BluetoothControl;  // default
    uint32_t      selectStart = millis();
    uint32_t      lastDrawMs  = 0;

    while (true) {
        kBoard.update();

        if (kButton.wasReleased()) {
            sel = (sel == OperationMode::BluetoothControl)
                  ? OperationMode::AccelControl
                  : OperationMode::BluetoothControl;
            selectStart = millis();  // reset timer on selection change
        }

        uint32_t elapsed  = millis() - selectStart;
        int      secsLeft = (int)((MODE_SELECT_MS - elapsed) / 1000) + 1;
        if (secsLeft > 3) secsLeft = 3;
        if (secsLeft < 0) secsLeft = 0;

        uint32_t now = millis();
        if (now - lastDrawMs >= 100) {
            lastDrawMs = now;
            display.drawModeSelect(sel, secsLeft);
        }

        if (elapsed >= MODE_SELECT_MS) break;
    }

    modeManager.setMode(sel);
    Serial.printf("[Boot] Mode: %s\n",
                  sel == OperationMode::BluetoothControl ? "BT GAMEPAD" : "ACCEL TILT");
}

void setup() {
    kBoard.begin();

    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    Serial.println("[Boot] setup start");

    display.begin();

    // Scan for known drones before the mode-select screen so the blocking
    // scan doesn't freeze the countdown UI.
    int droneIdx = WifiManager::scanForFirst(DRONE_SSIDS, N_DRONES);
    if (droneIdx < 0) droneIdx = 0;  // default to black drone if none visible

    runModeSelection();

    if (droneIdx == 1) {
        activeDrone = &greyDrone;
        Serial.println("[Boot] Drone: FLOW-WIFI grey");
    } else {
        activeDrone = &blackDrone;
        Serial.println("[Boot] Drone: WIFI_8K_ black");
    }

    wifi.begin(DRONE_SSIDS[droneIdx]);
    activeDrone->begin();
    imu.begin();

    // Construct FlightController with the detected drone.
    static FlightController fc({ kButton, *activeDrone });
    flight = &fc;
    flight->setMode(modeManager.current());
    flight->begin();

    if (modeManager.current() == OperationMode::BluetoothControl) {
        gamepad.begin();
    }
}

void loop() {
    kBoard.update();
    wifi.update();
    imu.update(kImu);

    bool wifiNow = wifi.isConnected();
    if (wifiNow && !_prevWifiConnected) {
        Serial.println("[Main] WiFi connected — starting app mode entry");
        activeDrone->beginAppModeEntry();
    }
    _prevWifiConnected = wifiNow;

    GamepadAxes gpAxes{};
    if (modeManager.current() == OperationMode::BluetoothControl) {
        gamepad.update();
        gpAxes = gamepad.axes();
    }

    flight->update(imu.data(), gpAxes, wifi.isConnected());

    if (wifi.isConnected()) {
        activeDrone->update();
    }

    bool gpConnected = gamepad.axes().connected;
    if (gpConnected && !_btWasConnected) _btConnectedMs = millis();
    _btWasConnected = gpConnected;

    bool showBtScreen = (modeManager.current() == OperationMode::BluetoothControl)
                        && (flight->state() == FlightState::Idle)
                        && (!gpConnected || !wifi.isConnected() || (millis() - _btConnectedMs < 1500));

    // D-pad LEFT: toggle screen on/off — only available on the flight HUD (not BT status screen)
    uint16_t gpBtns   = gpAxes.buttons;
    uint16_t gpPressed = gpBtns & ~_prevGpBtns;
    _prevGpBtns        = gpBtns;
    if ((gpPressed & GamepadBtn::DpadLeft) && !showBtScreen) {
        _screenOff = !_screenOff;
        if (_screenOff) display.sleep();
        else            display.wake();
    }

    if (_prevShowBtScreen && !showBtScreen) {
        display.markDirty();
        _screenOff = true;
        display.sleep();
    }
    if (!_prevShowBtScreen && showBtScreen && _screenOff) {
        _screenOff = false;
        display.wake();
    }
    _prevShowBtScreen = showBtScreen;

    uint32_t now = millis();
    if (_screenOff) return;  // backlight off — skip all display work
    if (now - _lastDisplayMs >= DISPLAY_INTERVAL_MS) {
        _lastDisplayMs = now;
        if (showBtScreen) {
            display.drawBtStatus(gamepad.status(),
                                 wifi.isConnected(),
                                 kBoard.getBatteryLevel(),
                                 kBoard.isCharging());
        } else {
            // In Idle+BT mode preview gamepad axes in the HUD bars without
            // touching the drone protocol (no UDP sent).
            DroneState displayState = activeDrone->state();
            if (modeManager.current() == OperationMode::BluetoothControl
                && flight->state() == FlightState::Idle
                && gpAxes.connected) {
                auto tobyte = [](float v) -> uint8_t {
                    int i = 0x80 + (int)(v * 127.0f);
                    return (uint8_t)(i < 1 ? 1 : i > 254 ? 254 : i);
                };
                displayState.roll     = tobyte( gpAxes.roll);
                displayState.pitch    = tobyte(-gpAxes.pitch);
                displayState.yaw      = tobyte( gpAxes.yaw);
                displayState.throttle = 0x80;  // no accumulator in Idle; keep neutral
                displayState.active   = true;
            }
            display.update(wifi.isConnected(), flight->state(),
                           displayState, imu.data(),
                           kBoard.getBatteryLevel(), kBoard.isCharging());
        }
    }
}
