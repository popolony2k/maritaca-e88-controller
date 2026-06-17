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
static const BLEUUID HID_REPORT_MAP_UUID((uint16_t)0x2A4B);

// ---- BLE scan parameters ----
static constexpr uint16_t BLE_SCAN_INTERVAL_MS   =  200; ///< Scan interval.
static constexpr uint16_t BLE_SCAN_WINDOW_MS     =  180; ///< Scan active window — ~90% duty cycle.
static constexpr uint32_t BLE_SCAN_DURATION_S    =    5; ///< Timed scan window length.
static constexpr uint32_t BLE_RESCAN_INTERVAL_MS = 6000; ///< Restart scan window every ~6 s while not connected.
static constexpr size_t   HID_REPORT_BUF_SIZE    =   32; ///< Max raw HID report size buffered.

// ---- Shared state (accessed from BLE callbacks + main task) ----
static BLEAdvertisedDevice* _foundDevice = nullptr;
static volatile bool        _doConnect   = false;
static volatile bool        _connected   = false;
static volatile bool        _doScan      = false;

static volatile bool _reportReady = false;
static uint8_t       _reportBuf[HID_REPORT_BUF_SIZE];
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
    scan->setInterval(BLE_SCAN_INTERVAL_MS);
    scan->setWindow(BLE_SCAN_WINDOW_MS);  // ~90% duty cycle for max discovery chance
    scan->setActiveScan(true);            // active scan — needed to catch HID UUID in scan responses
    scan->clearResults();                 // free heap from previous window
    scan->start(BLE_SCAN_DURATION_S, nullptr, false);  // timed window; update() restarts it
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

    // Print HID Report Map to reveal the exact report format
    BLERemoteCharacteristic* reportMap = hidService->getCharacteristic(HID_REPORT_MAP_UUID);
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

    // Restart timed scan window when it expires
    if (!_connected && !_doConnect && (millis() - _lastScanMs >= BLE_RESCAN_INTERVAL_MS)) {
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
static constexpr uint32_t DEBUG_REPORT_LOG_COUNT = 200; ///< Log this many raw reports to Serial, then stop.

void BleGamepad::parseReport(const uint8_t* data, uint8_t len) {
    bool debug = (_debugCount < DEBUG_REPORT_LOG_COUNT);
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
    static constexpr uint8_t IPEGA_REPORT_LEN        =  17; ///< Total report length.
    static constexpr int     IPEGA_NUM_CONTACTS      =   4; ///< Touch contact blocks per report.
    static constexpr int     IPEGA_BYTES_PER_CONTACT =   4; ///< Bytes per contact block.
    static constexpr uint8_t IPEGA_TIP_SWITCH_MASK   = 0x01; ///< Tip Switch bit within contact byte 0.
    static constexpr uint8_t IPEGA_CONTACT_ID_MASK   = 0x0F; ///< Contact ID nibble mask within contact byte 0.
    static constexpr uint8_t IPEGA_CONTACT_ID_SHIFT  =    4; ///< Contact ID nibble shift within contact byte 0.
    static constexpr int     IPEGA_STICK_Y_BOUNDARY  = 1000; ///< Y threshold splitting left/right stick zones.
    static constexpr float   IPEGA_THROTTLE_DEADBAND = 0.08f; ///< Dead band for throttle up/down classification.
    if (len == IPEGA_REPORT_LEN) {
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
        _axes.buttons   = 0;
        _axes.connected = true;

        // Fixed touchscreen positions for each button (iPega HOME+A digitizer mode).
        // Contacts matching these positions (±BTN_TOL) are button presses, not stick input.
        struct BtnDef { int x, y; uint16_t bit; };
        static constexpr BtnDef kBtns[] = {
            {332,  2044, GamepadBtn::A},
            {780,  1281, GamepadBtn::B},
            {241,  1270, GamepadBtn::X},
            {818,  2020, GamepadBtn::Y},
            {723,   544, GamepadBtn::DpadUp},
            {352,   562, GamepadBtn::DpadDown},
            {534,   379, GamepadBtn::DpadLeft},
            {536,   740, GamepadBtn::DpadRight},
            {1173,  416, GamepadBtn::LT},
            {578,  2041, GamepadBtn::R1},
        };
        static constexpr int N_BTNS  = (int)(sizeof(kBtns) / sizeof(kBtns[0]));
        static constexpr int BTN_TOL = 30;

        for (int i = 0; i < IPEGA_NUM_CONTACTS; i++) {
            const uint8_t* blk = data + i * IPEGA_BYTES_PER_CONTACT;
            uint8_t tipSwitch = blk[0] & IPEGA_TIP_SWITCH_MASK;
            uint8_t cid       = (blk[0] >> IPEGA_CONTACT_ID_SHIFT) & IPEGA_CONTACT_ID_MASK;
            int x = getX(blk), y = getY(blk);

            if (debug) {
                Serial.printf("[iPega] blk[%d] tip=%d cid=%d x=%d y=%d\n",
                              i, (int)tipSwitch, (int)cid, x, y);
            }

            if (!tipSwitch) continue;

            // Check for button contact first (fixed position)
            bool isBtn = false;
            for (int j = 0; j < N_BTNS; j++) {
                if (abs(x - kBtns[j].x) <= BTN_TOL && abs(y - kBtns[j].y) <= BTN_TOL) {
                    _axes.buttons |= kBtns[j].bit;
                    isBtn = true;
                    break;
                }
            }
            if (isBtn) continue;

            // Not a button — classify as stick by Y zone
            if (y < IPEGA_STICK_Y_BOUNDARY) {           // left stick area
                // LEFT/RIGHT (Y) → yaw;  UP/DOWN (X) → throttle
                _axes.yaw = clamp((y - LY_CTR) / RANGE_LY);
                float lthrottle = (x - LX_CTR) / RANGE_LX;  // UP → x increases → positive
                if (debug) Serial.printf("[THR] x=%d lthrottle=%.2f\n", x, lthrottle);
                _axes.throttleUp   = (lthrottle >  IPEGA_THROTTLE_DEADBAND) ? clamp(lthrottle) : 0.0f;
                _axes.throttleDown = (lthrottle < -IPEGA_THROTTLE_DEADBAND) ? clamp(-lthrottle) : 0.0f;
            } else {                                   // right stick area
                // LEFT/RIGHT (Y) → roll;  UP/DOWN (X) → pitch
                _axes.roll  = clamp((y - RY_CTR) / RANGE_RY);
                _axes.pitch = clamp((x - RX_CTR) / RANGE_RX);
            }
        }
        return;
    }

    // --- 8BitDo Switch mode (7 or 8 bytes, unsigned 8-bit axes centered at 128) ---
    static constexpr uint8_t SWITCH_REPORT_ID_PREFIX = 0x01; ///< Optional leading Report ID byte.
    static constexpr uint8_t SWITCH_REPORT_ID_LEN    =    8; ///< Report length when the Report ID byte is present.
    static constexpr uint8_t SWITCH_MIN_REPORT_LEN   =    7; ///< Minimum valid report length (without Report ID).
    static constexpr uint8_t BYTE_BUTTONS_LO         =    0; ///< Offset: Buttons[7:0].
    static constexpr uint8_t BYTE_LEFT_STICK_X       =    3; ///< Offset: Left stick X.
    static constexpr uint8_t BYTE_LEFT_STICK_Y       =    4; ///< Offset: Left stick Y.
    static constexpr uint8_t BYTE_RIGHT_STICK_X      =    5; ///< Offset: Right stick X.
    static constexpr uint8_t AXIS_CENTER_8BIT        =  128; ///< Raw axis center (0-255 range).
    static constexpr uint8_t BTN_ZR_BIT              = 0x80; ///< ZR trigger bit in Buttons[7:0].
    static constexpr uint8_t BTN_ZL_BIT              = 0x40; ///< ZL trigger bit in Buttons[7:0].

    uint8_t o = (len == SWITCH_REPORT_ID_LEN && data[0] == SWITCH_REPORT_ID_PREFIX) ? 1 : 0;
    if ((len - o) < SWITCH_MIN_REPORT_LEN) return;

    uint8_t btn0 = data[o + BYTE_BUTTONS_LO];
    uint8_t lx   = data[o + BYTE_LEFT_STICK_X];
    uint8_t ly   = data[o + BYTE_LEFT_STICK_Y];
    uint8_t rx   = data[o + BYTE_RIGHT_STICK_X];

    auto norm = [](uint8_t v) { return (v - AXIS_CENTER_8BIT) / (float)AXIS_CENTER_8BIT; };

    _axes.roll         = clamp( norm(lx));
    _axes.pitch        = clamp(-norm(ly));  // invert Y: push forward = positive
    _axes.yaw          = clamp( norm(rx));
    _axes.throttleUp   = (btn0 & BTN_ZR_BIT) ? 1.0f : 0.0f;
    _axes.throttleDown = (btn0 & BTN_ZL_BIT) ? 1.0f : 0.0f;
    _axes.connected    = true;
}
