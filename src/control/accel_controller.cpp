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

    scaled = scaled * ((1.0f - expo) + expo * scaled);

    return (uint8_t)((0.5f + sign * scaled * 0.5f) * 254.0f);
}

void AccelController::begin() {
    _filteredRoll  = 0.0f;
    _filteredPitch = 0.0f;
    _filteredYaw   = 0.0f;
    _throttle      = THROTTLE_INIT;
    _currentRoll   = 128.0f;
    _currentPitch  = 128.0f;
    _currentYaw    = 128.0f;
}

void AccelController::adjustThrottle(float delta) {
    _throttle += delta;
    if (_throttle <   0.0f) _throttle =   0.0f;
    if (_throttle > 254.0f) _throttle = 254.0f;
}

void AccelController::update(const ImuData& imu, DroneState& out) {
    if (!_enabled || !imu.valid) return;

    // --- Tilt angles from accelerometer ---
    float pitchDeg = atan2f(imu.ax, sqrtf(imu.ay * imu.ay + imu.az * imu.az))
                     * (180.0f / (float)M_PI);
    float rollDeg  = atan2f(imu.ay, sqrtf(imu.ax * imu.ax + imu.az * imu.az))
                     * (180.0f / (float)M_PI);

    // --- Yaw from gyroscope Z rate ---
    float yawRate = imu.gz;

    // --- Low-pass filter ---
    _filteredRoll  = ANGLE_ALPHA * rollDeg  + (1.0f - ANGLE_ALPHA) * _filteredRoll;
    _filteredPitch = ANGLE_ALPHA * pitchDeg + (1.0f - ANGLE_ALPHA) * _filteredPitch;
    _filteredYaw   = ANGLE_ALPHA * yawRate  + (1.0f - ANGLE_ALPHA) * _filteredYaw;

    // Roll negated: drone nose faces away from user, so left/right is mirrored.
    // Pitch: tilt forward = fly forward, tilt back = fly backward.
    //        If direction feels inverted, negate _filteredPitch below.
    float targetRoll  = (float)mapAxis(-_filteredRoll, MAX_TILT_DEG,  TILT_DEAD_ZONE,  TILT_EXPO);
    float targetPitch = (float)mapAxis(_filteredPitch,  MAX_PITCH_DEG, PITCH_DEAD_ZONE, PITCH_EXPO);
    float targetYaw   = (float)mapAxis(-_filteredYaw,   MAX_YAW_RATE,  YAW_DEAD_ZONE,   YAW_EXPO);

    // Slew rate limiter — ramp all axes toward target at max SLEW_RATE units/frame.
    auto slew = [](float target, float current) {
        float d = target - current;
        if (d >  SLEW_RATE) d =  SLEW_RATE;
        if (d < -SLEW_RATE) d = -SLEW_RATE;
        return current + d;
    };
    _currentRoll  = slew(targetRoll,  _currentRoll);
    _currentPitch = slew(targetPitch, _currentPitch);
    _currentYaw   = slew(targetYaw,   _currentYaw);

    out.roll  = (uint8_t)_currentRoll;
    out.pitch = (uint8_t)_currentPitch;
    out.yaw   = (uint8_t)_currentYaw;
    out.throttle = (uint8_t)_throttle;
    out.cmd      = DroneCmd::None;
    out.active   = true;
}
