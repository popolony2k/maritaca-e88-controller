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
