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

/**
 * @brief Command byte flags shared by all supported drone protocol variants.
 *
 * Multiple flags may be OR-combined, though in practice only one is sent
 * per packet.  Not all flags are supported by every drone variant.
 */
namespace DroneCmd {
    constexpr uint8_t None      = 0x00; ///< No command — normal flight packet.
    constexpr uint8_t TakeOff   = 0x01; ///< Auto take-off (also used as Land toggle on FLOW-WIFI).
    constexpr uint8_t Land      = 0x02; ///< Controlled landing (WIFI_8K_ only).
    constexpr uint8_t EmergStop = 0x04; ///< Immediate motor cut-off.
    constexpr uint8_t Flip      = 0x08; ///< 360° flip manoeuvre.
    constexpr uint8_t Headless  = 0x10; ///< Toggle headless mode.
    constexpr uint8_t Lock      = 0x20; ///< Lock motors (disarm).
    constexpr uint8_t Unlock    = 0x40; ///< Unlock motors (arm).
    constexpr uint8_t CaliGyro  = 0x80; ///< Gyro calibration (hold flat).
}

/**
 * @brief Snapshot of the last control values written to the drone.
 *
 * All axis values use the Eachine convention: 0x80 = neutral, 0x00 = min, 0xFE = max.
 */
struct DroneState {
    uint8_t roll     = 0x80;          ///< Roll axis (0x80 = neutral).
    uint8_t pitch    = 0x80;          ///< Pitch axis (0x80 = neutral).
    uint8_t throttle = 0x80;          ///< Throttle (0x80 = neutral/hover).
    uint8_t yaw      = 0x80;          ///< Yaw axis (0x80 = neutral).
    uint8_t cmd      = DroneCmd::None; ///< Active command flag byte.
    bool    active   = false;          ///< True when sending control; false in keepalive mode.
};

/**
 * @brief Abstract interface for all drone UDP protocol drivers.
 *
 * Concrete implementations: DroneProtocol (WIFI_8K_ black drone) and
 * FlowWifiProtocol (FLOW-WIFI grey drone).  FlightController and main.cpp
 * depend only on this interface — they never touch the concrete types.
 */
class DroneProtocolBase {
public:
    virtual ~DroneProtocolBase() = default;

    /** @brief Open the UDP socket. Call once in setup() after WiFi is initialised. */
    virtual void begin() = 0;

    /** @brief Advance timers and transmit pending packets. Call every loop(). */
    virtual void update() = 0;

    /** @brief Set active control values for the next packet. */
    virtual void setControl(uint8_t roll, uint8_t pitch, uint8_t throttle,
                            uint8_t yaw, uint8_t cmd = DroneCmd::None) = 0;

    /** @brief Return to keepalive / idle mode. */
    virtual void setIdle() = 0;

    /** @brief Begin app-mode activation sequence (no-op for drones that don't need it). */
    virtual void beginAppModeEntry() {}

    /** @brief Exit app mode (no-op for drones that don't need it). */
    virtual void exitAppMode() {}

    /** @brief Last control state written via setControl(). */
    virtual const DroneState& state() const = 0;

    /** @brief True once the drone is ready to accept control packets. */
    virtual bool appModeOk() const { return true; }

    /**
     * @brief True if the drone requires the Calibrating→Arming sequence
     *        (CaliGyro + Unlock + TakeOff) before flying.
     *
     * Return false for drones that auto-arm on a TakeOff toggle (e.g. FLOW-WIFI).
     * When false, FlightController jumps directly from Idle to Flying and fires
     * a TakeOff command on entry.
     */
    virtual bool supportsArmSequence() const { return true; }
};
