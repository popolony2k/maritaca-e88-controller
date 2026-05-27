#pragma once
#include <Arduino.h>

/**
 * @brief Non-blocking WiFi station manager with automatic reconnection.
 *
 * Connects to the configured SSID on begin() and retries every
 * RECONNECT_INTERVAL_MS if the connection drops. All reconnection logic
 * runs from update() without blocking the main loop.
 */
class WifiManager {
public:
    /**
     * @brief Start WiFi and initiate the first connection attempt.
     * @param ssid      Target network SSID.
     * @param password  WPA2 passphrase, or nullptr for open networks.
     */
    void begin(const char* ssid, const char* password = nullptr);

    /** @brief Poll connection state and issue reconnection if needed. Call every loop(). */
    void update();

    /** @brief True when the station is associated and has an IP address. */
    bool isConnected() const;

    /** @brief Current IP address as a string, or "0.0.0.0" when not connected. */
    String localIP() const;

private:
    const char* _ssid     = nullptr;
    const char* _password = nullptr;
    uint32_t    _lastReconnectMs = 0;

    static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000; ///< Retry period.
};
