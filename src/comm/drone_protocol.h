#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include <IPAddress.h>

namespace DroneCmd {
    constexpr uint8_t None      = 0x00;
    constexpr uint8_t TakeOff   = 0x01;
    constexpr uint8_t Land      = 0x02;
    constexpr uint8_t EmergStop = 0x04;
    constexpr uint8_t Flip      = 0x08;
    constexpr uint8_t Headless  = 0x10;
    constexpr uint8_t Lock      = 0x20;
    constexpr uint8_t Unlock    = 0x40;
    constexpr uint8_t CaliGyro  = 0x80;
}

struct DroneState {
    uint8_t roll     = 0x80;
    uint8_t pitch    = 0x80;
    uint8_t throttle = 0x80;
    uint8_t yaw      = 0x80;
    uint8_t cmd      = DroneCmd::None;
    bool    active   = false;
};

class DroneProtocol {
public:
    void begin();
    void update();

    void setControl(uint8_t roll, uint8_t pitch, uint8_t throttle,
                    uint8_t yaw, uint8_t cmd = DroneCmd::None);
    void setIdle();

    // Called once when WiFi connects; sends 42 76 to port 8080 repeatedly for
    // APP_MODE_ENTRY_MS, then stops.  exitAppMode() sends 42 77 and cancels.
    void beginAppModeEntry();
    void exitAppMode();

    const DroneState& state() const { return _state; }
    bool appModeOk() const { return _appModeOk; }

private:
    void sendControlPacket();
    void sendKeepalive();
    int  sendToVideoPort(uint8_t byte1, uint8_t byte2);

    WiFiUDP   _udp;
    DroneState _state;
    uint32_t  _lastControlMs     = 0;
    uint32_t  _lastKeepaliveMs   = 0;
    bool      _appModePending    = false; // waiting for settle delay
    uint32_t  _appModePendingMs  = 0;
    bool      _appModeActive     = false; // actively sending retries
    uint32_t  _appModeStartMs    = 0;
    uint32_t  _appModeLastSendMs = 0;
    bool      _appModeOk         = false; // last send was successful

    static const IPAddress    DRONE_IP;
    static constexpr uint16_t CONTROL_PORT            = 8090;
    static constexpr uint16_t VIDEO_PORT              = 8080;
    static constexpr uint32_t CONTROL_INTERVAL_MS     = 40;   // 25 Hz
    static constexpr uint32_t KEEPALIVE_INTERVAL_MS   = 790;
    static constexpr uint32_t APP_MODE_SETTLE_MS      =  500; // wait after WiFi connect
    static constexpr uint32_t APP_MODE_ENTRY_MS       = 1000; // total retry window
    static constexpr uint32_t APP_MODE_INTERVAL_MS    =  150; // resend interval
};
