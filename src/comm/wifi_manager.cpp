#include <Arduino.h>
#include <WiFi.h>
#include "wifi_manager.h"

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
