#pragma once
#include <Arduino.h>
#include "../bt/gamepad_axes.h"
#include "../comm/drone_protocol.h"

class GamepadController {
public:
    void begin();
    void update(const GamepadAxes& axes, DroneState& out);

private:
    static uint8_t mapAxis(float value, float deadZone, float expo);

    float _throttle     = 128.0f;
    float _currentRoll  = 128.0f;
    float _currentPitch = 128.0f;
    float _currentYaw   = 128.0f;

    static constexpr float DEAD_ZONE         = 0.12f;
    static constexpr float EXPO              = 0.4f;
    static constexpr float SLEW_RATE         = 8.0f;
    static constexpr float THROTTLE_RATE_MAX = 2.0f;  // units/frame at full trigger press
    static constexpr float THROTTLE_INIT     = 128.0f;
};
