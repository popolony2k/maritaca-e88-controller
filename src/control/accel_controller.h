#pragma once
#include <Arduino.h>
#include "../imu/accelerometer.h"
#include "../comm/drone_protocol.h"

class AccelController {
public:
    void begin();
    void update(const ImuData& imu, DroneState& out);

    void setEnabled(bool en)      { _enabled = en; }
    bool isEnabled()  const       { return _enabled; }

    void adjustThrottle(float delta);

private:
    static uint8_t mapAxis(float value, float maxRange, float deadZone, float expo);

    bool  _enabled       = false;
    float _filteredRoll  = 0.0f;
    float _filteredYaw   = 0.0f;
    float _filteredPitch = 0.0f;
    float _throttle      = 128.0f; // persistent throttle level [0, 254]
    float _currentRoll   = 128.0f; // slew-limited output state
    float _currentPitch  = 128.0f;
    float _currentYaw    = 128.0f;

    // ---- Roll tuning -----------------------------------------------
    static constexpr float MAX_TILT_DEG   = 20.0f;
    static constexpr float TILT_DEAD_ZONE = 10.0f;
    static constexpr float TILT_EXPO      =  0.5f;

    // ---- Yaw tuning ------------------------------------------------
    static constexpr float MAX_YAW_RATE   = 120.0f;
    static constexpr float YAW_DEAD_ZONE  =  20.0f;
    static constexpr float YAW_EXPO       =   0.5f;

    // ---- Pitch (forward/backward) tuning ---------------------------
    // If forward/backward feels inverted, negate _filteredPitch in update().
    static constexpr float MAX_PITCH_DEG   = 20.0f;
    static constexpr float PITCH_DEAD_ZONE = 10.0f;
    static constexpr float PITCH_EXPO      =  0.5f;

    // ---- Slew rate limiter -----------------------------------------
    // Max output change per frame (25 Hz). 5 units/frame = ~1s neutral→full.
    static constexpr float SLEW_RATE       =   3.0f;

    // ---- Throttle --------------------------------------------------
    static constexpr float THROTTLE_INIT   = 128.0f; // hover-neutral when Flying begins

    // ---- Low-pass filter -------------------------------------------
    static constexpr float ANGLE_ALPHA     =  0.25f;
};
