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
 * @brief Non-blocking WiFi station manager with automatic reconnection.
 *
 * Connects to the configured SSID on begin() and retries every
 * RECONNECT_INTERVAL_MS if the connection drops. All reconnection logic
 * runs from update() without blocking the main loop.
 */
class WifiManager {
public:
    /**
     * @brief Scan for visible WiFi networks and return the index of the first
     *        SSID from @p ssids that is found, or -1 if none are visible.
     *
     * Blocking — takes 1–4 s. Call once during setup(), before begin().
     *
     * @param ssids  Null-terminated array of SSID strings to search for.
     * @param count  Number of entries in @p ssids.
     * @return Index into @p ssids of the first match, or -1.
     */
    static int scanForFirst(const char* const ssids[], int count);

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
