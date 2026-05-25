#pragma once
#include <Arduino.h>

class WifiManager {
public:
    void begin(const char* ssid, const char* password = nullptr);
    void update();
    bool isConnected() const;
    String localIP() const;

private:
    const char* _ssid     = nullptr;
    const char* _password = nullptr;
    uint32_t _lastReconnectMs = 0;

    static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;
};
