# Arduino + Claude Development Context — maritaca-e88-controller

This project uses **VS Code + PlatformIO + Claude Code** to build a custom WiFi controller for Eachine E88 / E58 toy drones, running on an M5Stack AtomS3.

---

## Project Goal

Control an **Eachine E88 / E58** Chinese toy drone from an **M5Stack AtomS3** device.
These drones expose a WiFi access point and accept UDP command packets using a proprietary Eachine protocol.
The AtomS3 connects to the drone's AP, sends throttle / roll / pitch / yaw packets, and displays flight status on its built-in LCD.

---

## Hardware

| Item | Detail |
|---|---|
| **Board** | M5Stack AtomS3 (ESP32-S3, 240 MHz, 8 MB Flash, 2 MB PSRAM) |
| **Display** | Built-in 0.85" 128×128 LCD, GC9107 driver, SPI — rotated 270° in firmware |
| **Input** | Built-in **screen/face button** = BtnA (GPIO 41). Side button = hardware reset — do NOT use for flight. Say "press the screen." |
| **LED** | Built-in RGB LED (via M5Unified) |
| **Communication** | WiFi 2.4 GHz (ESP32-S3 built-in) — connects to drone AP |
| **Power base** | M5Stack Atomic Battery Base — IP5306 power IC at I2C 0x75 |
| **Target drones** | Eachine E88, E58 (same Eachine UDP protocol family) |
| **Power** | USB-C |

---

## Project Setup

- **Editor**: VS Code
- **Build system**: PlatformIO
- **Source files**: C++ (`.cpp` / `.h`) with `#include <Arduino.h>` — not `.ino` files
- **Configuration**: `platformio.ini`

### `platformio.ini`

```ini
[env:m5stack-atoms3]
platform = espressif32
board = m5stack-atoms3
framework = arduino
monitor_speed = 115200
lib_deps =
    m5stack/M5Unified
```

---

## Coding Conventions

- `#include <Arduino.h>` at the top of every `.cpp` file.
- `#include <M5Unified.h>` only in `src/hal/m5atoms3.cpp` — nowhere else.
- Non-blocking patterns with `millis()` — no `delay()` in the main loop.
- `Serial.begin(115200)` as default baud rate; `Serial.setTxTimeoutMs(0)` to avoid stalling on HWCDC.
- Hardware abstractions use the HAL pattern: function-pointer structs with non-capturing lambdas (zero runtime overhead, decay to raw function pointers at compile time).
- Libraries declared in `platformio.ini` under `lib_deps`.

### HAL pattern (established)

```cpp
// hal.h — pure interface, no M5 dependency
struct BoardHal {
    void (*begin)           ();
    void (*update)          ();
    int  (*getBatteryLevel) ();   // 0/25/50/75/100 in %, or -1
    bool (*isCharging)      ();
};

// m5atoms3.cpp — only file that includes M5Unified.h
const BoardHal kBoard {
    .begin           = [] { auto cfg = M5.config(); M5.begin(cfg); },
    .update          = [] { M5.update(); },
    .getBatteryLevel = [] { return ip5306BatteryLevel(); },
    .isCharging      = [] { return ip5306IsCharging(); },
};
```

### IP5306 battery IC (Atomic Battery Base)

Direct I2C read via `M5.In_I2C.readRegister8()` — **do not use raw Wire calls**, they block the loop when the device doesn't respond on default pins.

```cpp
static constexpr uint8_t  IP5306_ADDR     = 0x75;
static constexpr uint8_t  IP5306_REG_STA0 = 0x70;  // bit 3: charging
static constexpr uint8_t  IP5306_REG_STA1 = 0x78;  // bits[2:1]: 00=25% 01=50% 10=75% 11=100%
static constexpr uint32_t IP5306_I2C_FREQ = 400000;
```

Level register only updates at power-on or physical button press — it is a snapshot, not live.

### Display

- LovyanGFX rotation values: 0=0°, 1=90°CW, 2=180°, 3=270°CW — no library constants, use integers.
- Display is rotated 270° (`ROTATION_270 = 3`), screen is 128×128.
- Display is a pure renderer — receives values, does not fetch data from sources.

### M5Unified display quick reference

