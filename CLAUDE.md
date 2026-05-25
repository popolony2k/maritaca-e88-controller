# Arduino + Claude Development Context — atoms3-e88-controller

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
| **Display** | Built-in 0.85" 128×128 LCD, GC9107 driver, SPI |
| **Input** | Built-in button (GPIO 41) |
| **LED** | Built-in RGB LED (via M5Unified) |
| **Communication** | WiFi 2.4 GHz (ESP32-S3 built-in) — connects to drone AP |
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
- `#include <M5Unified.h>` for display, button, and LED access.
- Non-blocking patterns with `millis()` — no `delay()` in the main loop.
- `Serial.begin(115200)` as default baud rate.
- ESP32-S3 ADC range 0–4095; use `analogReadResolution(12)` if reading analog pins.
- Hardware abstractions in their own `.h` / `.cpp` files, not all in `main.cpp`.
- Libraries declared in `platformio.ini` under `lib_deps`.

### M5Unified display quick reference

```cpp
M5.Display.fillScreen(TFT_NAVY);
M5.Display.setTextDatum(MC_DATUM);      // middle-center
M5.Display.setTextColor(TFT_WHITE, TFT_NAVY);
M5.Display.setTextSize(1);
M5.Display.drawString("text", x, y);
M5.Display.setRotation(2);              // USB-C at bottom
```

### Button

```cpp
M5.update();                // call every loop()
if (M5.BtnA.wasPressed()) { ... }
```

---

## Project Structure

```
atoms3-e88-controller/
├── src/
│   └── main.cpp            # entry point
├── include/                # shared headers
├── lib/                    # local libraries
├── test/                   # PlatformIO unit tests
├── doc/
│   └── vscode/             # reference copies of .vscode config files
├── platformio.ini
└── CLAUDE.md
```

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
| UDP **8080** | UDP | **Confirmed open** | **MJPEG video stream** (drone → client) |

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
