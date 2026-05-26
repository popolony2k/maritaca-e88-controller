#pragma once
#include <Arduino.h>
#include "../hal/hal.h"
#include "../comm/drone_protocol.h"
#include "../imu/accelerometer.h"
#include "../control/flight_controller.h"
#include "../control/operation_mode.h"
#include "../bt/ble_gamepad.h"

class Display {
public:
    explicit Display(const DisplayHal& hal);
    void begin();
    void markDirty();  // force full redraw on next update()

    void update(bool wifiConnected, FlightState flightState,
                const DroneState& drone, const ImuData& imu,
                int batteryLevel, bool charging);

    void drawModeSelect(OperationMode selected, int secondsLeft);
    void drawBtStatus(BleStatus status, bool wifiOk, int batteryLevel, bool charging);

private:
    void drawStatusBar(bool wifiConnected, FlightState flightState,
                       int batteryLevel, bool charging);
    void drawControlBars(const DroneState& drone);
    void drawImu(const ImuData& imu);
    void label(int x, int y, const char* txt, uint16_t color = Rgb565::DarkGrey);
    void drawBar(int x, int y, int w, int h, uint8_t value, uint16_t color);
    void drawBarV(int x, int y, int w, int h, uint8_t value, uint16_t color);

    const DisplayHal& _hal;
    bool _needsFullRedraw = true;
    bool _modeSelectReady = false;
    bool _btScreenReady   = false;
};
