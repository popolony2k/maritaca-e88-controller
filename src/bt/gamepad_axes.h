#pragma once
#include <Arduino.h>

/**
 * @brief Normalised axis and button values from a BLE HID gamepad.
 *
 * All values are pre-normalised by BleGamepad::parseReport() so that
 * GamepadController and any future consumers need no knowledge of the
 * raw HID report format.
 */
struct GamepadAxes {
    bool  connected    = false; ///< True while a gamepad is actively connected.
    float roll         = 0.0f;  ///< Left stick X  [-1, 1], positive = right.
    float pitch        = 0.0f;  ///< Left stick Y  [-1, 1], positive = forward.
    float yaw          = 0.0f;  ///< Right stick X [-1, 1], positive = right.
    float throttleUp   = 0.0f;  ///< ZR trigger [0, 1] — climb.
    float throttleDown = 0.0f;  ///< ZL trigger [0, 1] — descend.
};
