#include <Arduino.h>
#include "hal/m5atoms3.h"
#include "control/operation_mode.h"
#include "control/flight_controller.h"
#include "comm/wifi_manager.h"
#include "comm/drone_protocol.h"
#include "imu/accelerometer.h"
#include "ui/display.h"
#include "bt/ble_gamepad.h"

static constexpr char DRONE_SSID[] = "WIFI_8K_Wf48702";

static OperationModeManager modeManager;
static WifiManager          wifi;
static DroneProtocol        drone;
static Accelerometer        imu;
static FlightController     flight({ kButton, drone });
static BleGamepad           gamepad;
static Display              display(kDisplay);

static uint32_t _lastDisplayMs = 0;
static constexpr uint32_t DISPLAY_INTERVAL_MS = 100;  // 10 Hz
static bool _prevWifiConnected = false;

static uint32_t _btConnectedMs    = 0;
static bool     _btWasConnected   = false;
static bool     _prevShowBtScreen = false;

static constexpr uint32_t MODE_SELECT_MS = 3000;

static void runModeSelection() {
    OperationMode sel       = OperationMode::BluetoothControl;  // default
    uint32_t      selectStart = millis();
    uint32_t      lastDrawMs  = 0;

    while (true) {
        kBoard.update();

        if (kButton.wasReleased()) {
            sel = (sel == OperationMode::BluetoothControl)
                  ? OperationMode::AccelControl
                  : OperationMode::BluetoothControl;
            selectStart = millis();  // reset timer on selection change
        }

        uint32_t elapsed  = millis() - selectStart;
        int      secsLeft = (int)((MODE_SELECT_MS - elapsed) / 1000) + 1;
        if (secsLeft > 3) secsLeft = 3;
        if (secsLeft < 0) secsLeft = 0;

        uint32_t now = millis();
        if (now - lastDrawMs >= 100) {
            lastDrawMs = now;
            display.drawModeSelect(sel, secsLeft);
        }

        if (elapsed >= MODE_SELECT_MS) break;
    }

    modeManager.setMode(sel);
    Serial.printf("[Boot] Mode: %s\n",
                  sel == OperationMode::BluetoothControl ? "BT GAMEPAD" : "ACCEL TILT");
}

void setup() {
    kBoard.begin();

    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    Serial.println("[Boot] setup start");

    display.begin();

    runModeSelection();

    wifi.begin(DRONE_SSID);
    drone.begin();
    imu.begin();

    flight.setMode(modeManager.current());
    flight.begin();

    if (modeManager.current() == OperationMode::BluetoothControl) {
        gamepad.begin();  // init BLE and start scanning
    }
}

void loop() {
    kBoard.update();
    wifi.update();
    imu.update(kImu);

    bool wifiNow = wifi.isConnected();
    if (wifiNow && !_prevWifiConnected) {
        Serial.println("[Main] WiFi connected — starting app mode entry");
        drone.beginAppModeEntry();
    }
    _prevWifiConnected = wifiNow;

    GamepadAxes gpAxes{};
    if (modeManager.current() == OperationMode::BluetoothControl) {
        gamepad.update();
        gpAxes = gamepad.axes();
    }

    flight.update(imu.data(), gpAxes, wifi.isConnected());

    if (wifi.isConnected()) {
        drone.update();
    }

    bool gpConnected = gamepad.axes().connected;
    if (gpConnected && !_btWasConnected) _btConnectedMs = millis();
    _btWasConnected = gpConnected;

    bool showBtScreen = (modeManager.current() == OperationMode::BluetoothControl)
                        && (flight.state() == FlightState::Idle)
                        && (!gpConnected || (millis() - _btConnectedMs < 1500));

    if (_prevShowBtScreen && !showBtScreen) display.markDirty();
    _prevShowBtScreen = showBtScreen;

    uint32_t now = millis();
    if (now - _lastDisplayMs >= DISPLAY_INTERVAL_MS) {
        _lastDisplayMs = now;
        if (showBtScreen) {
            display.drawBtStatus(gamepad.status(),
                                 wifi.isConnected(),
                                 kBoard.getBatteryLevel(),
                                 kBoard.isCharging());
        } else {
            display.update(wifi.isConnected(), flight.state(),
                           drone.state(), imu.data(),
                           kBoard.getBatteryLevel(), kBoard.isCharging());
        }
    }
}
