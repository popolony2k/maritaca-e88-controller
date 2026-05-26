#pragma once
#include <Arduino.h>
#include "accel_controller.h"
#include "gamepad_controller.h"
#include "operation_mode.h"
#include "../comm/drone_protocol.h"
#include "../imu/accelerometer.h"
#include "../bt/gamepad_axes.h"
#include "../hal/hal.h"

enum class FlightState {
    Idle,
    Calibrating,
    Arming,
    Flying,
    Landing,
    Emergency,
};

const char* flightStateName(FlightState s);

struct FlightDeps {
    const ButtonHal& button;
    DroneProtocol&   drone;
};

class FlightController {
public:
    explicit FlightController(FlightDeps deps);
    void begin();
    void setMode(OperationMode m) { _mode = m; }

    void update(const ImuData& imu, const GamepadAxes& gamepad, bool wifiOk);

    FlightState state() const { return _state; }

private:
    void enterState(FlightState s, bool sendModeCmd = true);
    void handleButton(bool wifiOk);
    void handleDoubleClick(bool wifiOk);
    void runState(const ImuData& imu);

    FlightDeps      _deps;
    OperationMode   _mode            = OperationMode::AccelControl;
    FlightState     _state           = FlightState::Idle;
    uint32_t        _stateEnteredMs  = 0;
    bool            _stateFirstFrame = true;

    // Button gesture state
    uint8_t         _clickCount      = 0;
    uint32_t        _lastReleaseMs   = 0;
    bool            _buttonDown      = false;
    bool            _btnIsHold       = false;
    bool            _btnHoldIsDown   = false;
    bool            _yawEnabled      = false;

    GamepadAxes       _lastGamepadAxes;
    AccelController   _accel;
    GamepadController _gamepad;

    static constexpr uint32_t DOUBLE_CLICK_MS     =  300;
    static constexpr uint32_t HOLD_THRESHOLD_MS   =  500;
    static constexpr uint32_t CALI_DURATION_MS    = 1500;
    static constexpr uint32_t UNLOCK_DURATION_MS  =  500;
    static constexpr uint32_t TAKEOFF_DURATION_MS =  500;
    static constexpr uint32_t ACCEL_LOCKOUT_MS    = 2000;
    static constexpr uint32_t LANDING_DURATION_MS = 2000;
    static constexpr float    THROTTLE_HOLD_RATE  =  1.0f;
};
