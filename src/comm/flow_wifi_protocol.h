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
 * @brief FLOW-WIFI drone UDP control protocol driver (grey E88 clone).
 *
 * Sends 88-byte UDP packets to 192.168.169.1:8800 at 25 Hz.
 *
 * Packet layout (88 bytes total):
 *
 *   Outer header (18 bytes):
 *     EF 02 | [len LE16=0x0058] | 02 02 | 00 01 | 00 00 | 00 00
 *     | [seq LE16] | 00 00 | [inner_len LE16=0x0014]
 *
 *   Inner control packet (20 bytes at offset 18):
 *     66 14 | Roll | Pitch | Throttle | Yaw | Cmd | 02
 *     | [10 × 0x00] | Roll^Pitch^Throttle^Yaw^0x02 | 99
 *
 *   Trailing zeros (50 bytes at offset 38).
 *
 * All axis values: 0x00 = min, 0x80 = neutral, 0xFF = max.
 * No app-mode handshake required — drone accepts control on connect.
 */
class FlowWifiProtocol : public DroneProtocolBase {
public:
    void begin()   override;
    void update()  override;

    void setControl(uint8_t roll, uint8_t pitch, uint8_t throttle,
                    uint8_t yaw, uint8_t cmd = DroneCmd::None) override;
    void setIdle() override;

    const DroneState& state()        const override { return _state; }
    bool appModeOk()                 const override { return true; }
    bool supportsArmSequence()       const override { return false; }

private:
    void sendPacket();

    WiFiUDP    _udp;
    DroneState _state;
    uint16_t   _seq         = 0;
    uint32_t   _lastSendMs  = 0;

    static const IPAddress    DRONE_IP;
    static constexpr uint16_t CONTROL_PORT        = 8800;
    static constexpr uint32_t CONTROL_INTERVAL_MS =   40; ///< 25 Hz.
};
