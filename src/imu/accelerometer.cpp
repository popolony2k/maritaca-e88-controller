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
#include "accelerometer.h"

void Accelerometer::begin() {
    _data = {};
}

void Accelerometer::update(const ImuHal& hal) {
    uint32_t now = millis();
    if (now - _lastUpdateMs < UPDATE_INTERVAL_MS) return;
    _lastUpdateMs = now;

    _data.valid = hal.getAccel(&_data.ax, &_data.ay, &_data.az);
    if (_data.valid) {
        hal.getGyro(&_data.gx, &_data.gy, &_data.gz);
    }
}
