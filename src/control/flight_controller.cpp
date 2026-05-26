#include <Arduino.h>
#include "flight_controller.h"

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
    runState(imu);
}

void FlightController::enterState(FlightState s, bool sendModeCmd) {
    _state           = s;
    _stateEnteredMs  = millis();
    _stateFirstFrame = true;
    _btnIsHold       = false;
    _buttonDown      = false;
    _clickCount      = 0;

    _accel.setEnabled(s == FlightState::Flying);
    if (s == FlightState::Flying) {
        _accel.begin();
        _gamepad.begin();
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
            enterState(FlightState::Calibrating);
            break;
        case FlightState::Flying:
            enterState(FlightState::Landing);
            break;
        default:
            break;
    }
}

void FlightController::runState(const ImuData& imu) {
    uint32_t elapsed = millis() - _stateEnteredMs;

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
            DroneState cs;
            if (_mode == OperationMode::BluetoothControl) {
                _gamepad.update(_lastGamepadAxes, cs);
            } else {
                if (_btnIsHold) {
                    float delta = _btnHoldIsDown ? -THROTTLE_HOLD_RATE : THROTTLE_HOLD_RATE;
                    _accel.adjustThrottle(delta);
                }
                _accel.update(imu, cs);
            }
            if (cs.active) {
                _deps.drone.setControl(cs.roll, cs.pitch, cs.throttle,
                                      _yawEnabled ? cs.yaw : (uint8_t)0x80,
                                      DroneCmd::None);
            } else {
                _deps.drone.setIdle();
            }
            break;
        }

        case FlightState::Landing:
            _deps.drone.setControl(0x80, 0x80, 0x80, 0x80, DroneCmd::Land);
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
