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
        if (dev.haveServiceUUID() && dev.isAdvertisingService(HID_SERVICE_UUID)) {
            Serial.printf("[BLE] Found HID device: \"%s\" %s\n",
                          dev.getName().c_str(),
                          dev.getAddress().toString().c_str());
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
    BLEDevice::init("");
    Serial.println("[BLE] Initialized — scanning for HID gamepad (pair 8BitDo with X+Start)");
    startScan();
}

void BleGamepad::startScan() {
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&_scanCb);
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);
    scan->start(0, nullptr, false);  // scan indefinitely
}

void BleGamepad::doConnect() {
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

    // Subscribe to all Input Report characteristics
    int registered = 0;
    auto* chars = hidService->getCharacteristics();
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

void BleGamepad::onDisconnected() {
    _axes = GamepadAxes{};
}

void BleGamepad::update() {
    // Trigger connection attempt (blocking — happens once, acceptable in Idle state)
    if (_doConnect) {
        _doConnect = false;
        doConnect();
    }

    // Restart scan after disconnect
    if (_doScan && !_connected) {
        _doScan = false;
        _axes   = GamepadAxes{};
        startScan();
        Serial.println("[BLE] Scanning...");
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

// 8BitDo Switch mode (X+Start) HID report format:
//   Byte 0: Buttons[7:0]  B=0x01 A=0x02 Y=0x04 X=0x08 L=0x10 R=0x20 ZL=0x40 ZR=0x80
//   Byte 1: Buttons[15:8] -=0x01 +=0x02 L3=0x04 R3=0x08 Home=0x10 Capture=0x20
//   Byte 2: HAT (0=N 2=E 4=S 6=W 8=center)
//   Byte 3: Left stick X  (0–255, center≈128)
//   Byte 4: Left stick Y  (0–255, center≈128, up=0)
//   Byte 5: Right stick X (0–255, center≈128)
//   Byte 6: Right stick Y (0–255, center≈128)
// Some controllers prepend a Report ID byte (0x01); detected by checking length.
void BleGamepad::parseReport(const uint8_t* data, uint8_t len) {
    // Log raw bytes for the first 20 reports to help confirm format
    if (_debugCount < 20) {
        Serial.printf("[BLE] report[%u] len=%u:", _debugCount, len);
        for (uint8_t i = 0; i < len; i++) Serial.printf(" %02X", data[i]);
        Serial.println();
        _debugCount++;
    }

    // Detect report ID prefix: if len==8 and byte[0]==0x01, skip it
    uint8_t o = (len == 8 && data[0] == 0x01) ? 1 : 0;

    if ((len - o) < 7) return;  // not enough bytes

    uint8_t btn0 = data[o + 0];
    // uint8_t btn1 = data[o + 1];  // available if needed
    uint8_t lx   = data[o + 3];
    uint8_t ly   = data[o + 4];
    uint8_t rx   = data[o + 5];
    // uint8_t ry = data[o + 6];   // right stick Y — not mapped yet

    auto norm = [](uint8_t v) { return (v - 128) / 128.0f; };
    auto clamp = [](float v) { return v < -1.0f ? -1.0f : v > 1.0f ? 1.0f : v; };

    _axes.roll         = clamp( norm(lx));
    _axes.pitch        = clamp(-norm(ly));  // invert Y: push forward = positive
    _axes.yaw          = clamp( norm(rx));
    _axes.throttleUp   = (btn0 & 0x80) ? 1.0f : 0.0f;  // ZR
    _axes.throttleDown = (btn0 & 0x40) ? 1.0f : 0.0f;  // ZL
    _axes.connected    = true;
}
