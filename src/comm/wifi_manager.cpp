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
#include <WiFi.h>
#include "wifi_manager.h"

int WifiManager::scanForFirst(const char* const ssids[], int count) {
    Serial.println("[WiFi] Scanning for known drones...");
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks();
    Serial.printf("[WiFi] %d network(s) found\n", n);
    for (int i = 0; i < n; i++) {
        String found = WiFi.SSID(i);
        for (int j = 0; j < count; j++) {
            if (found == ssids[j]) {
                Serial.printf("[WiFi] Drone detected: %s\n", ssids[j]);
                WiFi.scanDelete();
                WiFi.disconnect(false);  // reset scan state so begin() works cleanly after
                return j;
            }
        }
    }
    WiFi.scanDelete();
    WiFi.disconnect(false);  // reset scan state so begin() works cleanly after
    Serial.println("[WiFi] No known drone found — defaulting to first");
    return -1;
}

void WifiManager::begin(const char* ssid, const char* password) {
    _ssid     = ssid;
    _password = password;
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid, _password);
    Serial.printf("[WiFi] Connecting to %s\n", _ssid);
}

void WifiManager::update() {
    if (WiFi.status() == WL_CONNECTED) return;

    uint32_t now = millis();
    if (now - _lastReconnectMs < RECONNECT_INTERVAL_MS) return;
    _lastReconnectMs = now;

    Serial.printf("[WiFi] Reconnecting to %s\n", _ssid);
    WiFi.disconnect(false);
    WiFi.begin(_ssid, _password);
}

bool WifiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WifiManager::localIP() const {
    return WiFi.localIP().toString();
}
