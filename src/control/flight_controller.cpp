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
#include "flight_controller.h"
#include "../bt/gamepad_axes.h"

const char* flightStateName(FlightState s) {
    switch (s) {
        case FlightState::Idle:        return "IDLE";
        case FlightState::Calibrating: return "CALIB";
        case FlightState::Arming:      return "ARMING";
        case FlightState::Flying:      return "FLYING";
        case FlightState::Landing:     return "LANDING";
        case FlightState::Emergency:   return "EMERGENCY";
        default:                       return "?";
    }
}

FlightController::FlightController(FlightDeps deps) : _deps(deps) {}

void FlightController::begin() {
    _accel.begin();
    _gamepad.begin();
    _state           = FlightState::Idle;
    _stateEnteredMs  = millis();
    _stateFirstFrame = true;
    // Do NOT call enterState() — exitAppMode() must not fire at boot.
}

void FlightController::update(const ImuData& imu, const GamepadAxes& gamepad, bool wifiOk) {
    _lastGamepadAxes = gamepad;
    handleButton(wifiOk);
    if (_mode == OperationMode::BluetoothControl) handleGamepadButtons(wifiOk);
    runState(imu, wifiOk);
}

void FlightController::enterState(FlightState s, bool sendModeCmd) {
    _state           = s;
    _stateEnteredMs  = millis();
    _stateFirstFrame = true;
    _btnIsHold       = false;
    _buttonDown      = false;
    _clickCount      = 0;

    _oneShotCmd   = DroneCmd::None;
    _oneShotUntil = 0;
    _headless     = false;

    // Yaw defaults ON for gamepad (right stick controls it intentionally),
    // OFF for accel (gyro Z drift would cause unwanted rotation).
    _yawEnabled = (s == FlightState::Flying &&
                   _mode == OperationMode::BluetoothControl);

    _accel.setEnabled(s == FlightState::Flying);
    if (s == FlightState::Flying) {
        _accel.begin();
        _gamepad.begin();
        // Drones without arm sequence (e.g. FLOW-WIFI) use TakeOff toggle to arm and lift off.
        if (!_deps.drone.supportsArmSequence()) {
            _oneShotCmd   = DroneCmd::TakeOff;
            _oneShotUntil = millis() + 1000;
        }
    }

    if (sendModeCmd && s == FlightState::Idle) {
        _deps.drone.exitAppMode();
    }

    Serial.printf("[Flight] -> %s\n", flightStateName(s));
}

void FlightController::handleButton(bool wifiOk) {
    uint32_t now     = millis();
    uint32_t elapsed = now - _stateEnteredMs;

    if (_deps.button.wasPressed()) _buttonDown = true;

    // Hold detection — AccelControl only, Flying state, after lockout
    if (_mode == OperationMode::AccelControl &&
        _state == FlightState::Flying &&
        elapsed >= ACCEL_LOCKOUT_MS &&
        _deps.button.pressedFor(HOLD_THRESHOLD_MS) &&
        !_btnIsHold) {
        _btnHoldIsDown = (_clickCount == 1);
        _btnIsHold     = true;
        _clickCount    = 0;
        Serial.printf("[Flight] Throttle hold -> %s\n",
                      _btnHoldIsDown ? "DOWN" : "UP");
    }

    if (_deps.button.wasReleased()) {
        _buttonDown = false;
        if (_btnIsHold) {
            _btnIsHold = false;
            return;
        }
        _clickCount++;
        _lastReleaseMs = now;
        if (_clickCount == 3) {
            _clickCount = 0;
            Serial.println("[Flight] Emergency (triple-click)");
            enterState(FlightState::Emergency);
            return;
        }
    }

    if (_clickCount > 0 && !_buttonDown &&
        (now - _lastReleaseMs) >= DOUBLE_CLICK_MS) {
        uint8_t count = _clickCount;
        _clickCount = 0;
        if (count == 2) handleDoubleClick(wifiOk);
        if (count == 1 && _state == FlightState::Flying) {
            _yawEnabled = !_yawEnabled;
            Serial.printf("[Flight] Yaw %s\n", _yawEnabled ? "ON" : "OFF");
        }
    }
}

