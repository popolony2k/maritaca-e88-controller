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
    GamepadConfig gamepadConfig()    const override {
        // Optical-flow flight controller handles its own smoothing — no slew lag needed.
        // slewRate=128 reaches any target in ≤1 frame (40 ms) vs 630 ms with the default 8.
        return GamepadConfig(0.12f, 0.40f, 128.0f);
    }

private:
    void sendPacket();
    void sendKeepalive();

    WiFiUDP    _udp;
    DroneState _state;
    uint16_t   _seq              = 0;
    uint32_t   _lastSendMs       = 0;
    uint32_t   _lastKeepaliveMs  = 0;

    static const IPAddress    DRONE_IP;
    static constexpr uint16_t CONTROL_PORT        = 8800;
    static constexpr uint16_t KEEPALIVE_PORT      = 7099;
    static constexpr uint32_t CONTROL_INTERVAL_MS   =   40;  ///< 25 Hz.
    static constexpr uint32_t IDLE_HEARTBEAT_MS     = 2000; ///< 0.5 Hz — keeps 8800 session alive in Idle without re-arm.
    static constexpr uint32_t KEEPALIVE_INTERVAL_MS = 1000; ///< 1 Hz — matches KY UFO app.

    // ---- Packet sizes ------------------------------------------------------
    static constexpr size_t  PACKET_SIZE       = 88; ///< Total outer packet size (bytes).
    static constexpr size_t  INNER_OFFSET      = 18; ///< Inner packet offset within the outer packet.
    static constexpr size_t  INNER_PACKET_SIZE = 20; ///< Inner control packet size (bytes).

    // ---- Outer header bytes (18 bytes) --------------------------------------
    static constexpr uint8_t OUTER_SYNC     = 0xEF; ///< Outer envelope sync byte.
    static constexpr uint8_t OUTER_VERSION  = 0x02; ///< Outer envelope version/type byte.
    static constexpr uint8_t OUTER_LEN_LO   = 0x58; ///< Payload length LE low byte (88 decimal).
    static constexpr uint8_t OUTER_LEN_HI   = 0x00; ///< Payload length LE high byte.
    static constexpr uint8_t OUTER_FLAG_A   = 0x02; ///< Outer flags byte ("02 02" pair, byte 1 of 2).
    static constexpr uint8_t OUTER_FLAG_B   = 0x02; ///< Outer flags byte ("02 02" pair, byte 2 of 2).
    static constexpr uint8_t OUTER_FLAG_C   = 0x00; ///< Outer flags byte ("00 01" pair, byte 1 of 2).
    static constexpr uint8_t OUTER_FLAG_D   = 0x01; ///< Outer flags byte ("00 01" pair, byte 2 of 2).
    static constexpr uint8_t INNER_LEN_LO   = 0x14; ///< Inner packet length LE low byte (20 decimal).
    static constexpr uint8_t INNER_LEN_HI   = 0x00; ///< Inner packet length LE high byte.

    // ---- Inner control packet bytes (20 bytes at INNER_OFFSET) ---------------
    static constexpr uint8_t INNER_SYNC        = 0x66; ///< Inner packet sync byte — same as E58 header.
    static constexpr uint8_t INNER_LENGTH_BYTE = 0x14;  ///< Inner packet's own length field (20 decimal).
    static constexpr uint8_t INNER_CONST_FLAG  = 0x02;  ///< Constant flag byte; also XOR'd into the checksum.
    static constexpr uint8_t INNER_FOOTER      = 0x99;  ///< Inner packet footer — same as E58 footer.

    // ---- Secondary keepalive (port 7099) --------------------------------------
    static constexpr uint8_t KEEPALIVE_BYTE = 0x01; ///< Both bytes of the [0x01, 0x01] keepalive packet.
};
