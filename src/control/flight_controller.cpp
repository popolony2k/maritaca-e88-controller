#include <Arduino.h>
#include "flight_controller.h"

const char* flightStateName(FlightState s) {
    switch (s) {
        case FlightState::Idle:        return "IDLE";
        case FlightState::Calibrating: return "CALIB";
        case FlightState::Arming:      return "ARMING";
        case FlightState::Flying:    return "FLYING";
        case FlightState::Landing:   return "LANDING";
        case FlightState::Emergency: return "EMERGENCY";
        default:                     return "?";
    }
}

FlightController::FlightController(FlightDeps deps) : _deps(deps) {}

void FlightController::begin() {
    _accel.begin();
    _state           = FlightState::Idle;
    _stateEnteredMs  = millis();
    _stateFirstFrame = true;
    // Do NOT call enterState() — exitAppMode() must not fire at boot.
}

void FlightController::update(const ImuData& imu, bool wifiOk) {
    handleButton(wifiOk);
    runState(imu);
}

void FlightController::enterState(FlightState s, bool sendModeCmd) {
    _state           = s;
    _stateEnteredMs  = millis();
    _stateFirstFrame = true;

    _accel.setEnabled(s == FlightState::Flying);
    if (s == FlightState::Flying) {
        _accel.begin();
    }

    if (sendModeCmd && s == FlightState::Idle) {
        _deps.drone.exitAppMode();
    }

    Serial.printf("[Flight] -> %s\n", flightStateName(s));
}

void FlightController::handleButton(bool wifiOk) {
    if (_deps.button.pressedFor(LONG_PRESS_MS) && !_longPressHandled) {
        _longPressHandled = true;
        enterState(FlightState::Emergency);
        return;
    }

    if (!_deps.button.wasReleased()) return;

    // Release after a long press — reset flag, don't treat as short press
    if (_longPressHandled) {
        _longPressHandled = false;
        return;
    }

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
            // Both sticks down equivalent: throttle=0x00, CaliGyro flag.
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
            _accel.update(imu, cs);
            if (cs.active) {
                _deps.drone.setControl(cs.roll, cs.pitch, cs.throttle, cs.yaw, DroneCmd::None);
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
