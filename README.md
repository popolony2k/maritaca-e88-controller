# maritaca-e88-controller

A custom WiFi controller for **Eachine E88 / E58** toy drones, running on an **M5Stack AtomS3**.  
The AtomS3 connects to the drone's WiFi access point, sends throttle / roll / pitch / yaw control
packets using the reverse-engineered Eachine UDP protocol, and displays real-time flight status
on its built-in 128×128 LCD.

---

## Hardware

| Component | Detail |
|---|---|
| **Board** | M5Stack AtomS3 (ESP32-S3, 240 MHz, 8 MB Flash, 2 MB PSRAM) |
| **Display** | Built-in 0.85" 128×128 LCD, GC9107 driver, SPI |
| **Input** | Built-in button — front screen face (GPIO 41 / BtnA) |
| **IMU** | Built-in accelerometer + gyroscope (via M5Unified) |
| **Communication** | WiFi 2.4 GHz (ESP32-S3 built-in) — connects to drone AP |
| **Target drones** | Eachine E88, E58 and compatible WIFI_8K_ variants |
| **Power** | USB-C |

---

## Project Goal

Replace the drone's factory RF remote with a tilt-based gesture controller:

- **Tilt the AtomS3 left/right** → roll (strafe)
- **Rotate the board** (gyro Z-rate) → yaw (spin)
- **Tilt the board back** (near edge up) → throttle increases continuously
- **Tilt the board forward** (near edge down) → throttle decreases continuously
- **Level** → throttle holds its current value
- **Short press** (screen button) → start calibration → arm → fly
- **Long press (2 s)** → emergency stop, return to idle

---

## Protocol — WIFI_8K_ Variant (Reverse-Engineered)

Identified hardware: **WIFI_8K_Wf48702** drone, IP `192.168.4.153`.

### App mode activation — required before control works

The drone boots in RF remote mode and ignores the control port until a mode-switch command
is sent to the video port:

| Action | Port | Payload |
|---|---|---|
| Enter WiFi control | UDP **8080** | `42 76` |
| Exit WiFi control | UDP **8080** | `42 77` |

### Control packets — UDP port 8090, ~25 Hz

```
[ 0x66 | Roll | Pitch | Throttle | Yaw | Cmd | XOR | 0x99 ]
```

| Byte | Value | Description |
|---|---|---|
| 0 | `0x66` | Header (fixed) |
| 1 | 0–254 | Roll — neutral = `0x80` |
| 2 | 0–254 | Pitch — neutral = `0x80` |
| 3 | 0–254 | Throttle — neutral = `0x80` |
| 4 | 0–254 | Yaw — neutral = `0x80` |
| 5 | flags | Command byte |
| 6 | computed | XOR of bytes 1–4 |
| 7 | `0x99` | Footer (fixed) |

### Keepalive — UDP port 8090, ~790 ms interval

Sent when no active control is in progress:

```
[ 0xAA | 0x80 | 0x80 | 0x00 | 0x80 | 0x00 | 0x80 | 0x55 ]
```

### Command byte flags

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

Packet captures used for reverse engineering are stored in `resources/pcap/`.

---

## Flight State Machine

```
         [button press + WiFi OK]
  Idle ──────────────────────────► Calibrating (1.5 s, CaliGyro)
                                        │
                                        ▼
                                    Arming
                                    ├── Unlock  (0.5 s)
                                    └── TakeOff (0.5 s)
                                        │
                                        ▼
                                    Flying  ◄────────────────┐
                                        │                    │
                              [button press]          (future: resume)
                                        │
                                        ▼
                                    Landing (2 s) ──► Idle

  [long press 2 s, any state] ──► Emergency ──► Idle
```

| State | Action |
|---|---|
| **Idle** | Sends neutral keepalive; awaits button press |
| **Calibrating** | Sends `CaliGyro` + zero throttle for 1.5 s (gyro calibration) |
| **Arming** | Sends `Unlock` → `TakeOff` command sequence |
| **Flying** | Accel-driven control at 25 Hz via `AccelController` |
| **Landing** | Sends `Land` command for 2 s, then returns to Idle |
| **Emergency** | Sends `EmergStop` once, immediately returns to Idle |

---

## Architecture

The firmware is layered: hardware details are isolated in the HAL so that the business
logic never touches M5Unified or Arduino APIs directly.

