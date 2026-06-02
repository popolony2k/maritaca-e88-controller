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
    _seq        = 0;
    _lastSendMs = 0;
    _state      = DroneState{};
    Serial.println("[FlowWifi] Protocol ready — 192.168.169.1:8800");
}

void FlowWifiProtocol::update() {
    uint32_t now = millis();
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
    _state.roll     = 0x80;
    _state.pitch    = 0x80;
    _state.throttle = 0x80;
    _state.yaw      = 0x80;
    _state.cmd      = DroneCmd::None;
    _state.active   = false;
}

void FlowWifiProtocol::sendPacket() {
    uint8_t pkt[88];
    memset(pkt, 0, sizeof(pkt));

    // ---- Outer header (18 bytes) ----------------------------------------
    pkt[0]  = 0xEF;
    pkt[1]  = 0x02;
    pkt[2]  = 0x58;                         // payload length LE: 88 = 0x58
    pkt[3]  = 0x00;
    pkt[4]  = 0x02;
    pkt[5]  = 0x02;
    pkt[6]  = 0x00;
    pkt[7]  = 0x01;
    // pkt[8..11] = 0x00 (flags + padding)
    pkt[12] = (uint8_t)(_seq & 0xFF);        // sequence counter LE
    pkt[13] = (uint8_t)(_seq >> 8);
    // pkt[14..15] = 0x00
    pkt[16] = 0x14;                          // inner packet length LE: 20 = 0x14
    pkt[17] = 0x00;
    _seq++;

    // ---- Inner control packet (20 bytes at offset 18) --------------------
    uint8_t* inner = pkt + 18;
    inner[0]  = 0x66;
    inner[1]  = 0x14;
    inner[2]  = _state.roll;
    inner[3]  = _state.pitch;
    inner[4]  = _state.throttle;
    inner[5]  = _state.yaw;
    inner[6]  = _state.cmd;
    inner[7]  = 0x02;
    // inner[8..17] = 0x00 (padding)
    inner[18] = _state.roll ^ _state.pitch ^ _state.throttle ^ _state.yaw ^ _state.cmd ^ 0x02;
    inner[19] = 0x99;
    // pkt[38..87] = 0x00 (trailing zeros)

    _udp.beginPacket(DRONE_IP, CONTROL_PORT);
    _udp.write(pkt, sizeof(pkt));
    _udp.endPacket();
}
