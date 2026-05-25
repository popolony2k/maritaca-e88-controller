#include <Arduino.h>
#include "drone_protocol.h"

const IPAddress DroneProtocol::DRONE_IP(192, 168, 4, 153);

void DroneProtocol::begin() {
    _udp.begin(0);  // bind to any local port
}

void DroneProtocol::update() {
    uint32_t now = millis();

    if (_appModePending) {
        if (now - _appModePendingMs >= APP_MODE_SETTLE_MS) {
            _appModePending    = false;
            _appModeActive     = true;
            _appModeStartMs    = now;
            _appModeLastSendMs = now;
            _udp.stop();
            _udp.begin(0);
            int ok = sendToVideoPort(0x42, 0x76);
            _appModeOk = (ok != 0);
            Serial.printf("[Drone] appMode first send -> :8080 %s\n", ok ? "OK" : "FAIL");
        }
    } else if (_appModeActive) {
        if (now - _appModeLastSendMs >= APP_MODE_INTERVAL_MS) {
            _appModeLastSendMs = now;
            int ok = sendToVideoPort(0x42, 0x76);
            if (ok) _appModeOk = true;
            Serial.printf("[Drone] appMode retry -> :8080 %s\n", ok ? "OK" : "FAIL");
        }
        if (now - _appModeStartMs >= APP_MODE_ENTRY_MS) {
            _appModeActive = false;
            Serial.println("[Drone] appMode entry done");
        }
    }

    if (_state.active) {
        if (now - _lastControlMs >= CONTROL_INTERVAL_MS) {
            _lastControlMs = now;
            sendControlPacket();
        }
    } else if (!_appModePending && !_appModeActive) {
        if (now - _lastKeepaliveMs >= KEEPALIVE_INTERVAL_MS) {
            _lastKeepaliveMs = now;
            sendKeepalive();
        }
    }
}

void DroneProtocol::setControl(uint8_t roll, uint8_t pitch, uint8_t throttle,
                                uint8_t yaw, uint8_t cmd) {
    _state.roll     = roll;
    _state.pitch    = pitch;
    _state.throttle = throttle;
    _state.yaw      = yaw;
    _state.cmd      = cmd;
    _state.active   = true;
}

void DroneProtocol::setIdle() {
    _state.roll     = 0x80;
    _state.pitch    = 0x80;
    _state.throttle = 0x80;
    _state.yaw      = 0x80;
    _state.cmd      = DroneCmd::None;
    _state.active   = false;
}

void DroneProtocol::sendControlPacket() {
    uint8_t pkt[8];
    pkt[0] = 0x66;
    pkt[1] = _state.roll;
    pkt[2] = _state.pitch;
    pkt[3] = _state.throttle;
    pkt[4] = _state.yaw;
    pkt[5] = _state.cmd;
    pkt[6] = _state.roll ^ _state.pitch ^ _state.throttle ^ _state.yaw;
    pkt[7] = 0x99;

    _udp.beginPacket(DRONE_IP, CONTROL_PORT);
    _udp.write(pkt, sizeof(pkt));
    _udp.endPacket();
}

void DroneProtocol::sendKeepalive() {
    uint8_t pkt[8] = {0xAA, 0x80, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55};
    _udp.beginPacket(DRONE_IP, CONTROL_PORT);
    _udp.write(pkt, sizeof(pkt));
    _udp.endPacket();
}

void DroneProtocol::beginAppModeEntry() {
    _appModePending    = true;
    _appModePendingMs  = millis();
    _appModeActive     = false;
    _appModeOk         = false;
    _lastKeepaliveMs   = millis();  // delay keepalive until after 42 76 is sent
    Serial.printf("[Drone] appMode pending (settle %u ms)\n", APP_MODE_SETTLE_MS);
}

void DroneProtocol::exitAppMode() {
    _appModePending = false;
    _appModeActive  = false;
    _appModeOk      = false;
    sendToVideoPort(0x42, 0x77);
    Serial.println("[Drone] appMode exit -> :8080");
}

int DroneProtocol::sendToVideoPort(uint8_t byte1, uint8_t byte2) {
    uint8_t pkt[2] = {byte1, byte2};
    if (!_udp.beginPacket(DRONE_IP, VIDEO_PORT)) return 0;
    _udp.write(pkt, sizeof(pkt));
    return _udp.endPacket();
}
