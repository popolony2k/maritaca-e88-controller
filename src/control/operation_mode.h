#pragma once

enum class OperationMode {
    AccelControl,     // AtomS3 tilt/accel-driven
    BluetoothControl, // BLE gamepad-driven
};

class OperationModeManager {
public:
    OperationMode current() const { return _mode; }
    void setMode(OperationMode m) { _mode = m; }

private:
    OperationMode _mode = OperationMode::BluetoothControl;
};
