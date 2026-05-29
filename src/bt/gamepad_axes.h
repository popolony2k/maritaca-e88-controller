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
 * @brief Button bitmask constants for GamepadAxes::buttons.
 *
 * iPega PG-9021S HOME+A (digitizer) mode — fixed touchscreen positions
 * detected by BleGamepad::parseReport(). Only buttons confirmed working
 * on hardware are listed; broken or reserved buttons are omitted.
 */
namespace GamepadBtn {
    constexpr uint16_t A         = 0x0001; ///< Face button A  (x=332,  y=2044).
    constexpr uint16_t B         = 0x0002; ///< Face button B  (x=780,  y=1281).
    constexpr uint16_t X         = 0x0004; ///< Face button X  (x=241,  y=1270).
    constexpr uint16_t Y         = 0x0008; ///< Face button Y  (x=818,  y=2020).
    constexpr uint16_t DpadUp    = 0x0010; ///< D-pad UP       (x=723,  y=544).
    constexpr uint16_t DpadDown  = 0x0020; ///< D-pad DOWN     (x=352,  y=562).
    constexpr uint16_t DpadLeft  = 0x0040; ///< D-pad LEFT     (x=534,  y=379).
    constexpr uint16_t DpadRight = 0x0080; ///< D-pad RIGHT    (x=536,  y=740).
    constexpr uint16_t LT        = 0x0100; ///< Left trigger   (x=1173, y=416).
    constexpr uint16_t R1        = 0x0200; ///< Right shoulder (x=578,  y=2041).
}

/**
 * @brief Normalised axis and button values from a BLE HID gamepad.
 *
 * All values are pre-normalised by BleGamepad::parseReport() so that
 * GamepadController and any future consumers need no knowledge of the
 * raw HID report format.
 */
struct GamepadAxes {
    bool     connected    = false;  ///< True while a gamepad is actively connected.
    float    roll         = 0.0f;   ///< Left stick X  [-1, 1], positive = right.
    float    pitch        = 0.0f;   ///< Left stick Y  [-1, 1], positive = forward.
    float    yaw          = 0.0f;   ///< Right stick X [-1, 1], positive = right.
    float    throttleUp   = 0.0f;   ///< Right stick up   [0, 1] — climb.
    float    throttleDown = 0.0f;   ///< Right stick down [0, 1] — descend.
    uint16_t buttons      = 0;      ///< Bitmask of pressed buttons (GamepadBtn::*).
};