```cpp
M5.Display.fillScreen(TFT_NAVY);
M5.Display.setTextDatum(MC_DATUM);
M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
M5.Display.setTextSize(1);
M5.Display.drawString("text", x, y);
M5.Display.setRotation(3);  // 270° — USB-C on right
```

### Button

```cpp
M5.update();                // call every loop()
if (M5.BtnA.wasReleased()) { ... }  // project uses wasReleased, not wasPressed
```

---

## Project Structure

```
maritaca-e88-controller/
├── src/
│   ├── main.cpp
│   ├── hal/
│   │   ├── hal.h               # HAL structs (BoardHal, DisplayHal, ImuHal, ButtonHal)
│   │   ├── m5atoms3.h          # extern declarations: kBoard, kDisplay, kImu, kButton
│   │   └── m5atoms3.cpp        # only file that includes M5Unified.h
│   ├── imu/
│   │   ├── accelerometer.h/cpp # ImuData struct, Accelerometer class
│   ├── comm/
│   │   ├── drone_protocol.h/cpp
│   │   └── wifi_manager.h/cpp
│   ├── bt/
│   │   ├── gamepad_axes.h          # GamepadAxes struct — normalized axes, no BLE deps
│   │   ├── ble_gamepad.h/cpp       # BLE HID host; BleStatus enum; all <BLEDevice.h> here only
│   ├── control/
│   │   ├── accel_controller.h/cpp  # tilt→roll/yaw + pitch→throttle mapping
│   │   ├── gamepad_controller.h/cpp# GamepadAxes→DroneState; dead zone, expo, slew rate
│   │   ├── flight_controller.h/cpp # state machine: Idle→Arming→Flying→Landing
│   │   └── operation_mode.h        # OperationMode enum; default = BluetoothControl
│   └── ui/
│       └── display.h/cpp
├── include/
├── lib/
├── test/
├── doc/
│   └── vscode/
├── platformio.ini
└── CLAUDE.md
```

---

## Current Implementation Status (2026-05-26)

### Working

- WiFi connection to drone AP (`WIFI_8K_Wf48702`)
- App mode activation (`42 76` → port 8080) — drone switches from RF to WiFi control
- Flight state machine: Idle → Calibrating (1.5 s) → Arming (Unlock + TakeOff) → Flying → Landing/Emergency
- Accel/tilt control: roll (left/right tilt), yaw (gz gyro rate), throttle (rate-based via pitch axis)
- Battery level display from IP5306 via direct I2C
- Display HUD: WiFi status, flight state, roll/pitch/yaw/throttle bars, battery
- **Mode selection screen** at boot: ACCEL TILT / BT GAMEPAD with 3 s countdown; button click cycles options and resets timer; default = BT GAMEPAD
- **BLE HID gamepad mode**: scans for BLE HID devices (8BitDo in Switch mode via X+Start), connects, subscribes to Input Report notifications, parses axis/button data → `GamepadAxes` → `GamepadController` → `DroneState`
- **Dedicated BT status screen**: shows SCANNING… / CONNECTING… / CONNECTED! with animated ping-pong bar, WiFi status, battery, pairing hint; transitions to flight HUD 1.5 s after connect with clean redraw

### Open Issues

1. **Drone rotates on ground before takeoff** — happens with board flat and still; suspected motor hardware fault or low battery. To diagnose: Serial-log `out.yaw` while board is flat — if 0x80 (128) it's hardware, not firmware. If not 0x80, gyro bias is leaking through dead zone → add startup calibration.
2. **Throttle too sensitive** — small hand movements cause too much altitude change. Parameter tuning has been reverted each time due to concurrent drone hardware issues during test flights. When flight is stable, try: `THROTTLE_DEAD_DEG=12`, `MAX_THROTTLE_DEG=25`, `THROTTLE_RATE_MAX=1.5`.
3. **Camera FPS quality command** — Android app sends a quality command (30%/60%/100% FPS) captured in PCAP. Need to identify the packet and check drone's default. Send 30% on connect if not default.
4. **BT gamepad untested on hardware** — 8BitDo HID report format is the expected Switch-mode layout; first 20 raw reports are logged to Serial to allow byte-offset verification. Adjust `parseReport()` offsets if needed once physical controller is available.

---

## Accel Controller Tuning Parameters

