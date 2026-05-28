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
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include "ble_gamepad.h"

// ---- UUIDs ----
static const BLEUUID HID_SERVICE_UUID((uint16_t)0x1812);
static const BLEUUID HID_REPORT_UUID ((uint16_t)0x2A4D);

// ---- Shared state (accessed from BLE callbacks + main task) ----
static BLEAdvertisedDevice* _foundDevice = nullptr;
static volatile bool        _doConnect   = false;
static volatile bool        _connected   = false;
static volatile bool        _doScan      = false;

static volatile bool _reportReady = false;
static uint8_t       _reportBuf[32];
static uint8_t       _reportLen  = 0;

static BLEClient*    _client     = nullptr;

BleGamepad* BleGamepad::_instance = nullptr;

// ---- BLE scan callback ----
class HidScanCallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) override {
        bool isHid = dev.haveServiceUUID() && dev.isAdvertisingService(HID_SERVICE_UUID);
        Serial.printf("[BLE] Seen: \"%s\" %s HID=%d RSSI=%d UUIDs=%s\n",
                      dev.getName().c_str(),
                      dev.getAddress().toString().c_str(),
                      (int)isHid,
                      dev.getRSSI(),
                      dev.haveServiceUUID() ? dev.getServiceUUID().toString().c_str() : "none");
        if (isHid) {
            BLEDevice::getScan()->stop();
            if (_foundDevice) delete _foundDevice;
            _foundDevice = new BLEAdvertisedDevice(dev);
            _doConnect   = true;
        }
    }
};

// ---- BLE client callbacks ----
class HidClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient*) override {
        _connected = true;
        if (BleGamepad::_instance) BleGamepad::_instance->onConnected();
        Serial.println("[BLE] Gamepad connected");
    }
    void onDisconnect(BLEClient*) override {
        _connected = false;
        _doScan    = true;
        if (BleGamepad::_instance) BleGamepad::_instance->onDisconnected();
        Serial.println("[BLE] Gamepad disconnected — will rescan");
    }
};

static HidScanCallback    _scanCb;
static HidClientCallbacks _clientCb;

// ---- HID report notify callback (runs in BLE task context) ----
static void notifyCallback(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    if (len > 0 && len <= sizeof(_reportBuf)) {
        memcpy(_reportBuf, data, len);
        _reportLen   = (uint8_t)len;
        _reportReady = true;
    }
}

// ---- BleGamepad implementation ----

void BleGamepad::begin() {
    _instance = this;
    _status   = BleStatus::Scanning;
    BLEDevice::init("");
    Serial.println("[BLE] Initialized — scanning for HID gamepad (pair 8BitDo with X+Start)");
    startScan();
}

void BleGamepad::startScan() {
    _lastScanMs = millis();
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&_scanCb);
    scan->setInterval(200);
    scan->setWindow(180);      // ~90% duty cycle for max discovery chance
    scan->setActiveScan(true); // active scan — needed to catch HID UUID in scan responses
    scan->clearResults();       // free heap from previous window
    scan->start(5, nullptr, false);  // 5 s timed window; update() restarts it
}