void FlightController::handleDoubleClick(bool wifiOk) {
    switch (_state) {
        case FlightState::Idle:
            if (!wifiOk) {
                Serial.println("[Flight] Button ignored — WiFi not connected");
                return;
            }
            // Drones that don't need CaliGyro/Unlock/TakeOff go straight to Flying.
            enterState(_deps.drone.supportsArmSequence()
                       ? FlightState::Calibrating : FlightState::Flying);
            break;
        case FlightState::Flying:
            enterState(FlightState::Landing);
            break;
        default:
            break;
    }
}

void FlightController::runState(const ImuData& imu, bool wifiOk) {
    uint32_t elapsed = millis() - _stateEnteredMs;

    if (!wifiOk && _state != FlightState::Idle && _state != FlightState::Emergency) {
        Serial.println("[Flight] WiFi lost — emergency stop");
        enterState(FlightState::Emergency);
        return;
    }

    switch (_state) {
        case FlightState::Idle:
            _deps.drone.setIdle();
            break;

        case FlightState::Calibrating:
            _deps.drone.setControl(0x80, 0x80, 0x00, 0x80, DroneCmd::CaliGyro);
            if (elapsed >= CALI_DURATION_MS) {
                enterState(FlightState::Arming);
            }
            break;

        case FlightState::Arming:
            if (elapsed < UNLOCK_DURATION_MS) {
                _deps.drone.setControl(0x80, 0x80, 0x80, 0x80, DroneCmd::Unlock);
            } else if (elapsed < UNLOCK_DURATION_MS + TAKEOFF_DURATION_MS) {
                _deps.drone.setControl(0x80, 0x80, 0x80, 0x80, DroneCmd::TakeOff);
            } else {
                enterState(FlightState::Flying);
            }
            break;

        case FlightState::Flying: {
            if (elapsed < ACCEL_LOCKOUT_MS) {
                _deps.drone.setControl(0x80, 0x80, 0x80, 0x80, DroneCmd::None);
                break;
            }
            // BT gamepad lost during flight → emergency stop.
            if (_mode == OperationMode::BluetoothControl && !_lastGamepadAxes.connected) {
                Serial.println("[Flight] BT lost during flight — emergency stop");
                enterState(FlightState::Emergency);
                return;
            }
            DroneState cs;
            if (_mode == OperationMode::BluetoothControl) {
                _gamepad.update(_lastGamepadAxes, cs);
                // Altitude-hold drones (e.g. FLOW-WIFI): direct throttle mapping.
                // 0x80 = hover, stick UP → climb, stick DOWN → descend, release → hover.
                if (!_deps.drone.supportsArmSequence()) {
                    float rate = _lastGamepadAxes.throttleUp - _lastGamepadAxes.throttleDown;
                    if (rate >  1.0f) rate =  1.0f;
                    if (rate < -1.0f) rate = -1.0f;
                    cs.throttle = (uint8_t)(0x80 + (int)(rate * 0x7F));
                }
            } else {
                if (_btnIsHold) {
                    float delta = _btnHoldIsDown ? -THROTTLE_HOLD_RATE : THROTTLE_HOLD_RATE;
                    _accel.adjustThrottle(delta);
                }
                _accel.update(imu, cs);
            }
            if (cs.active) {
                uint8_t cmd = DroneCmd::None;
                if (_headless) cmd |= DroneCmd::Headless;
                if (millis() < _oneShotUntil) {
                    cmd |= _oneShotCmd;
                } else {
                    _oneShotCmd   = DroneCmd::None;
                    _oneShotUntil = 0;
                }
                _deps.drone.setControl(cs.roll, cs.pitch, cs.throttle,
                                      _yawEnabled ? cs.yaw : (uint8_t)0x80,
                                      cmd);
            } else {
                _deps.drone.setIdle();
            }
            break;
        }

        case FlightState::Landing:
            if (_deps.drone.supportsArmSequence()) {
                _deps.drone.setControl(0x80, 0x80, 0x80, 0x80, DroneCmd::Land);
            } else {
                // FLOW-WIFI: TakeOff toggle acts as land; send for 1 s then idle.
                _deps.drone.setControl(0x80, 0x80, 0x80, 0x80,
                                       elapsed < 1000 ? DroneCmd::TakeOff : DroneCmd::None);
            }
            if (elapsed >= LANDING_DURATION_MS) {
                enterState(FlightState::Idle);
            }
            break;

        case FlightState::Emergency:
            _deps.drone.setControl(0x80, 0x80, 0x80, 0x80, DroneCmd::EmergStop);
            enterState(FlightState::Idle);
            break;
    }

    _stateFirstFrame = false;
}

