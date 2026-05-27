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

/**
 * @brief Available control modes for the drone controller.
 */
enum class OperationMode {
    AccelControl,     ///< AtomS3 tilt/accelerometer-driven control.
    BluetoothControl, ///< BLE HID gamepad-driven control.
};

/**
 * @brief Holds and mutates the active OperationMode.
 *
 * Owned by main.cpp and shared with FlightController via setMode().
 * Default is BluetoothControl (selected at boot if the user does not
 * interact with the mode-selection screen).
 */
class OperationModeManager {
public:
    /** @brief The currently active mode. */
    OperationMode current() const { return _mode; }

    /** @brief Switch to a different mode. */
    void setMode(OperationMode m) { _mode = m; }

private:
    OperationMode _mode = OperationMode::BluetoothControl;
};
