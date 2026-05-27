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
#include "gamepad_axes.h"

/**
 * @brief BLE connection state of the gamepad.
 */
enum class BleStatus {
    Scanning,   ///< Actively scanning for BLE HID devices.
    Connecting, ///< Found a device; connection attempt in progress.
    Connected,  ///< Device connected and HID reports are being received.
};

/**
 * @brief BLE HID host driver for an 8BitDo controller in Switch mode.
 *
 * Scans for devices advertising the HID service (UUID 0x1812), connects,
 * subscribes to Input Report notifications, and exposes normalised axes
 * via GamepadAxes. All BLE library includes are confined to ble_gamepad.cpp.
 *
 * Pairing: hold X + Start on the 8BitDo until the LED blinks rapidly.
 *
 * Thread safety: the notify callback runs in the BLE task; report data is
 * transferred to the main task via a volatile flag + memcpy buffer.
 */
class BleGamepad {
public:
    /** @brief Initialise the BLE stack and start scanning. Call once in setup(). */
    void begin();

    /** @brief Drive the connection state machine. Call every loop() — non-blocking. */
    void update();

    /** @brief Latest normalised axis values. Only valid when axes().connected is true. */
    const GamepadAxes& axes()   const { return _axes;   }

    /** @brief Current scanning/connecting/connected state. */
    BleStatus          status() const { return _status; }

    /** @brief Called by the BLE connect callback — not for external use. */
    void onConnected();

    /** @brief Called by the BLE disconnect callback — not for external use. */
    void onDisconnected();

    /** @brief Called by the scan callback to initiate a connection — not for external use. */
    void doConnect();

    static BleGamepad* _instance; ///< Singleton pointer used by static BLE callbacks.

private:
    /**
     * @brief Start a 5-second passive BLE scan window.
     *
     * Clears previous scan results before starting to prevent heap accumulation.
     * update() restarts the scan every ~6 s while not connected.
     */
    void startScan();

    /**
     * @brief Parse a raw HID Input Report from the 8BitDo in Switch mode.
     *
     * Expected format (7 bytes; may be prefixed with a 1-byte Report ID 0x01):
     *   [0] Buttons[7:0]  B=0x01 A=0x02 Y=0x04 X=0x08 L=0x10 R=0x20 ZL=0x40 ZR=0x80
     *   [1] Buttons[15:8] -=0x01 +=0x02 L3=0x04 R3=0x08
     *   [2] HAT (0=N 2=E 4=S 6=W 8=center)
     *   [3] Left stick X  (0–255, center≈128)
     *   [4] Left stick Y  (0–255, center≈128, up=0)
     *   [5] Right stick X (0–255, center≈128)
     *   [6] Right stick Y (0–255, center≈128)
     *
     * The first 20 reports are logged to Serial to aid format verification
     * with a physical controller.
     *
     * @param data  Pointer to the raw report bytes.
     * @param len   Number of bytes in the report.
     */
    void parseReport(const uint8_t* data, uint8_t len);

    GamepadAxes _axes;
    BleStatus   _status     = BleStatus::Scanning;
    uint32_t    _debugCount = 0;
    uint32_t    _lastScanMs = 0; ///< Timestamp of the last startScan() call.
};
