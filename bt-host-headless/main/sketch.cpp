// SPDX-License-Identifier: Apache-2.0
// BT-gamepad-to-drone build for the M5StickC Plus2. Bluepad32 reads an 8BitDo
// (Switch mode, BR/EDR) controller and drives the same DroneProtocol/
// FlowWifiProtocol/FlightController logic the main Arduino-framework firmware
// uses, reused directly from ../../src (see main/CMakeLists.txt) since that
// logic only depends on millis()/WiFiUdp/Serial — all present here via the
// arduino-esp32 core component. Display/battery now reused the same way via
// the vendored M5Unified/M5GFX components (see components/M5Unified,
// components/M5GFX). Physical BtnA gestures (double-click arm/land,
// triple-click emergency stop) reuse src/hal/m5stickcplus2's existing
// kButton HAL. This build still has no AccelControl mode and no
// mode-select screen (permanently BluetoothControl).
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
#include "ui/display.h"
#include "hal/m5stickcplus2.h"

// Known drone SSIDs — first match found during scan wins. Mirrors
// src/main.cpp's DRONE_SSIDS table.
static const char* const DRONE_SSIDS[] = {
    "WIFI_8K_Wf48702",   // index 0 — black drone (WIFI_8K_ variant)
    "FLOW-WIFI-304BA",   // index 1 — grey drone  (FLOW-WIFI variant)
};
static constexpr int N_DRONES = 2;
static constexpr uint32_t STATUS_LOG_INTERVAL_MS = 500;
static constexpr uint32_t DISPLAY_INTERVAL_MS    = 100; // 10 Hz, matches main.cpp
static constexpr uint32_t BT_STATUS_SCREEN_MS    = 1500; // matches main.cpp
static constexpr uint8_t  AXIS_DISPLAY_MIN       =    1; // matches main.cpp
static constexpr float    AXIS_DISPLAY_HALF_RANGE = 127.0f; // matches main.cpp

static WifiManager      wifi;
static DroneProtocol    blackDrone;
static FlowWifiProtocol greyDrone;
static DroneProtocolBase* activeDrone = &blackDrone;
static Bp32Gamepad      gamepad;
static FlightController* flight = nullptr;
static Display           display(kDisplay);

static bool _prevWifiConnected = false;

static uint32_t _btConnectedMs    = 0;
static bool     _btWasConnected   = false;
static bool     _prevShowBtScreen = false;
static bool     _screenOff        = false;
static uint16_t _prevGpBtns       = 0;
static uint32_t _lastDisplayMs    = 0;

// Bp32Status has no separate "connecting" phase the way BLE's scan/connect
// callbacks did — map both Scanning and the brief post-connect window onto
// BleStatus::Scanning/Connected, which is all drawBtStatus() distinguishes
// here (it also accepts Connecting, never reached from this build).
static BleStatus toBleStatus(Bp32Status s) {
    return s == Bp32Status::Connected ? BleStatus::Connected : BleStatus::Scanning;
}

void setup() {
    kBoard.begin();
    display.begin();

    Console.printf("Firmware: %s\n", BP32.firmwareVersion());

    int droneIdx = WifiManager::scanForFirst(DRONE_SSIDS, N_DRONES);
    if (droneIdx < 0) droneIdx = 0;  // default to black drone if none visible

    if (droneIdx == 1) {
        activeDrone = &greyDrone;
        Console.println("[Boot] Drone: FLOW-WIFI grey");
    } else {
        activeDrone = &blackDrone;
        Console.println("[Boot] Drone: WIFI_8K_ black");
    }

    // BT coexistence (Bluepad32/BTstack running concurrently) can make initial
    // WiFi association take 10-25+ s — wider than the 5 s default, which would
    // otherwise restart the attempt mid-handshake. See setReconnectInterval().
    wifi.setReconnectInterval(30000);
    wifi.begin(DRONE_SSIDS[droneIdx]);
    // WiFi modem sleep periodically cedes the radio to BT for coexistence —
    // mid-cede UDP sends fail outright ("could not send data: 12"). Disabling
    // sleep keeps WiFi consistently available, at the cost of more aggressive
    // radio time-sharing with Bluepad32/BTstack.
    WiFi.setSleep(false);
    activeDrone->begin();

    static FlightController fc({ kButton, *activeDrone });
    flight = &fc;
    flight->setMode(OperationMode::BluetoothControl);
    flight->begin();

    gamepad.begin();
}

void loop() {
    kBoard.update();
    wifi.update();

    bool wifiNow = wifi.isConnected();
    if (wifiNow && !_prevWifiConnected) {
        Console.println("[Main] WiFi connected — starting app mode entry");
        activeDrone->beginAppModeEntry();
    }
    _prevWifiConnected = wifiNow;

    gamepad.update();
    const GamepadAxes& gpAxes = gamepad.axes();
    static ImuData unusedImu{};  // AccelControl-only; this build is BT-gamepad-only.
    flight->update(unusedImu, gpAxes, wifi.isConnected());

    if (wifi.isConnected()) {
        activeDrone->update();
    }

    bool gpConnected = (gamepad.status() == Bp32Status::Connected);
    if (gpConnected && !_btWasConnected) _btConnectedMs = millis();
    _btWasConnected = gpConnected;

    bool showBtScreen = (flight->state() == FlightState::Idle)
                        && (!gpConnected || !wifi.isConnected() || (millis() - _btConnectedMs < BT_STATUS_SCREEN_MS));

    // D-pad LEFT: toggle screen on/off — only available on the flight HUD.
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
            // In Idle+BT preview gamepad axes in the HUD bars without
            // touching the drone protocol (no UDP sent) — matches main.cpp.
            DroneState displayState = activeDrone->state();
            if (flight->state() == FlightState::Idle && gpAxes.connected) {
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
                           displayState, unusedImu,
                           kBoard.getBatteryLevel(), kBoard.isCharging());
        }
    }

    static uint32_t lastLogMs = 0;
    if (now - lastLogMs >= STATUS_LOG_INTERVAL_MS) {
        lastLogMs = now;
        const DroneState& st = activeDrone->state();
        Console.printf("[Status] wifi=%d bp32=%d state=%s | roll=%3d pitch=%3d thr=%3d yaw=%3d | axes roll=%.2f pitch=%.2f yaw=%.2f up=%.2f down=%.2f btn=0x%04x\n",
                       wifi.isConnected(), gpAxes.connected, flightStateName(flight->state()),
                       st.roll, st.pitch, st.throttle, st.yaw,
                       gpAxes.roll, gpAxes.pitch, gpAxes.yaw, gpAxes.throttleUp, gpAxes.throttleDown, gpAxes.buttons);
    }

    delay(10);
}
