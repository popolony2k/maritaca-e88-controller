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
#include "gamepad_controller.h"

// Maps normalized [-1,1] with dead zone + expo → [0, 254], neutral = 128.
uint8_t GamepadController::mapAxis(float value, float deadZone, float expo) {
    if (fabsf(value) < deadZone) return 0x80;

    float sign   = value > 0.0f ? 1.0f : -1.0f;
    float scaled = (fabsf(value) - deadZone) / (1.0f - deadZone);
    if (scaled > 1.0f) scaled = 1.0f;

    scaled = scaled * ((1.0f - expo) + expo * scaled);

    return (uint8_t)((0.5f + sign * scaled * 0.5f) * 254.0f);
}

void GamepadController::begin() {
    _throttle     = THROTTLE_INIT;
    _currentRoll  = 128.0f;
    _currentPitch = 128.0f;
    _currentYaw   = 128.0f;
}

void GamepadController::update(const GamepadAxes& axes, DroneState& out) {
    if (!axes.connected) return;

    // Rate-based throttle from ZL/ZR buttons (or iPega left stick UP/DOWN).
    // Both drones run altitude-hold firmware: throttle deviation from 0x80 is
    // a climb/descend rate, not an accumulated lift level — snap back to
    // hover the instant the stick/triggers are centered.
    float rate = axes.throttleUp - axes.throttleDown;
    if (fabsf(rate) > 0.05f) {
        _throttle += rate * THROTTLE_RATE_MAX;
        if (_throttle <   0.0f) _throttle =   0.0f;
        if (_throttle > 254.0f) _throttle = 254.0f;
    } else {
        _throttle = THROTTLE_INIT;
    }

    float targetRoll  = (float)mapAxis(axes.roll,  DEAD_ZONE, EXPO);
    float targetPitch = (float)mapAxis(axes.pitch, DEAD_ZONE, EXPO);
    float targetYaw   = (float)mapAxis(axes.yaw,   DEAD_ZONE, EXPO);

    auto slew = [](float target, float current) {
        float d = target - current;
        if (d >  SLEW_RATE) d =  SLEW_RATE;
        if (d < -SLEW_RATE) d = -SLEW_RATE;
        return current + d;
    };
    _currentRoll  = slew(targetRoll,  _currentRoll);
    _currentPitch = slew(targetPitch, _currentPitch);
    _currentYaw   = slew(targetYaw,   _currentYaw);

    out.roll     = (uint8_t)_currentRoll;
    out.pitch    = (uint8_t)_currentPitch;
    out.yaw      = (uint8_t)_currentYaw;
    out.throttle = (uint8_t)_throttle;
    out.cmd      = DroneCmd::None;
    out.active   = true;
}