All in `src/control/accel_controller.h`:

```cpp
// Roll / Yaw
static constexpr float MAX_TILT_DEG   = 20.0f;
static constexpr float TILT_DEAD_ZONE = 10.0f;
static constexpr float TILT_EXPO      =  0.5f;

static constexpr float MAX_YAW_RATE   = 30.0f;  // tuned down from 60 — less spin
static constexpr float YAW_DEAD_ZONE  = 10.0f;
static constexpr float YAW_EXPO       =  0.4f;  // added for smoother yaw feel

// Throttle (rate-based via pitch axis)
static constexpr float THROTTLE_DEAD_DEG  =  8.0f;  // needs increase — too sensitive
static constexpr float MAX_THROTTLE_DEG   = 20.0f;
static constexpr float THROTTLE_RATE_MAX  =  2.5f;  // units/frame at max tilt
static constexpr float THROTTLE_INIT      = 128.0f;

// Low-pass filter
static constexpr float ANGLE_ALPHA    =  0.25f;
```

**Axis sign conventions:**

- Roll: negated (`-_filteredRoll`) — drone nose faces away from user, so left/right is mirrored
- Yaw: negated (`-_filteredYaw`) — same reason
- Pitch: drives throttle accumulator, `out.pitch` is always fixed at 0x80

---

## BT Gamepad — BleGamepad & GamepadController

### BLE library

Uses the **built-in ESP32 BLE Arduino** library (`<BLEDevice.h>`) that ships with the `espressif32` platform — no extra `lib_deps` entry required. All BLE includes are confined to `src/bt/ble_gamepad.cpp` (same isolation principle as M5Unified in m5atoms3.cpp).

### 8BitDo Switch-mode pairing

Hold **X + Start** until the LED blinks to enter Switch/BLE pairing mode. The controller advertises the HID service (UUID 0x1812) which `BleGamepad` scans for.

### HID report format (8BitDo Switch mode)

Some controllers prepend a 1-byte Report ID (`0x01`) — detected by `len == 8 && data[0] == 0x01`; offset `o` is set to 1 in that case, otherwise 0.

```
data[o+0]  Buttons[7:0]  B=0x01 A=0x02 Y=0x04 X=0x08 L=0x10 R=0x20 ZL=0x40 ZR=0x80
data[o+1]  Buttons[15:8] -=0x01 +=0x02 L3=0x04 R3=0x08 Home=0x10 Capture=0x20
data[o+2]  HAT  (0=N 2=E 4=S 6=W 8=center)
data[o+3]  Left stick X   (0–255, center≈128)
data[o+4]  Left stick Y   (0–255, center≈128, up=0 → invert)
data[o+5]  Right stick X  (0–255, center≈128)
data[o+6]  Right stick Y  (0–255, center≈128)
```

First 20 raw reports are dumped to Serial (`[BLE] report[N] len=N: XX XX …`) to allow format verification with a physical controller.

### GamepadController tuning parameters

All in `src/control/gamepad_controller.h`:

```cpp
static constexpr float DEAD_ZONE      = 0.12f;  // 12% of full scale
static constexpr float EXPO           = 0.40f;  // expo curve blending
static constexpr float SLEW           = 8.0f;   // units/frame max change for roll/pitch/yaw
static constexpr float THR_RATE       = 2.0f;   // units/frame throttle change from ZL/ZR
```

**Axis mapping:** Left stick X → roll, left stick Y → pitch (inverted), right stick X → yaw. ZR = throttle up, ZL = throttle down. Throttle is rate-based (same as accel mode) — hold ZR to climb, hold ZL to descend, release both to hold altitude.

---

## Drone Identification — Confirmed Hardware

The physical drone in use is an **E88 clone / WIFI_8K_ variant**, identified by:

| Property | Value |
|---|---|
| **WiFi SSID** | `WIFI_8K_Wf48702` |
| **Drone IP** | `192.168.4.153` (non-standard subnet — differs from documented E88 `192.168.1.1`) |
| **Client IP (AtomS3)** | `192.168.4.x` (DHCP assigned) |
| **Default gateway** | `192.168.4.153` |
| **MAC address** | `C2:38:85:02:87:F4` (locally administered — firmware-generated) |
| **MCU** | ESP32 confirmed (ping TTL=255, LwIP stack) |

