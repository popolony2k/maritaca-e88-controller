#pragma once
#include <Arduino.h>
#include "accel_controller.h"
#include "../comm/drone_protocol.h"
#include "../imu/accelerometer.h"
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

// Stable dependencies injected once at construction — objects that never change.
struct FlightDeps {
    const ButtonHal& button;
    DroneProtocol&   drone;
};

class FlightController {
public:
    explicit FlightController(FlightDeps deps);
    void begin();

    // Call every loop(). wifiOk must be true for button to arm.
    void update(const ImuData& imu, bool wifiOk);

    FlightState state() const { return _state; }

private:
    void enterState(FlightState s, bool sendModeCmd = true);
    void handleButton(bool wifiOk);
    void runState(const ImuData& imu);

    FlightDeps      _deps;
    FlightState     _state            = FlightState::Idle;
    uint32_t        _stateEnteredMs   = 0;
    bool            _longPressHandled = false;
    bool            _stateFirstFrame  = true;
    AccelController _accel;

    static constexpr uint32_t CALI_DURATION_MS     = 1500;
    static constexpr uint32_t UNLOCK_DURATION_MS  =  500;
    static constexpr uint32_t TAKEOFF_DURATION_MS =  500;
    static constexpr uint32_t LANDING_DURATION_MS = 2000;
    static constexpr uint32_t LONG_PRESS_MS       = 2000;
};