void BleGamepad::doConnect() {
    _status = BleStatus::Connecting;
    Serial.printf("[BLE] Connecting to %s...\n",
                  _foundDevice->getAddress().toString().c_str());

    if (_client) {
        _client->disconnect();
        _client = nullptr;
    }

    _client = BLEDevice::createClient();
    _client->setClientCallbacks(&_clientCb);

    if (!_client->connect(_foundDevice)) {
        Serial.println("[BLE] Connection failed — rescanning");
        delete _foundDevice;
        _foundDevice = nullptr;
        _doScan = true;
        return;
    }

    delete _foundDevice;
    _foundDevice = nullptr;

    BLERemoteService* hidService = _client->getService(HID_SERVICE_UUID);
    if (!hidService) {
        Serial.println("[BLE] HID service not found — disconnecting");
        _client->disconnect();
        return;
    }

    // Print HID Report Map (0x2A4B) to reveal the exact report format
    static const BLEUUID REPORT_MAP_UUID((uint16_t)0x2A4B);
    BLERemoteCharacteristic* reportMap = hidService->getCharacteristic(REPORT_MAP_UUID);
    if (reportMap) {
        std::string desc = reportMap->readValue();
        Serial.printf("[BLE] HID descriptor (%u bytes):", (unsigned)desc.size());
        for (size_t i = 0; i < desc.size(); i++) Serial.printf(" %02X", (uint8_t)desc[i]);
        Serial.println();
    } else {
        Serial.println("[BLE] HID descriptor not found");
    }

    // Print all characteristics in the HID service for inspection
    auto* chars = hidService->getCharacteristics();
    for (auto& pair : *chars) {
        BLERemoteCharacteristic* c = pair.second;
        Serial.printf("[BLE] char %s canNotify=%d canRead=%d\n",
                      c->getUUID().toString().c_str(),
                      (int)c->canNotify(), (int)c->canRead());
    }

    // Subscribe to all Input Report characteristics
    int registered = 0;
    for (auto& pair : *chars) {
        BLERemoteCharacteristic* c = pair.second;
        if (c->getUUID().equals(HID_REPORT_UUID) && c->canNotify()) {
            c->registerForNotify(notifyCallback);
            registered++;
        }
    }

    if (registered == 0) {
        Serial.println("[BLE] No notifiable HID report found — disconnecting");
        _client->disconnect();
        return;
    }

    Serial.printf("[BLE] Subscribed to %d HID report(s)\n", registered);
    _debugCount = 0;
}

void BleGamepad::onConnected() {
    _status = BleStatus::Connected;
}

void BleGamepad::onDisconnected() {
    _axes   = GamepadAxes{};
    _status = BleStatus::Scanning;
}

void BleGamepad::update() {
    // Trigger connection attempt (blocking — happens once, acceptable in Idle state)
    if (_doConnect) {
        _doConnect = false;
        doConnect();
    }

    // Restart scan after disconnect or failed connect
    if (_doScan && !_connected) {
        _doScan = false;
        _axes   = GamepadAxes{};
        _status = BleStatus::Scanning;
        startScan();
        Serial.println("[BLE] Scanning...");
    }

    // Restart timed scan window when it expires (every ~6 s)
    if (!_connected && !_doConnect && (millis() - _lastScanMs >= 6000)) {
        startScan();
    }

    // Process latest HID report
    if (_reportReady && _connected) {
        _reportReady = false;
        parseReport(_reportBuf, _reportLen);
    }

    if (!_connected) {
        _axes.connected = false;
    }
}

