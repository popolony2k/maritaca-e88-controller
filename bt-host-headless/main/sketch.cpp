// SPDX-License-Identifier: Apache-2.0
// Headless BT-gamepad-to-drone build for the M5StickC Plus2. No display, no
// M5Unified — Bluepad32 reads an 8BitDo (Switch mode, BR/EDR) controller and
// drives the same DroneProtocol/FlowWifiProtocol/FlightController logic the
// main Arduino-framework firmware uses, reused directly from ../../src (see
// main/CMakeLists.txt) since that logic only depends on millis()/WiFiUdp/
// Serial — all present here via the arduino-esp32 core component.
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

// Known drone SSIDs — first match found during scan wins. Mirrors
// src/main.cpp's DRONE_SSIDS table.
static const char* const DRONE_SSIDS[] = {
    "WIFI_8K_Wf48702",   // index 0 — black drone (WIFI_8K_ variant)
    "FLOW-WIFI-304BA",   // index 1 — grey drone  (FLOW-WIFI variant)
};
static constexpr int N_DRONES = 2;
static constexpr uint32_t STATUS_LOG_INTERVAL_MS = 500;

// This build never has a physical screen button — FlightController only
// consumes ButtonHal in AccelControl mode, which this build never enters.
static bool stubFalse() { return false; }
static bool stubFalseMs(uint32_t) { return false; }
static const ButtonHal kStubButton = {
    .wasPressed  = stubFalse,
    .wasReleased = stubFalse,
    .pressedFor  = stubFalseMs,
};

static WifiManager      wifi;
static DroneProtocol    blackDrone;
static FlowWifiProtocol greyDrone;
static DroneProtocolBase* activeDrone = &blackDrone;
static Bp32Gamepad      gamepad;
static FlightController* flight = nullptr;

static bool _prevWifiConnected = false;

void setup() {
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

    static FlightController fc({ kStubButton, *activeDrone });
    flight = &fc;
    flight->setMode(OperationMode::BluetoothControl);
    flight->begin();

    gamepad.begin();
}

void loop() {
    wifi.update();

    bool wifiNow = wifi.isConnected();
    if (wifiNow && !_prevWifiConnected) {
        Console.println("[Main] WiFi connected — starting app mode entry");
        activeDrone->beginAppModeEntry();
    }
    _prevWifiConnected = wifiNow;

    gamepad.update();
    static ImuData unusedImu{};  // AccelControl-only; this build is BT-gamepad-only.
    flight->update(unusedImu, gamepad.axes(), wifi.isConnected());

    if (wifi.isConnected()) {
        activeDrone->update();
    }

    static uint32_t lastLogMs = 0;
    uint32_t now = millis();
    if (now - lastLogMs >= STATUS_LOG_INTERVAL_MS) {
        lastLogMs = now;
        const DroneState& st = activeDrone->state();
        const GamepadAxes& ax = gamepad.axes();
        Console.printf("[Status] wifi=%d bp32=%d state=%s | roll=%3d pitch=%3d thr=%3d yaw=%3d | axes roll=%.2f pitch=%.2f yaw=%.2f up=%.2f down=%.2f btn=0x%04x\n",
                       wifi.isConnected(), ax.connected, flightStateName(flight->state()),
                       st.roll, st.pitch, st.throttle, st.yaw,
                       ax.roll, ax.pitch, ax.yaw, ax.throttleUp, ax.throttleDown, ax.buttons);
    }

    delay(10);
}
