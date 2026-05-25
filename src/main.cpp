#include <Arduino.h>
#include "hal/m5atoms3.h"
#include "control/operation_mode.h"
#include "control/flight_controller.h"
#include "comm/wifi_manager.h"
#include "comm/drone_protocol.h"
#include "imu/accelerometer.h"
#include "ui/display.h"

static constexpr char DRONE_SSID[] = "WIFI_8K_Wf48702";

static OperationModeManager modeManager;
static WifiManager          wifi;
static DroneProtocol        drone;
static Accelerometer        imu;
static FlightController     flight({ kButton, drone });
static Display              display(kDisplay);

static uint32_t _lastDisplayMs = 0;
static constexpr uint32_t DISPLAY_INTERVAL_MS = 100;  // 10 Hz
static bool _prevWifiConnected = false;

void setup() {
    kBoard.begin();

    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);  // HWCDC: don't stall if monitor not attached
    Serial.println("[Boot] setup start");

    display.begin();
    wifi.begin(DRONE_SSID);
    drone.begin();
    imu.begin();
    flight.begin();
    Serial.printf("[Battery] level=%d charging=%d\n",
                  kBoard.getBatteryLevel(), (int)kBoard.isCharging());
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

    if (modeManager.current() == OperationMode::AccelControl) {
        flight.update(imu.data(), wifi.isConnected());
    }

    if (wifi.isConnected()) {
        drone.update();
    }

    uint32_t now = millis();
    if (now - _lastDisplayMs >= DISPLAY_INTERVAL_MS) {
        _lastDisplayMs = now;
        display.update(wifi.isConnected(), flight.state(),
                       drone.state(), imu.data(),
                       kBoard.getBatteryLevel(), kBoard.isCharging());
    }
}
