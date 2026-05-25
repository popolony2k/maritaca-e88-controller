#include <Arduino.h>
#include <math.h>
#include "accel_controller.h"

// Maps a signed value in [-maxRange, +maxRange] with dead zone and expo to [0, 254].
// Values inside ±deadZone → neutral (128).
// expo: 0 = linear, 1 = quadratic (small inputs give much smaller outputs).
uint8_t AccelController::mapAxis(float value, float maxRange, float deadZone, float expo) {
    if (fabsf(value) < deadZone) return 0x80;

    float sign   = value > 0.0f ? 1.0f : -1.0f;
    float scaled = (fabsf(value) - deadZone) / (maxRange - deadZone);
    if (scaled > 1.0f) scaled = 1.0f;

    // Expo curve: blends linear with quadratic.
    // At expo=0.5: 25% deflection → ~16% output (vs 25% linear).
    scaled = scaled * ((1.0f - expo) + expo * scaled);

    return (uint8_t)((0.5f + sign * scaled * 0.5f) * 254.0f);
}

void AccelController::begin() {
    _filteredRoll  = 0.0f;
    _filteredPitch = 0.0f;
    _filteredYaw   = 0.0f;
    _throttle      = THROTTLE_INIT;
}

void AccelController::update(const ImuData& imu, DroneState& out) {
    if (!_enabled || !imu.valid) return;

    // --- Tilt angles from accelerometer ---
    float pitchDeg = atan2f(imu.ax, sqrtf(imu.ay * imu.ay + imu.az * imu.az))
                     * (180.0f / (float)M_PI);
    float rollDeg  = atan2f(imu.ay, sqrtf(imu.ax * imu.ax + imu.az * imu.az))
                     * (180.0f / (float)M_PI);

    // --- Yaw from gyroscope Z rate ---
    float yawRate = imu.gz;  // deg/s

    // --- Low-pass filter ---
    _filteredRoll  = ANGLE_ALPHA * rollDeg  + (1.0f - ANGLE_ALPHA) * _filteredRoll;
    _filteredPitch = ANGLE_ALPHA * pitchDeg + (1.0f - ANGLE_ALPHA) * _filteredPitch;
    _filteredYaw   = ANGLE_ALPHA * yawRate  + (1.0f - ANGLE_ALPHA) * _filteredYaw;

    // --- Rate-based throttle via pitch axis ---
    // Tilt back (near edge up)    → _filteredPitch positive → throttle up.
    // Tilt forward (near edge down) → _filteredPitch negative → throttle down.
    // Inside dead zone → throttle holds current value.
    float absPitch = fabsf(_filteredPitch);
    if (absPitch > THROTTLE_DEAD_DEG) {
        float beyond = absPitch - THROTTLE_DEAD_DEG;
        float rate   = beyond / (MAX_THROTTLE_DEG - THROTTLE_DEAD_DEG);
        if (rate > 1.0f) rate = 1.0f;
        float delta  = (_filteredPitch > 0.0f ? 1.0f : -1.0f) * rate * THROTTLE_RATE_MAX;
        _throttle += delta;
        if (_throttle <   0.0f) _throttle =   0.0f;
        if (_throttle > 254.0f) _throttle = 254.0f;
    }

    // Pitch axis drives throttle — drone pitch is fixed at neutral.
    out.roll     = mapAxis(_filteredRoll, MAX_TILT_DEG, TILT_DEAD_ZONE, TILT_EXPO);
    out.pitch    = 0x80;
    out.yaw      = mapAxis(_filteredYaw,  MAX_YAW_RATE, YAW_DEAD_ZONE,  0.0f);
    out.throttle = (uint8_t)_throttle;
    out.cmd      = DroneCmd::None;
    out.active   = true;
}