void FlightController::handleGamepadButtons(bool wifiOk) {
    uint16_t curr    = _lastGamepadAxes.buttons;
    uint16_t pressed = curr & ~_prevGpButtons;   // rising edges only
    _prevGpButtons   = curr;
    if (!pressed || !_lastGamepadAxes.connected) return;

    // X — emergency stop, highest priority, any state
    if (pressed & GamepadBtn::X) {
        Serial.println("[Flight] Emergency (X)");
        enterState(FlightState::Emergency);
        return;
    }
    // A — arm + takeoff (Idle + WiFi only)
    if ((pressed & GamepadBtn::A) && _state == FlightState::Idle && wifiOk) {
        Serial.println("[Flight] Arm (A)");
        enterState(_deps.drone.supportsArmSequence()
                   ? FlightState::Calibrating : FlightState::Flying);
        return;
    }
    // B — land (Flying only)
    if ((pressed & GamepadBtn::B) && _state == FlightState::Flying) {
        Serial.println("[Flight] Land (B)");
        enterState(FlightState::Landing);
        return;
    }
    // D-pad RIGHT — manual arm (grey drone only, Idle + WiFi):
    // enters Flying without auto-takeoff command; user does double-UP to lift off low.
    if ((pressed & GamepadBtn::DpadRight) && _state == FlightState::Idle && wifiOk
        && !_deps.drone.supportsArmSequence()) {
        Serial.println("[Flight] Manual arm (DpadRight) — use double-UP to lift off");
        enterState(FlightState::Flying);
        _oneShotCmd   = DroneCmd::None;  // cancel auto-takeoff fired by enterState
        _oneShotUntil = 0;
        return;
    }

    if (_state != FlightState::Flying) return;

    // Y — flip
    if (pressed & GamepadBtn::Y) {
        _oneShotCmd   = DroneCmd::Flip;
        _oneShotUntil = millis() + 200;
        Serial.println("[Flight] Flip (Y)");
    }
    // LT — lock motors
    if (pressed & GamepadBtn::LT) {
        _oneShotCmd   = DroneCmd::Lock;
        _oneShotUntil = millis() + 200;
        Serial.println("[Flight] Lock (LT)");
    }
    // R1 — unlock motors
    if (pressed & GamepadBtn::R1) {
        _oneShotCmd   = DroneCmd::Unlock;
        _oneShotUntil = millis() + 200;
        Serial.println("[Flight] Unlock (R1)");
    }

    // D-pad buttons are in the left stick zone — guard against accidental
    // triggers while the stick is deflected (thumb can't be on both).
    bool stickClear = (fabsf(_lastGamepadAxes.roll)  < 0.15f &&
                       fabsf(_lastGamepadAxes.pitch) < 0.15f);
    if (!stickClear) return;

    if (pressed & GamepadBtn::DpadUp) {
        _headless = !_headless;
        Serial.printf("[Flight] Headless %s (DpadUp)\n", _headless ? "ON" : "OFF");
    }
    if (pressed & GamepadBtn::DpadDown) {
        _oneShotCmd   = DroneCmd::CaliGyro;
        _oneShotUntil = millis() + 200;
        Serial.println("[Flight] CaliGyro (DpadDown)");
    }
    // DpadLeft / DpadRight — spare, no action
}
