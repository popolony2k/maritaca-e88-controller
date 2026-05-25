#pragma once

enum class OperationMode {
    AccelControl,   // AtomS3 is the sole controller, tilt/accel-driven
    // MirrorControl,  // future: companion / mirror mode
};

class OperationModeManager {
public:
    OperationMode current() const { return _mode; }

private:
    OperationMode _mode = OperationMode::AccelControl;
};
