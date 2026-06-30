// SPDX-License-Identifier: Apache-2.0
// BT-gamepad-to-drone build for the M5StickC Plus2. Bluepad32 reads an 8BitDo
// (Switch mode, BR/EDR) controller, or the built-in IMU drives tilt control
// in AccelControl mode. Shared DroneProtocol/FlowWifiProtocol/FlightController/
// AccelController/Display logic reused directly from ../../src (see
// main/CMakeLists.txt). M5Unified/M5GFX vendored as ESP-IDF components supply
// display, battery, and IMU access.
//
// NOTE: Bluepad32's built-in USB console (CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE)
// replaces Arduino's Serial — use Console, not Serial, for log output here.

#include "sdkconfig.h"

#include <Arduino.h>
#include <Bluepad32.h>
#include <WiFi.h>
#include "bp32_gamepad.h"
#include "comm/wifi_manager.h"
#include "comm/drone_protocol.h"
#include "comm/flow_wifi_protocol.h"
#include "control/flight_controller.h"
#include "control/operation_mode.h"
#include "imu/accelerometer.h"
#include "ui/display.h"
#include "hal/m5stickcplus2.h"

// Known drone SSIDs — first match found during scan wins.
static const char* const DRONE_SSIDS[] = {
    "WIFI_8K_Wf48702",   // index 0 — black drone (WIFI_8K_ variant)
    "FLOW-WIFI-304BA",   // index 1 — grey drone  (FLOW-WIFI variant)
};
static constexpr int N_DRONES = 2;
static constexpr uint32_t STATUS_LOG_INTERVAL_MS = 500;
static constexpr uint32_t DISPLAY_INTERVAL_MS    = 100; // 10 Hz
static constexpr uint32_t BT_STATUS_SCREEN_MS    = 1500;
static constexpr uint8_t  AXIS_DISPLAY_MIN       =    1;
static constexpr float    AXIS_DISPLAY_HALF_RANGE = 127.0f;
static constexpr uint32_t MODE_SELECT_MS          = 3000; // 3 s countdown
static constexpr int      MODE_SELECT_MAX_S       = (int)(MODE_SELECT_MS / 1000);

// AccelControl throttle rates tuned for the M5StickC Plus2's button feel.
// The shared FlightDeps defaults (0.3f up / 0.10f down) were calibrated for
// the AtomS3 and felt too slow on this board — kept separate per user decision.
static constexpr float STICKC_THROTTLE_RATE_UP   = 1.0f;  ///< Climb rate (units/frame at 25 Hz).
static constexpr float STICKC_THROTTLE_RATE_DOWN = 0.5f;  ///< Descend rate (units/frame at 25 Hz).

static OperationModeManager modeManager;
static WifiManager      wifi;
static DroneProtocol    blackDrone;
static FlowWifiProtocol greyDrone;
static DroneProtocolBase* activeDrone = &blackDrone;
static Bp32Gamepad      gamepad;
static Accelerometer    imu;
static FlightController* flight = nullptr;
static Display           display(kDisplay);

static bool _prevWifiConnected = false;
static uint32_t _btConnectedMs    = 0;
static bool     _btWasConnected   = false;
static bool     _prevShowBtScreen = false;
static bool     _screenOff        = false;
static uint16_t _prevGpBtns       = 0;
static uint32_t _lastDisplayMs    = 0;

static BleStatus toBleStatus(Bp32Status s) {
    return s == Bp32Status::Connected ? BleStatus::Connected : BleStatus::Scanning;
}

// Block in the boot mode-selection menu until the countdown expires.
// Mirrors main.cpp's runModeSelection() exactly.
static void runModeSelection() {
    OperationMode sel       = OperationMode::BluetoothControl;
    uint32_t      selectStart = millis();
    uint32_t      lastDrawMs  = 0;

    while (true) {
        kBoard.update();

        if (kButton.wasReleased()) {
            sel = (sel == OperationMode::BluetoothControl)
                  ? OperationMode::AccelControl
                  : OperationMode::BluetoothControl;
            selectStart = millis();
        }

        uint32_t elapsed  = millis() - selectStart;
        int      secsLeft = (int)((MODE_SELECT_MS - elapsed) / 1000) + 1;
        if (secsLeft > MODE_SELECT_MAX_S) secsLeft = MODE_SELECT_MAX_S;
        if (secsLeft < 0) secsLeft = 0;

        uint32_t now = millis();
        if (now - lastDrawMs >= DISPLAY_INTERVAL_MS) {
            lastDrawMs = now;
            display.drawModeSelect(sel, secsLeft);
        }

        if (elapsed >= MODE_SELECT_MS) break;
    }

    modeManager.setMode(sel);
    Console.printf("[Boot] Mode: %s\n",
                   sel == OperationMode::BluetoothControl ? "BT GAMEPAD" : "ACCEL TILT");
}

