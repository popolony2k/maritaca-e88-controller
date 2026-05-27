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
#include "../hal/hal.h"

/**
 * @brief Raw IMU sample from a single poll cycle.
 */
struct ImuData {
    float ax = 0, ay = 0, az = 0; ///< Accelerometer readings (g).
    float gx = 0, gy = 0, gz = 0; ///< Gyroscope readings (deg/s).
    bool  valid = false;           ///< False until the first successful HAL read.
};

/**
 * @brief Non-blocking IMU poller.
 *
 * Reads accelerometer and gyroscope via ImuHal at a fixed 50 Hz rate.
 * Consumers call data() to get the latest sample; stale data is never
 * re-published — @c valid stays true from the first successful read onward.
 */
class Accelerometer {
public:
    /** @brief Reset internal state. Call once in setup(). */
    void begin();

    /**
     * @brief Poll the IMU at 50 Hz and update the internal ImuData sample.
     * @param hal  IMU hardware abstraction providing getAccel/getGyro.
     */
    void update(const ImuHal& hal);

    /** @brief Latest IMU sample. Valid after the first successful read. */
    const ImuData& data() const { return _data; }

private:
    ImuData  _data;
    uint32_t _lastUpdateMs = 0;

    static constexpr uint32_t UPDATE_INTERVAL_MS = 20; ///< 50 Hz poll rate.
};
