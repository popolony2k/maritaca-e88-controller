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

/**
 * @brief Command byte flags for the Eachine E58/WIFI_8K_ control packet.
 *
 * Multiple flags may be OR-combined into a single packet, though in practice
 * only one command is sent per packet.
 */
namespace DroneCmd {
    constexpr uint8_t None      = 0x00; ///< No command — normal flight packet.
    constexpr uint8_t TakeOff   = 0x01; ///< Auto take-off sequence.
    constexpr uint8_t Land      = 0x02; ///< Controlled landing sequence.
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
    uint8_t roll     = 0x80; ///< Roll axis (0x80 = neutral).
    uint8_t pitch    = 0x80; ///< Pitch axis (0x80 = neutral).
    uint8_t throttle = 0x80; ///< Throttle (0x80 = neutral/hover).
    uint8_t yaw      = 0x80; ///< Yaw axis (0x80 = neutral).
    uint8_t cmd      = DroneCmd::None; ///< Active command flag byte.
    bool    active   = false; ///< True when sending control packets; false in keepalive mode.
};

/**
 * @brief Abstract base for all drone UDP protocol drivers.
 *
 * DroneProtocol (WIFI_8K_ black drone) and FlowWifiProtocol (FLOW-WIFI grey
 * drone) both implement this interface so that FlightController and main.cpp
 * can work with either drone without knowing the concrete type.
 */
class DroneProtocolBase {
public:
    virtual ~DroneProtocolBase() = default;

    /** @brief Open the UDP socket. Call once in setup() after WiFi is initialised. */
    virtual void begin() = 0;

    /** @brief Advance timers and transmit pending packets. Call every loop(). */
    virtual void update() = 0;

    /** @brief Set active control values. */
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
     *        (CaliGyro + Unlock + TakeOff commands) before flying.
     *
     * Return false for drones that auto-arm on throttle input (e.g. FLOW-WIFI).
     * When false, FlightController jumps directly from Idle to Flying.
     */
    virtual bool supportsArmSequence() const { return true; }
};

/**
 * @brief Eachine WIFI_8K_ UDP control protocol driver.
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
    /** @brief Encode and transmit a control packet to CONTROL_PORT. */
    void sendControlPacket();

    /** @brief Transmit the keepalive packet (AA 80 80 00 80 00 80 55) to CONTROL_PORT. */
    void sendKeepalive();

    /**
     * @brief Send a two-byte command to the video port (8080).
     * @return Non-zero on success, 0 on failure.
     */
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
    static constexpr uint16_t CONTROL_PORT          = 8090; ///< E58-format control channel.
    static constexpr uint16_t VIDEO_PORT            = 8080; ///< App-mode switch + MJPEG stream.
    static constexpr uint32_t CONTROL_INTERVAL_MS   =   40; ///< 25 Hz control rate.
    static constexpr uint32_t KEEPALIVE_INTERVAL_MS =  790; ///< Keepalive period.
    static constexpr uint32_t APP_MODE_SETTLE_MS    =  500; ///< Settle delay after WiFi connect.
    static constexpr uint32_t APP_MODE_ENTRY_MS     = 1000; ///< Total app-mode retry window.
    static constexpr uint32_t APP_MODE_INTERVAL_MS  =  150; ///< App-mode resend interval.
};