void setup() {
    kBoard.begin();
    display.begin();

    Console.printf("Firmware: %s\n", BP32.firmwareVersion());

    int droneIdx = WifiManager::scanForFirst(DRONE_SSIDS, N_DRONES);
    if (droneIdx < 0) droneIdx = 0;

    runModeSelection();

    if (droneIdx == 1) {
        activeDrone = &greyDrone;
        Console.println("[Boot] Drone: FLOW-WIFI grey");
    } else {
        activeDrone = &blackDrone;
        Console.println("[Boot] Drone: WIFI_8K_ black");
    }

    wifi.setReconnectInterval(30000);
    wifi.begin(DRONE_SSIDS[droneIdx]);
    WiFi.setSleep(false);
    activeDrone->begin();
    imu.begin();

    static FlightController fc({ kButton, *activeDrone,
                                  STICKC_THROTTLE_RATE_UP, STICKC_THROTTLE_RATE_DOWN });
    flight = &fc;
    flight->setMode(modeManager.current());
    flight->begin();

    if (modeManager.current() == OperationMode::BluetoothControl) {
        gamepad.begin();
    }
}

void loop() {
    kBoard.update();

    // Right-side button (BtnB) — single press restarts the firmware.
    if (kButtonReset.wasReleased()) {
        Console.println("[Main] BtnB: restarting...");
        ESP.restart();
    }

    wifi.update();
    imu.update(kImu);

    bool wifiNow = wifi.isConnected();
    if (wifiNow && !_prevWifiConnected) {
        Console.println("[Main] WiFi connected — starting app mode entry");
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

    bool gpConnected = (gamepad.status() == Bp32Status::Connected);
    if (gpConnected && !_btWasConnected) _btConnectedMs = millis();
    _btWasConnected = gpConnected;

    bool showBtScreen = (modeManager.current() == OperationMode::BluetoothControl)
                        && (flight->state() == FlightState::Idle)
                        && (!gpConnected || !wifi.isConnected() || (millis() - _btConnectedMs < BT_STATUS_SCREEN_MS));

    uint16_t gpBtns    = gpAxes.buttons;
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
    if (!_screenOff && now - _lastDisplayMs >= DISPLAY_INTERVAL_MS) {
        _lastDisplayMs = now;
        if (showBtScreen) {
            display.drawBtStatus(toBleStatus(gamepad.status()),
                                 wifi.isConnected(),
                                 kBoard.getBatteryLevel(),
                                 kBoard.isCharging());
        } else {
            DroneState displayState = activeDrone->state();
            if (modeManager.current() == OperationMode::BluetoothControl
                && flight->state() == FlightState::Idle
                && gpAxes.connected) {
                auto tobyte = [](float v) -> uint8_t {
                    int i = DroneAxis::NEUTRAL + (int)(v * AXIS_DISPLAY_HALF_RANGE);
                    return (uint8_t)(i < AXIS_DISPLAY_MIN ? AXIS_DISPLAY_MIN :
                                      i > DroneAxis::MAX   ? DroneAxis::MAX  : i);
                };
                displayState.roll     = tobyte( gpAxes.roll);
                displayState.pitch    = tobyte(-gpAxes.pitch);
                displayState.yaw      = tobyte( gpAxes.yaw);
                displayState.throttle = DroneAxis::NEUTRAL;
                displayState.active   = true;
            }
            display.update(wifi.isConnected(), flight->state(),
                           displayState, imu.data(),
                           kBoard.getBatteryLevel(), kBoard.isCharging());
        }
    }

    static uint32_t lastLogMs = 0;
    if (now - lastLogMs >= STATUS_LOG_INTERVAL_MS) {
        lastLogMs = now;
        const DroneState& st = activeDrone->state();
        Console.printf("[Status] wifi=%d bp32=%d state=%s | roll=%3d pitch=%3d thr=%3d yaw=%3d\n",
                       wifi.isConnected(), gpAxes.connected, flightStateName(flight->state()),
                       st.roll, st.pitch, st.throttle, st.yaw);
    }

    delay(10);
}
