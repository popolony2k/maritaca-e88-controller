#pragma once
#include <Arduino.h>

struct GamepadAxes {
    bool  connected    = false;
    float roll         = 0.0f;  // [-1, 1] left stick X
    float pitch        = 0.0f;  // [-1, 1] left stick Y
    float yaw          = 0.0f;  // [-1, 1] right stick X
    float throttleUp   = 0.0f;  // [0, 1]  ZR button
    float throttleDown = 0.0f;  // [0, 1]  ZL button
};
