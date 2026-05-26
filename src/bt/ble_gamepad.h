#pragma once
#include <Arduino.h>
#include "gamepad_axes.h"

enum class BleStatus { Scanning, Connecting, Connected };

// BLE HID host for 8BitDo in Switch mode (X+Start pairing).
// All BLE library includes are confined to ble_gamepad.cpp.
class BleGamepad {
public:
    void begin();   // initialize BLE, start scanning
    void update();  // call every loop — non-blocking

    const GamepadAxes& axes()  const { return _axes;  }
    BleStatus          status() const { return _status; }

    // Called by internal BLE callbacks — not for external use.
    void onConnected();
    void onDisconnected();
    void doConnect();

    static BleGamepad* _instance;

private:
    void startScan();
    void parseReport(const uint8_t* data, uint8_t len);

    GamepadAxes _axes;
    BleStatus   _status     = BleStatus::Scanning;
    uint32_t    _debugCount = 0;
};
