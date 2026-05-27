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