---

## WIFI_8K_ Variant — Confirmed Protocol (Fully Reverse-Engineered)

### Port Map

| Port | Protocol | Status | Purpose |
|---|---|---|---|
| UDP 7099 | UDP | **Closed** | Standard E88 control (not used by this variant) |
| UDP 40000 | UDP | **Closed** | Standard E58 handshake (not used) |
| UDP 50000 | UDP | **Closed** | Standard E58 control (not used) |
| UDP **8090** | UDP | **CONFIRMED OPEN** | **Control channel** — E58-compatible protocol |
| UDP **8080** | UDP | **Confirmed open** | **MJPEG video stream** (drone → client) + app mode switch |

---

### Control Protocol — UDP 8090 (CONFIRMED via phone PCAP)

The WIFI_8K_ variant uses the **E58 8-byte packet format**, but on **UDP port 8090** instead of 50000.
Confirmed by packet capture from the user's phone connected to `WIFI_8K_Wf48702`.

#### Packet Format

```
[ 0x66 | Roll | Pitch | Throttle | Yaw | Cmd | XOR | 0x99 ]
```

| Byte | Value | Description |
|---|---|---|
| 0 | `0x66` | Header (fixed) |
| 1 | 0–254 | Roll (neutral = 128 = `0x80`) |
| 2 | 0–254 | Pitch (neutral = 128 = `0x80`) |
| 3 | 0–254 | Throttle (neutral = 128 = `0x80`) |
| 4 | 0–254 | Yaw (neutral = 128 = `0x80`) |
| 5 | flags | Command byte (see table below) |
| 6 | computed | XOR of bytes 1–4 (Roll ^ Pitch ^ Throttle ^ Yaw) |
| 7 | `0x99` | Footer (fixed) |

**Verified samples from live capture:**
- `66 80 80 26 80 00 a6 99` → throttle ramp-up, checksum: `0x80^0x80^0x26^0x80 = 0xa6` ✓
- `66 80 80 80 80 00 00 99` → all neutral, checksum: `0x80^0x80^0x80^0x80 = 0x00` ✓
- `66 80 80 52 80 00 d2 99` → throttle mid, checksum: `0x80^0x80^0x52^0x80 = 0xd2` ✓

#### App Mode Activation — REQUIRED before 8090 control works

The drone boots in **2.4 GHz RF controller mode** and ignores port 8090 until a mode-switch command is sent to **port 8080** (the video port doubles as a command channel):

| Packet | Port | Payload | Effect |
|---|---|---|---|
| Enter app mode | UDP **8080** | `42 76` | Drone switches to WiFi control; video stream starts |
| Exit app mode | UDP **8080** | `42 77` | Drone returns to RF controller mode; video stops |

**Confirmed from PCAP** (`PCAPdroid_25_mai._01_09_36.pcap`):

- `42 76` → sent by app button "switch to app control" → video stream begins instantly on port 8080
- `42 77` → sent by app button "return to 2.4GHz" → all drone traffic stops

**Implementation:** send `[0x42, 0x76]` to `192.168.4.153:8080` before starting the control sequence. Call `DroneProtocol::enterAppMode()` / `exitAppMode()` — already wired into `FlightController::enterState()`.

#### Transmission Rate

- **Control packets**: ~25 Hz (one packet every ~40 ms)
- **Handshake required**: send `42 76` to port 8080 first; drone ignores port 8090 otherwise

#### Keepalive / Heartbeat Packet

When the joystick is idle (no active control input), the app sends a different packet at **~790 ms intervals**:

```
[ 0xAA | 0x80 | 0x80 | 0x00 | 0x80 | 0x00 | 0x80 | 0x55 ]
```

- Header `0xAA` and footer `0x55` are bitwise complements of `0x66`/`0x99`
- Likely keeps the connection alive so the drone doesn't timeout
- The AtomS3 should send this when not actively controlling

#### Command Byte Flags

| Bit | Value | Function |
|---|---|---|
| 0 | `0x01` | Auto take-off |
| 1 | `0x02` | Land |
| 2 | `0x04` | Emergency stop |
| 3 | `0x08` | 360° flip |
| 4 | `0x10` | Headless mode |
| 5 | `0x20` | Lock |
| 6 | `0x40` | Unlock motors |
| 7 | `0x80` | Calibrate gyro |

