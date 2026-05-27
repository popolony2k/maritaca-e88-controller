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
class DroneProtocol {
public:
    /** @brief Open the UDP socket. Call once in setup() after WiFi is initialised. */
    void begin();

    /** @brief Advance timers and transmit pending packets. Call every loop(). */
    void update();

    /**
     * @brief Set active control values for the next packet.
     *
     * Switches the driver into active mode; control packets are sent at 25 Hz.
     * @param roll      Roll axis [0x00, 0xFE], neutral = 0x80.
     * @param pitch     Pitch axis [0x00, 0xFE], neutral = 0x80.
     * @param throttle  Throttle [0x00, 0xFE], neutral = 0x80.
     * @param yaw       Yaw axis [0x00, 0xFE], neutral = 0x80.
     * @param cmd       Optional command flag from DroneCmd namespace.
     */
    void setControl(uint8_t roll, uint8_t pitch, uint8_t throttle,
                    uint8_t yaw, uint8_t cmd = DroneCmd::None);

    /** @brief Return to keepalive mode — all axes neutral, no active command. */
    void setIdle();

    /**
     * @brief Begin the app-mode activation sequence.
     *
     * Waits APP_MODE_SETTLE_MS after WiFi connects, then sends 0x42 0x76
     * to port 8080 repeatedly for APP_MODE_ENTRY_MS. The drone switches
     * from RF remote mode to WiFi control on the first successful receipt.
     * Call once per WiFi connection event.
     */
    void beginAppModeEntry();

    /**
     * @brief Exit app mode and return the drone to RF remote control.
     *
     * Sends 0x42 0x77 to port 8080 once and cancels any pending entry.
     * Called automatically when the flight controller transitions to Idle.
     */
    void exitAppMode();

    /** @brief Last control state written via setControl(). */
    const DroneState& state() const { return _state; }

    /** @brief True after the first successful app-mode send. */
    bool appModeOk() const { return _appModeOk; }

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
