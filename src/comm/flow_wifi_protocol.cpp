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
#include <Arduino.h>
#include "flow_wifi_protocol.h"

const IPAddress FlowWifiProtocol::DRONE_IP(192, 168, 169, 1);

void FlowWifiProtocol::begin() {
    _udp.begin(0);
    _seq             = 0;
    _lastSendMs      = 0;
    _lastKeepaliveMs = 0;
    _state           = DroneState{};
    Serial.println("[FlowWifi] Protocol ready — 192.168.169.1:8800");
}

void FlowWifiProtocol::update() {
    uint32_t now = millis();
    if (!_state.active) {
        // In idle: send a slow 8800 heartbeat so the drone knows a controller
        // is connected (without it the drone ignores the first TakeOff command).
        // throttle=0x00, cmd=0x00 — safe for a grounded disarmed drone and
        // does not trigger altitude-hold re-arm (unlike the 0x80 idle packets
        // the original code used).
        if (now - _lastSendMs >= IDLE_HEARTBEAT_MS) {
            _lastSendMs = now;
            sendPacket();
        }
        // Also send the secondary keepalive on port 7099 as the KY UFO app does.
        if (now - _lastKeepaliveMs >= KEEPALIVE_INTERVAL_MS) {
            _lastKeepaliveMs = now;
            sendKeepalive();
        }
        return;
    }
    if (now - _lastSendMs < CONTROL_INTERVAL_MS) return;
    _lastSendMs = now;
    sendPacket();
}

void FlowWifiProtocol::setControl(uint8_t roll, uint8_t pitch,
                                   uint8_t throttle, uint8_t yaw, uint8_t cmd) {
    _state.roll     = roll;
    _state.pitch    = pitch;
    _state.throttle = throttle;
    _state.yaw      = yaw;
    _state.cmd      = cmd;
    _state.active   = true;
}

void FlowWifiProtocol::setIdle() {
    _state.roll     = DroneAxis::NEUTRAL;
    _state.pitch    = DroneAxis::NEUTRAL;
    _state.throttle = DroneAxis::MIN;  // MIN not NEUTRAL — NEUTRAL triggers altitude-hold re-arm on a grounded drone
    _state.yaw      = DroneAxis::NEUTRAL;
    _state.cmd      = DroneCmd::None;
    _state.active   = false;
}

void FlowWifiProtocol::sendPacket() {
    uint8_t pkt[PACKET_SIZE];
    memset(pkt, 0, sizeof(pkt));

    // ---- Outer header (18 bytes) ----------------------------------------
    pkt[0]  = OUTER_SYNC;
    pkt[1]  = OUTER_VERSION;
    pkt[2]  = OUTER_LEN_LO;
    pkt[3]  = OUTER_LEN_HI;
    pkt[4]  = OUTER_FLAG_A;
    pkt[5]  = OUTER_FLAG_B;
    pkt[6]  = OUTER_FLAG_C;
    pkt[7]  = OUTER_FLAG_D;
    // pkt[8..11] = 0x00 (flags + padding)
    pkt[12] = (uint8_t)(_seq & 0xFF);        // sequence counter LE
    pkt[13] = (uint8_t)(_seq >> 8);
    // pkt[14..15] = 0x00
    pkt[16] = INNER_LEN_LO;
    pkt[17] = INNER_LEN_HI;
    _seq++;

    // ---- Inner control packet (20 bytes at offset INNER_OFFSET) ----------
    uint8_t* inner = pkt + INNER_OFFSET;
    inner[0]  = INNER_SYNC;
    inner[1]  = INNER_LENGTH_BYTE;
    inner[2]  = _state.roll;
    inner[3]  = _state.pitch;
    inner[4]  = _state.throttle;
    inner[5]  = _state.yaw;
    inner[6]  = _state.cmd;
    inner[7]  = INNER_CONST_FLAG;
    // inner[8..17] = 0x00 (padding)
    inner[18] = _state.roll ^ _state.pitch ^ _state.throttle ^ _state.yaw ^ _state.cmd ^ INNER_CONST_FLAG;
    inner[19] = INNER_FOOTER;
    // pkt[38..87] = 0x00 (trailing zeros)

    _udp.beginPacket(DRONE_IP, CONTROL_PORT);
    _udp.write(pkt, sizeof(pkt));
    _udp.endPacket();
}

void FlowWifiProtocol::sendKeepalive() {
    uint8_t pkt[2] = {KEEPALIVE_BYTE, KEEPALIVE_BYTE};
    _udp.beginPacket(DRONE_IP, KEEPALIVE_PORT);
    _udp.write(pkt, sizeof(pkt));
    _udp.endPacket();
}
