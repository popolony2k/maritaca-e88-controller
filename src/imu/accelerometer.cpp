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
