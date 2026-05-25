#pragma once
#include <Arduino.h>
#include "../hal/hal.h"

struct ImuData {
    float ax = 0, ay = 0, az = 0;  // g
    float gx = 0, gy = 0, gz = 0;  // deg/s
    bool  valid = false;
};

class Accelerometer {
public:
    void begin();
    void update(const ImuHal& hal);
    const ImuData& data() const { return _data; }

private:
    ImuData  _data;
    uint32_t _lastUpdateMs = 0;

    static constexpr uint32_t UPDATE_INTERVAL_MS = 20;  // 50 Hz
};
