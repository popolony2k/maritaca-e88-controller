#pragma once
#include <Arduino.h>
#include "../imu/accelerometer.h"
#include "../comm/drone_protocol.h"

class AccelController {
public:
    void begin();
    void update(const ImuData& imu, DroneState& out);

    void setEnabled(bool en) { _enabled = en; }
    bool isEnabled() const   { return _enabled; }

private:
    // Maps a signed value through dead zone + expo curve → [0, 254], neutral = 128.
    static uint8_t mapAxis(float value, float maxRange, float deadZone, float expo);

    bool  _enabled       = false;
    float _filteredRoll  = 0.0f;  // low-pass state
    float _filteredYaw   = 0.0f;
    float _filteredPitch = 0.0f;
    float _throttle      = 128.0f; // persistent throttle level [0, 254]

    // ---- Roll / Yaw tuning -----------------------------------------------
    static constexpr float MAX_TILT_DEG   = 20.0f;
    static constexpr float TILT_DEAD_ZONE = 10.0f;
    static constexpr float TILT_EXPO      =  0.5f;

    static constexpr float MAX_YAW_RATE   = 30.0f;
    static constexpr float YAW_DEAD_ZONE  = 10.0f;
    static constexpr float YAW_EXPO       =  0.4f;

    // ---- Throttle tuning (rate-based via pitch axis) ----------------------
    // Tilt board back  (near edge up)   → throttle increases continuously.
    // Tilt board forward (near edge down) → throttle decreases continuously.
    // Inside dead zone → throttle holds its current value.
    // Raise THROTTLE_RATE_MAX if altitude changes feel too slow.
    static constexpr float THROTTLE_DEAD_DEG  =  8.0f;  // no-change band
    static constexpr float MAX_THROTTLE_DEG   = 20.0f;  // full-rate deflection
    static constexpr float THROTTLE_RATE_MAX  =  2.5f;  // units/frame at max tilt
    static constexpr float THROTTLE_INIT      = 128.0f; // level when Flying begins

    // ---- Low-pass filter -------------------------------------------------
    static constexpr float ANGLE_ALPHA    =  0.25f;
};