#### Arduino Implementation Sketch

```cpp
#include <WiFiUdp.h>

WiFiUdp udp;
IPAddress droneIP(192, 168, 4, 153);
const uint16_t CONTROL_PORT = 8090;

void sendControl(uint8_t roll, uint8_t pitch,
                 uint8_t throttle, uint8_t yaw, uint8_t cmd = 0) {
    uint8_t pkt[8];
    pkt[0] = 0x66;
    pkt[1] = roll;
    pkt[2] = pitch;
    pkt[3] = throttle;
    pkt[4] = yaw;
    pkt[5] = cmd;
    pkt[6] = roll ^ pitch ^ throttle ^ yaw;
    pkt[7] = 0x99;
    udp.beginPacket(droneIP, CONTROL_PORT);
    udp.write(pkt, 8);
    udp.endPacket();
}

void sendKeepalive() {
    uint8_t pkt[8] = {0xAA, 0x80, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55};
    udp.beginPacket(droneIP, CONTROL_PORT);
    udp.write(pkt, 8);
    udp.endPacket();
}
```

---

### Video Stream — UDP 8080 (Confirmed via Packet Capture)

The drone streams MJPEG video **from** `192.168.4.153:8080` **to** the client's ephemeral port.

**Packet structure:**
```
[ IPv4 | UDP src=192.168.4.153:8080 dst=10.215.173.1:<ephemeral> ]
[ 8-byte proprietary header: 86 00 00 02 54 5A 48 00 ("TZH" magic) ]
[ JPEG payload: FF D8 FF DB ... ]
```

**Key details:**
- Capture taken from user's **phone connected to this exact drone** (`WIFI_8K_Wf48702`) — fully confirmed for this hardware
- Phone IP on the drone network: **`10.215.173.1`** (likely via packet capture VPN app such as PCAPdroid)
- Magic tag `54 5A 48` = ASCII **"TZH"** — proprietary Chinese-OEM IP camera/NVR framing (not RTP/RTSP)
- Video: **640×480 baseline JPEG**, YCbCr 4:2:0 color
- Each UDP packet carries one chunk of a JPEG frame; same TZH header with incrementing sequence numbers
- UDP checksum disabled (`00 00`) — typical for high-throughput streaming
- TTL 64 on video packets (LwIP stack on ESP32)

---

## E58 vs E88 vs WIFI_8K_ — Protocol Comparison

| Feature | E58 (standard) | E88 (standard) | **WIFI_8K_ (this drone)** |
|---|---|---|---|
| Drone IP | `192.168.0.1` | `192.168.1.1` | **`192.168.4.153`** |
| Control port | UDP 50000 | UDP 7099 | **UDP 8090** |
| Packet format | 8-byte `66…99` | ~2 bytes | **8-byte `66…99` (same as E58)** |
| Packet rate | ~20 Hz | Unknown | **~25 Hz** |
| Handshake | Port 40000 | Unknown | **None required** |
| Video | UDP 8800 | RTSP 7070 | **UDP 8080 (TZH/MJPEG)** |
| Keepalive | None documented | None documented | **`AA…55` every ~790 ms** |

---

## Build & Upload Workflow

- **Build**: PlatformIO bottom toolbar → checkmark (or `pio run`)
- **Upload**: bottom toolbar → right-arrow (or `pio run -t upload`)
- **Serial monitor**: bottom toolbar → plug icon (or `pio device monitor`)
- **Clean**: command palette → "PlatformIO: Clean"

---

## Common Tasks / Useful Prompts

- "Add WiFi station mode to connect to `WIFI_8K_Wf48702` and send a UDP control packet to `192.168.4.153:8090`."
- "Implement a non-blocking control loop sending E58-format packets at 25 Hz with keepalive at 790 ms."
- "Draw a HUD on the 128×128 display showing throttle, roll, pitch, yaw, and connection status."
- "Refactor this `delay()`-based loop to use non-blocking `millis()` timing."
- "Generate a header-only driver class for the WIFI_8K_ UDP control protocol."
- "Add unit tests using PlatformIO's `test` framework for the packet encoder and checksum."