// 8BitDo Switch mode (X+Start) HID report format (7 or 8 bytes):
//   Byte 0: Buttons[7:0]  B=0x01 A=0x02 Y=0x04 X=0x08 L=0x10 R=0x20 ZL=0x40 ZR=0x80
//   Byte 1: Buttons[15:8] -=0x01 +=0x02 L3=0x04 R3=0x08 Home=0x10 Capture=0x20
//   Byte 2: HAT (0=N 2=E 4=S 6=W 8=center)
//   Byte 3: Left stick X  (0–255, center≈128)
//   Byte 4: Left stick Y  (0–255, center≈128, up=0)
//   Byte 5: Right stick X (0–255, center≈128)
//   Byte 6: Right stick Y (0–255, center≈128)
// Some controllers prepend a Report ID byte (0x01); detected by checking length.
//
// iPega PG-9021S HID report format (17 bytes):
//   HID usage page 0x0D (Digitizer), usage 0x04 (Touch Screen) — it is a multi-touch panel.
//   4 contact blocks × 4 bytes each + 1 byte contact count (byte 16).
//   Per 4-byte contact block (bit-packed, little-endian):
//     bit 0:     Tip Switch (1 = finger touching)
//     bit 1:     In Range
//     bits 2–3:  padding
//     bits 4–7:  Contact ID (0 = left stick area, 1 = right stick area)
//     bits 8–19: X coordinate (0–1200)
//     bits 20–31:Y coordinate (0–2200, top=0)
//   Center positions measured from observed rest-touch data:
//     Left  stick: X≈523  Y≈450    Right stick: X≈533  Y≈1650
void BleGamepad::parseReport(const uint8_t* data, uint8_t len) {
    bool debug = (_debugCount < 200);
    if (debug) {
        Serial.printf("[BLE] report[%u] len=%u:", _debugCount, len);
        for (uint8_t i = 0; i < len; i++) Serial.printf(" %02X", data[i]);
        Serial.println();
        _debugCount++;
    }

    auto clamp = [](float v) { return v < -1.0f ? -1.0f : v > 1.0f ? 1.0f : v; };

    // --- iPega PG-9021S (17 bytes, multi-touch digitizer) ---
    // Touchscreen is portrait-oriented internally; held in landscape.
    // Physical UP/DOWN maps to touchscreen X (0–1200); UP = X increases.
    // Physical LEFT/RIGHT maps to touchscreen Y (0–2200); LEFT = Y increases.
    if (len == 17) {
        auto getX = [](const uint8_t* b) -> int {
            return (int)((uint16_t)b[1] | (((uint16_t)(b[2] & 0x0F)) << 8));
        };
        auto getY = [](const uint8_t* b) -> int {
            return (int)(((uint16_t)(b[2] >> 4)) | (((uint16_t)b[3]) << 4));
        };

        // Both sticks report cid=0; split by Y coordinate.
        // Portrait-top  (Y < 1000) = landscape-LEFT  = left  stick; center ≈ (523, 500)
        // Portrait-bot  (Y > 1000) = landscape-RIGHT = right stick; center ≈ (533, 1650)
        static constexpr int   LX_CTR = 523, LY_CTR = 500;
        static constexpr int   RX_CTR = 533, RY_CTR = 1650;
        static constexpr float RANGE_LX = 300.0f;  // left stick X travel (UP/DOWN → pitch)
        static constexpr float RANGE_LY = 240.0f;  // left stick Y travel (LEFT/RIGHT → roll)
        static constexpr float RANGE_RX = 145.0f;  // right stick X travel (UP/DOWN → throttle)
        static constexpr float RANGE_RY = 120.0f;  // right stick Y travel (LEFT/RIGHT → yaw)

        _axes.roll = _axes.pitch = _axes.yaw = 0.0f;
        _axes.throttleUp = _axes.throttleDown = 0.0f;
        _axes.connected = true;

        for (int i = 0; i < 4; i++) {
            const uint8_t* blk = data + i * 4;
            uint8_t tipSwitch = blk[0] & 0x01;
            uint8_t cid       = (blk[0] >> 4) & 0x0F;
            int x = getX(blk), y = getY(blk);

            if (debug) {
                Serial.printf("[iPega] blk[%d] tip=%d cid=%d x=%d y=%d\n",
                              i, (int)tipSwitch, (int)cid, x, y);
            }

            if (!tipSwitch) continue;

            if (y < 1000) {                            // left stick area
                _axes.roll  = clamp( (y - LY_CTR) / RANGE_LY);
                _axes.pitch = clamp(-(x - LX_CTR) / RANGE_LX);
            } else {                                   // right stick area
                _axes.yaw = clamp( (y - RY_CTR) / RANGE_RY);
                float rthrottle = (x - RX_CTR) / RANGE_RX;  // UP → x increases → positive
                if (debug) Serial.printf("[THR] x=%d rthrottle=%.2f\n", x, rthrottle);
                _axes.throttleUp   = (rthrottle >  0.08f) ? clamp(rthrottle) : 0.0f;
                _axes.throttleDown = (rthrottle < -0.08f) ? clamp(-rthrottle) : 0.0f;
            }
        }
        return;
    }

    // --- 8BitDo Switch mode (7 or 8 bytes, unsigned 8-bit axes centered at 128) ---
    uint8_t o = (len == 8 && data[0] == 0x01) ? 1 : 0;
    if ((len - o) < 7) return;

    uint8_t btn0 = data[o + 0];
    uint8_t lx   = data[o + 3];
    uint8_t ly   = data[o + 4];
    uint8_t rx   = data[o + 5];

    auto norm = [](uint8_t v) { return (v - 128) / 128.0f; };

    _axes.roll         = clamp( norm(lx));
    _axes.pitch        = clamp(-norm(ly));  // invert Y: push forward = positive
    _axes.yaw          = clamp( norm(rx));
    _axes.throttleUp   = (btn0 & 0x80) ? 1.0f : 0.0f;  // ZR
    _axes.throttleDown = (btn0 & 0x40) ? 1.0f : 0.0f;  // ZL
    _axes.connected    = true;
}
