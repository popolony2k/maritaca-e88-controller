// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <Arduino.h>
#include "bt/gamepad_axes.h"

/**
 * @brief Connection state of the Bluepad32 gamepad host.
 *
 * Mirrors BleStatus from the main firmware's ble_gamepad.h so this class's
 * usage pattern matches BleGamepad's, even though the underlying stack
 * (Bluepad32/BR-EDR vs Arduino BLEDevice) is completely different.
 */
enum class Bp32Status {
    Scanning,   ///< No controller connected yet.
    Connected,  ///< Controller connected and reporting data.
};

/**
 * @brief Bluepad32-based BR/EDR HID gamepad host.
 *
 * Replaces BleGamepad (BLE-only) for this headless build — the M5StickC
 * Plus2's ESP32-PICO-V3-02 has a genuine BR/EDR radio, so Switch-mode
 * controllers (e.g. 8BitDo Zero 2) connect directly via Bluepad32 instead of
 * needing BLE HID emulation.
 *
 * Axis mapping (Nintendo Switch Pro Controller layout, matches the
 * previously-coded-but-BLE-unreachable 8BitDo Switch-mode convention):
 *   - Left  stick X/Y  → roll/pitch
 *   - Right stick X    → yaw
 *   - R2 (analog)      → throttle up
 *   - L2 (analog)      → throttle down
 *
 * Buttons map onto the same GamepadBtn bits FlightController::
 * handleGamepadButtons() already consumes (A/B/X/Y unchanged; D-pad from
 * Bluepad32's dpad() bitmask; LT/R1 approximated from the L1/R1 shoulder
 * buttons — exact feel to be tuned against real hardware).
 */
class Bp32Gamepad {
public:
    /** @brief Initialise Bluepad32 and start scanning. Call once in setup(). */
    void begin();

    /** @brief Poll Bluepad32 for new controller data. Call every loop() — non-blocking. */
    void update();

    /** @brief Latest normalised axis values. Only valid when axes().connected is true. */
    const GamepadAxes& axes() const { return _axes; }

    /** @brief Current connection state. */
    Bp32Status status() const { return _status; }

private:
    GamepadAxes _axes;
    Bp32Status  _status = Bp32Status::Scanning;
};