```
src/
├── main.cpp                    # setup() / loop() — wires all modules together
│
├── hal/
│   ├── hal.h                   # HAL structs: BoardHal, DisplayHal, ImuHal, ButtonHal
│   ├── m5atoms3.h              # Extern declarations: kBoard, kDisplay, kImu, kButton
│   └── m5atoms3.cpp            # HAL implementations via M5Unified (only file that
│                               #   includes M5Unified.h); non-capturing lambdas
│
├── comm/
│   ├── wifi_manager.h/.cpp     # WiFi station mode; auto-reconnect every 5 s
│   └── drone_protocol.h/.cpp   # Packet encoder, 25 Hz control loop, keepalive,
│                               #   app-mode entry/exit handshake
│
├── imu/
│   └── accelerometer.h/.cpp    # Raw IMU polling at 50 Hz via ImuHal
│
├── control/
│   ├── operation_mode.h        # OperationMode enum (AccelControl; future: Mirror)
│   ├── flight_controller.h/.cpp# State machine (Idle→Calib→Arming→Flying→Landing)
│   │                           #   + button handling (short/long press)
│   └── accel_controller.h/.cpp # Tilt→control mapping: roll/yaw via mapAxis(),
│                               #   rate-based throttle via pitch axis, EMA filter
│
└── ui/
    ├── display.h               # Display class declaration
    └── display.cpp             # 128×128 HUD: vertical throttle bar (left) +
                                #   horizontal ROL/YAW/PCH bars (right), status bar
```

### HAL pattern

`hal.h` defines plain C structs of function pointers — no virtual dispatch, no heap.
`m5atoms3.cpp` fills them with non-capturing lambdas that decay to raw function pointers
at compile time, giving zero runtime overhead.  All other modules receive a `const XxxHal&`
and remain fully testable without hardware.

### Control loop timing

| Task | Rate |
|---|---|
| IMU read | 50 Hz (every 20 ms) |
| Control packet | 25 Hz (every 40 ms) |
| Keepalive | ~1.3 Hz (every 790 ms) |
| Display refresh | 10 Hz (every 100 ms) |

---

## Display Layout (128×128, rotated 270°)

```
┌──────────────────────────────┐
│ WiFi   [STATE]            A  │  ← status bar (WiFi / flight state / app mode)
├─────────────────────────────┤
│     │  ROL ████████░░░░░░   │
│     │  YAW ░░░████████░░░   │
│ THR │  PCH ░░░░░░░░░░░░░   │  ← PCH greyed out (throttle-only mode)
│  ▓  │                       │
│  ▓  │                       │
│     │                       │
└──────────────────────────────┘
```

- **THR** — vertical orange bar, left column, fills bottom-to-top
- **ROL / YAW** — horizontal cyan bars when accel active, grey when idle
- **PCH** — always grey (pitch axis drives throttle, not direct drone pitch)
- **Status bar** — WiFi (green/red), flight state name, "A" app-mode indicator

---

## Build & Flash

### Prerequisites

- [VS Code](https://code.visualstudio.com/) + [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
- USB-C cable connected to the AtomS3

### Commands

```bash
# Build
pio run

# Upload
pio run -t upload

# Serial monitor (115200 baud)
pio device monitor
```

Or use the PlatformIO toolbar icons in VS Code (✓ build, → upload, plug monitor).

### platformio.ini

```ini
[env:m5stack-atoms3]
platform  = espressif32
board     = m5stack-atoms3
framework = arduino
monitor_speed = 115200
build_flags   = -DARDUINO_USB_CDC_ON_BOOT=1
lib_deps  =
    m5stack/M5Unified
```

---

## Project Structure

```
maritaca-e88-controller/
├── src/                        # Firmware source (see Architecture above)
├── include/                    # Shared headers (currently unused)
├── lib/                        # Local libraries (currently unused)
├── test/                       # PlatformIO unit tests (placeholder)
├── resources/
│   └── pcap/                   # Raw packet captures used for protocol RE
├── doc/
│   └── vscode/                 # Reference copies of .vscode config files
├── packet-analysis-conversation.md  # Protocol reverse-engineering notes
├── platformio.ini
├── CLAUDE.md                   # AI-assistant context and coding conventions
└── README.md
```

---

## Dependencies

| Library | Source | Purpose |
|---|---|---|
| `M5Unified` | PlatformIO registry (`m5stack/M5Unified`) | Display, button, IMU, board init |

All other code is self-contained in `src/`.
