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
#include "../imu/accelerometer.h"
#include "../comm/drone_protocol.h"

/**
 * @brief Tilt-to-flight control mapping for the AtomS3 accelerometer/gyroscope.
 *
 * Converts raw IMU data into Eachine-protocol axis values:
 *   - Left/right board tilt  → roll
 *   - Forward/backward tilt  → pitch
 *   - Gyroscope Z rate       → yaw
 *   - Throttle is rate-based: held via adjustThrottle(), not directly mapped.
 *
 * Each axis passes through a dead zone, an expo curve, and a slew-rate
 * limiter before being written to DroneState. An exponential moving average
 * (EMA) low-pass filter smooths IMU noise.
 *
 * Sign conventions:
 *   - Roll  is negated: drone nose faces away from the user, so left/right is mirrored.
 *   - Yaw   is negated: same reason.
 *   - Pitch is not negated: tilt forward = fly forward.
 */
class AccelController {
public:
    /** @brief Reset all filtered values and set throttle to hover-neutral. */
    void begin();

    /**
     * @brief Compute axis values from IMU data and write to @p out.
     *
     * Does nothing if disabled or if ImuData::valid is false.
     * @param imu  Latest IMU sample from Accelerometer::data().
     * @param out  DroneState to populate; out.active is set to true on success.
     */
    void update(const ImuData& imu, DroneState& out);

    /** @brief Enable or disable output. Disabled state leaves @p out unchanged. */
    void setEnabled(bool en) { _enabled = en; }

    /** @brief True when the controller is producing output. */
    bool isEnabled() const { return _enabled; }

    /**
     * @brief Nudge the persistent throttle level by @p delta units.
     * @param delta  Positive to increase, negative to decrease. Clamped to [0, 254].
     */
    void adjustThrottle(float delta);

private:
    /**
     * @brief Map a signed sensor value to the Eachine [0, 254] range.
     *
     * Values within ±deadZone map to neutral (0x80). Outside the dead zone,
     * the value is rescaled to [0, 1], passed through an expo curve, then
     * mapped to [0, 254] symmetrically around 128.
     *
     * @param value     Sensor reading in physical units (deg or deg/s).
     * @param maxRange  Sensor value that maps to full deflection.
     * @param deadZone  Half-width of the neutral dead band.
     * @param expo      Expo blend factor: 0 = linear, 1 = quadratic.
     * @return Eachine axis byte [0x00, 0xFE], neutral = 0x80.
     */
    static uint8_t mapAxis(float value, float maxRange, float deadZone, float expo);

    bool  _enabled       = false;
    float _filteredRoll  = 0.0f;
    float _filteredYaw   = 0.0f;
    float _filteredPitch = 0.0f;
    float _throttle      = 128.0f; ///< Persistent throttle level [0, 254].
    float _currentRoll   = 128.0f; ///< Slew-limited roll output.
    float _currentPitch  = 128.0f; ///< Slew-limited pitch output.
    float _currentYaw    = 128.0f; ///< Slew-limited yaw output.

    // ---- Roll / Pitch tuning -------------------------------------------
    static constexpr float MAX_TILT_DEG    = 20.0f; ///< Tilt angle for full deflection (deg).
    static constexpr float TILT_DEAD_ZONE  = 10.0f; ///< Dead zone half-width (deg).
    static constexpr float TILT_EXPO       =  0.5f; ///< Expo for roll/pitch.

    // ---- Yaw tuning ----------------------------------------------------
    static constexpr float MAX_YAW_RATE    = 120.0f; ///< Gyro Z rate for full yaw (deg/s).
    static constexpr float YAW_DEAD_ZONE   =  20.0f; ///< Dead zone half-width (deg/s).
    static constexpr float YAW_EXPO        =   0.5f; ///< Expo for yaw.

    // ---- Pitch (forward/backward) tuning --------------------------------
    static constexpr float MAX_PITCH_DEG   = 20.0f;
    static constexpr float PITCH_DEAD_ZONE = 10.0f;
    static constexpr float PITCH_EXPO      =  0.5f;

    // ---- Slew rate limiter ---------------------------------------------
    static constexpr float SLEW_RATE       =  3.0f; ///< Max output change per frame at 25 Hz.

    // ---- Throttle ------------------------------------------------------
    static constexpr float THROTTLE_INIT   = 128.0f; ///< Starting throttle when Flying begins.

    // ---- Low-pass filter -----------------------------------------------
    static constexpr float ANGLE_ALPHA     =  0.25f; ///< EMA coefficient (higher = less smoothing).
};
