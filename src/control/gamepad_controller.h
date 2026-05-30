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
#include "../bt/gamepad_axes.h"
#include "../comm/drone_protocol.h"

/**
 * @brief Maps normalised BLE gamepad axes to Eachine protocol control values.
 *
 * Axis mapping (iPega PG-9021S HOME+A mode):
 *   - Left  stick UP/DOWN    → throttle (rate-based accumulator)
 *   - Left  stick LEFT/RIGHT → yaw
 *   - Right stick UP/DOWN    → pitch
 *   - Right stick LEFT/RIGHT → roll
 *
 * Axis mapping (8BitDo Switch mode):
 *   - Left  stick X → roll,  Left stick Y → pitch
 *   - Right stick X → yaw
 *   - ZR (hold) → throttle up,  ZL (hold) → throttle down
 *
 * Each stick axis passes through a dead zone, expo curve, and slew-rate
 * limiter matching the feel of AccelController. Throttle is rate-based —
 * releasing both triggers holds the current altitude.
 */
class GamepadController {
public:
    /** @brief Reset throttle and slew state to neutral. */
    void begin();

    /**
     * @brief Compute axis values from gamepad input and write to @p out.
     *
     * Does nothing if axes.connected is false.
     * @param axes  Normalised gamepad axes from BleGamepad::axes().
     * @param out   DroneState to populate; out.active is set to true on success.
     */
    void update(const GamepadAxes& axes, DroneState& out);

private:
    /**
     * @brief Map a normalised [-1, 1] axis value to the Eachine [0, 254] range.
     *
     * @param value    Normalised stick position [-1, 1].
     * @param deadZone Dead zone threshold (fraction of full scale).
     * @param expo     Expo blend factor: 0 = linear, 1 = quadratic.
     * @return Eachine axis byte [0x00, 0xFE], neutral = 0x80.
     */
    static uint8_t mapAxis(float value, float deadZone, float expo);

    float _throttle     = 128.0f; ///< Persistent throttle level [0, 254].
    float _currentRoll  = 128.0f; ///< Slew-limited roll output.
    float _currentPitch = 128.0f; ///< Slew-limited pitch output.
    float _currentYaw   = 128.0f; ///< Slew-limited yaw output.

    static constexpr float DEAD_ZONE         = 0.12f; ///< 12% dead zone around stick centre.
    static constexpr float EXPO              =  0.4f; ///< Expo curve blend factor.
    static constexpr float SLEW_RATE         =  8.0f; ///< Max output change per frame at 25 Hz.
    static constexpr float THROTTLE_RATE_MAX =  0.6f; ///< Throttle units/frame at full trigger.
    static constexpr float THROTTLE_INIT     = 128.0f; ///< Starting throttle when Flying begins.
};
