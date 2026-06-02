/*
 *  Copyright (C) since 2026 by PopolonY2k and Leidson Campos Alves Ferreira.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include <IPAddress.h>
#include "drone_protocol_base.h"

/**
 * @brief Eachine WIFI_8K_ UDP control protocol driver (black drone).
 *
 * Encodes and sends the 8-byte E58-format control packets to the drone on
 * UDP port 8090 at 25 Hz. Sends a keepalive (AA…55) at ~790 ms intervals
 * when no active control is in progress.
 *
 * The drone must first be switched from RF mode to WiFi mode by sending
 * the app-mode activation command (0x42 0x76) to port 8080. Call
 * beginAppModeEntry() once WiFi connects; it handles the settle delay
 * and retry window automatically.
 *
 * Packet format: [ 0x66 | Roll | Pitch | Throttle | Yaw | Cmd | XOR | 0x99 ]
 * XOR is computed over bytes 1–4 (Roll ^ Pitch ^ Throttle ^ Yaw).
 */
class DroneProtocol : public DroneProtocolBase {
public:
    void begin()   override;
    void update()  override;
    void setControl(uint8_t roll, uint8_t pitch, uint8_t throttle,
                    uint8_t yaw, uint8_t cmd = DroneCmd::None) override;
    void setIdle() override;

    void beginAppModeEntry() override;
    void exitAppMode()       override;
    const DroneState& state()  const override { return _state; }
    bool appModeOk()           const override { return _appModeOk; }

private:
    void sendControlPacket();
    void sendKeepalive();
    int  sendToVideoPort(uint8_t byte1, uint8_t byte2);

    WiFiUDP    _udp;
    DroneState _state;
    uint32_t   _lastControlMs     = 0;
    uint32_t   _lastKeepaliveMs   = 0;
    bool       _appModePending    = false;
    uint32_t   _appModePendingMs  = 0;
    bool       _appModeActive     = false;
    uint32_t   _appModeStartMs    = 0;
    uint32_t   _appModeLastSendMs = 0;
    bool       _appModeOk         = false;

    static const IPAddress    DRONE_IP;
    static constexpr uint16_t CONTROL_PORT          = 8090;
    static constexpr uint16_t VIDEO_PORT            = 8080;
    static constexpr uint32_t CONTROL_INTERVAL_MS   =   40;
    static constexpr uint32_t KEEPALIVE_INTERVAL_MS =  790;
    static constexpr uint32_t APP_MODE_SETTLE_MS    =  500;
    static constexpr uint32_t APP_MODE_ENTRY_MS     = 1000;
    static constexpr uint32_t APP_MODE_INTERVAL_MS  =  150;
};
