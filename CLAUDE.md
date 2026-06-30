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
| --- | --- |
| **Board** | M5Stack AtomS3 (ESP32-S3, 240 MHz, 8 MB Flash, 2 MB PSRAM) |
| **Display** | Built-in 0.85" 128×128 LCD, GC9107 driver, SPI — rotated 270° in firmware |
| **Input** | Built-in **screen/face button** = BtnA (GPIO 41). Side button = hardware reset — do NOT use for flight. Say "press the screen." |
| **LED** | Built-in RGB LED (via M5Unified) |
| **Communication** | WiFi 2.4 GHz (ESP32-S3 built-in) — connects to drone AP |
| **Power base** | M5Stack Atomic Battery Base — ETA9085 power IC (no I2C exposed); battery level read via GPIO8 (ADC2) voltage divider |
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
    int  (*getBatteryLevel) ();   // 0/25/50/75/100 in %
    bool (*isCharging)      ();   // always false — not available on this hardware
};

// m5atoms3.cpp — only file that includes M5Unified.h
const BoardHal kBoard {
    .begin           = [] { auto cfg = M5.config(); M5.begin(cfg); },
    .update          = [] { M5.update(); },
    .getBatteryLevel = [] { return batteryLevel(); },
    .isCharging      = [] { return false; },
};
```

### Battery level (Atomic Battery Base)

The Atomic Battery Base's power IC (board sticker: **ETA9085**, not a genuine IP5306) exposes **no I2C interface** — confirmed via a full bus scan of both `M5.In_I2C` and `M5.Ex_I2C` (only the onboard IMU at `0x68` on `In_I2C` responds; `0x75` NACKs on both buses). Do not attempt I2C register reads for battery status on this hardware.

Battery voltage is instead sensed on **GPIO8** (`ADC2` in the AtomS3 `pins_arduino.h`) through an onboard voltage divider:

```cpp
static constexpr uint8_t ADC_SAMPLES        = 8;     // averaged to smooth ~10 mV ADC jitter
static constexpr float   ADC_DIVIDER_RATIO  = 2.06f; // empirically derived, see below
static constexpr float   BATTERY_HYSTERESIS = 0.03f; // volts a boundary must be re-crossed by before the level changes

static float batteryVoltage() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; i++) sum += analogReadMilliVolts(ADC2);
    return (sum / (float)ADC_SAMPLES) / 1000.0f * ADC_DIVIDER_RATIO;
}

static int batteryLevel() {
    static int lastLevel = -1;  // unknown until the first read

    struct Boundary { float v; int below; int above; };
    static const Boundary boundaries[] = {
        {3.00f,   0,  25},
        {3.48f,  25,  50},
        {3.62f,  50,  75},
        {3.82f,  75, 100},
    };

    float v = batteryVoltage();

    if (lastLevel < 0) {
        int level = 0;
        for (auto& b : boundaries) if (v >= b.v) level = b.above;
        lastLevel = level;
        return lastLevel;
    }

    for (auto& b : boundaries) {
        // Descend immediately at the raw boundary — no lag on low-battery warnings.
        if (lastLevel == b.above && v < b.v) { lastLevel = b.below; break; }
        // Ascend only once past the boundary by a margin — prevents bounce-back from noise/recovery.
        if (lastLevel == b.below && v >= b.v + BATTERY_HYSTERESIS) { lastLevel = b.above; break; }
    }
    return lastLevel;
}
```

`ADC_DIVIDER_RATIO = 2.06` was derived empirically (2026-06-13) from calibration points
against the base's 4-LED indicator, on battery power only (no USB):

| LEDs lit | A2 (GPIO8) reading | VBAT = A2 × 2.06 | Threshold bucket |
| --- | --- | --- | --- |
| 3 (75%) | ~1776 mV | 3.66 V | 3.62–3.81 V → 75% ✓ |
| 2 (50%) | ~1726 mV | 3.56 V | 3.48–3.61 V → 50% ✓ |
| 1 (25%) | ~1676 mV | 3.45 V | 3.00–3.47 V → 25% ✓ |

`batteryLevel()` applies **asymmetric** hysteresis (`BATTERY_HYSTERESIS = 0.03f`): the
level descends immediately at the raw boundary (matches the LED indicator with no lag —
e.g. 1 LED/25% confirmed lit at VBAT ~3.45 V, just below the 3.48 V boundary), but must
climb back past the boundary by 0.03 V before ascending again. A symmetric band was
tried first (2026-06-13) but caused the display to stick at 50% for minutes — even
after the LED already showed 25% — because the real voltage plateaued in the
3.45–3.48 V gap created by the symmetric margin.

Charging status is **not detectable** on this hardware (`isCharging()` always returns `false`).

### Display

- LovyanGFX rotation values: 0=0°, 1=90°CW, 2=180°, 3=270°CW — no library constants, use integers.
- Display is rotated 270° (`ROTATION_270 = 3`), screen is 128×128.
- Display is a pure renderer — receives values, does not fetch data from sources.
- `DisplayHal` includes `drawPng(data, len, x, y, w, h, scaleX, scaleY)` — M5Unified PNG decoding is fully behind the HAL. `display.cpp` does **not** include `M5Unified.h`. The PNG header (`src/resources/popolon_png.h`) is always included on all boards.
- `Display::drawSplash()` fills the screen black then draws the PNG scaled to fit: 128×128 on AtomS3 (fills the square panel), 128×128 centered in the 135×240 portrait panel on StickC Plus2. Called once in `setup()` for `SPLASH_DURATION_MS = 2000` before `runModeSelection()`.

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

```text
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
│   │   ├── drone_protocol_base.h       # DroneCmd, DroneState, DroneProtocolBase (abstract)
│   │   ├── drone_protocol.h/cpp        # DroneProtocol — WIFI_8K_ black drone (E58 8-byte)
│   │   ├── flow_wifi_protocol.h/cpp    # FlowWifiProtocol — FLOW-WIFI grey drone (88-byte)
│   │   └── wifi_manager.h/cpp          # WiFi STA; scanForFirst() for auto-detection
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

## Current Implementation Status (2026-07-02)

### Working

- WiFi connection to drone AP (`WIFI_8K_Wf48702`) — auto-detected at boot via `WifiManager::scanForFirst()`
- App mode activation (`42 76` → port 8080) — drone switches from RF to WiFi control
- Flight state machine: Idle → Calibrating (1.5 s) → Arming (Unlock + TakeOff) → Flying → Landing/Emergency
- Accel/tilt control: roll (left/right tilt), pitch (forward/back tilt, drives drone pitch directly); throttle via screen-button hold gesture — see Throttle section below
- Battery level display via GPIO8 (ADC2) voltage divider on the Atomic Battery Base (ETA9085 — no I2C)
- Display HUD: WiFi status, flight state, roll/pitch/yaw/throttle bars, battery
- **Mode selection screen** at boot: ACCEL TILT / BT GAMEPAD with 3 s countdown; button click cycles options and resets timer; default = BT GAMEPAD
- **BLE HID gamepad mode**: scans for BLE HID devices, connects, subscribes to Input Report notifications, parses axis data → `GamepadAxes` → `GamepadController` → `DroneState`
- **Dedicated BT status screen**: shows SCANNING… / CONNECTING… / CONNECTED! with animated ping-pong bar, WiFi status, battery, pairing hint; transitions to flight HUD 1.5 s after connect (requires WiFi also connected) with clean redraw
- **iPega PG-9021S fully supported**: all 4 analog axes + 10 buttons mapped and confirmed working. See iPega section below.
- **Idle+BT HUD preview**: when gamepad is connected but flight state is Idle, axis bars (including throttle) show raw gamepad input without sending UDP.
- **Screen auto-off/on**: screen turns off automatically when flight HUD activates; turns back on when BT/WiFi disconnects; D-pad LEFT toggles on/off while HUD is active. Uses `DisplayHal::setBrightness()` + `Display::sleep()`/`wake()`.
- **FLOW-WIFI grey drone fully supported**: auto-detected, 88-byte protocol, direct throttle (altitude hold), TakeOff toggle arm/land. See FLOW-WIFI section below.
- **Both drones confirmed to run altitude-hold firmware** (2026-06-11): `0x80` throttle = hold current altitude, deviation = continuous climb/descend rate. Throttle-hold gestures (ACCEL screen-button hold and BT-gamepad left stick/ZL/ZR) now snap back to `0x80` the instant the gesture ends, instead of leaving the drone climbing/descending indefinitely. See Throttle sections below.
- **FLOW-WIFI Idle session maintenance** (2026-06-16): In Idle, `FlowWifiProtocol::update()` sends a slow 8800 heartbeat (every 2 s) with `throttle=0x00, cmd=0x00` so the drone knows a controller is connected; without this the drone ignores the first TakeOff command. Also sends the secondary keepalive `[0x01, 0x01]` to port 7099 at 1 Hz (matches KY UFO app behaviour). `setIdle()` uses `throttle=0x00` (not `0x80`) to prevent altitude-hold re-arm while the drone is grounded and disarmed.
- **ACCEL mode yaw redesigned as single-click toggle + tilt** (2026-06-17): replaces the old gyro-rate-based yaw (hard to control reliably). See Yaw section under Accel Controller Tuning Parameters below.
- **bt-host fully tuned and hardware-confirmed on M5StickC Plus2 (2026-07-01)**: BtnB (right-side button) triggers `ESP.restart()` as a one-press firmware restart; `YAW_SLEW_RATE` reduced 5% (5.0 → 4.75) for calmer yaw; `SLEW_RATE` overridden to 6.0 for `BOARD_STICKC_PLUS2` via preprocessor conditional (AtomS3 stays at 3.0) — compensates for lower effective loop rate under BT+WiFi coexistence. All AccelControl axes confirmed on real hardware.
- **Boot splash screen (2026-07-02)**: `popolon.png` displayed full-screen for 2 s at every boot, before the SELECT MODE menu. AtomS3: 128×128 fills the square screen entirely. StickC Plus2: 128×128 centered in the 135×240 portrait panel. `DisplayHal::drawPng()` added — PNG decoding is now fully behind the HAL; `display.cpp` no longer includes `M5Unified.h` directly. `FlightDeps` gained an explicit constructor (C++11 aggregate restriction — default member initializers make a struct non-aggregate in C++11, breaking brace-init; constructor with defaults preserves all existing call sites).

### Open Issues

1. **iPega L1/RT/L3/R3 — confirmed dead-end via HOME+A digitizer mode** — resoldering the switches restored them in Android's Standard Gamepad mode (HOME+X) but HOME+A "Direct Play" mode emulates a PUBG touchscreen layout where these buttons were never present, so they will never generate HID contacts on the ESP32 BLE connection. Mapping them would require switching to HOME+X + implementing BLE bonding — significant rework, not currently planned.

### Resolved Issues

- **Drone rotates on ground before takeoff (mitigated 2026-06-17)** — was suspected to be either motor hardware imbalance or gyro bias leaking through yaw's dead zone while gyro-rate-based yaw was ambiently active. Resolved as a side effect of the yaw redesign: yaw is no longer gyro-rate based at all, so there is no path left for ambient drift to leak through — yaw can only ever be non-neutral during the explicit single-click-toggle + tilt gesture. If ground rotation is ever observed again, it would now point conclusively to motor/hardware, not firmware.

- **Dr.One "stops responding to everything" after pressing Land (2026-06-18)** — looked exactly like a firmware lockup (HUD correctly showed `LANDING`, drone neither descended nor responded to any further input). Diffed `flight_controller.cpp`'s Landing/B-button logic back to the last confirmed-working commit and found zero functional differences — ruled out a code regression. **Root cause: low Dr.One battery voltage**, not firmware — confirmed by the user swapping batteries, which immediately fixed it. Likely mechanism: low voltage degrades the drone's WiFi radio or causes its flight controller to brown out under motor load, so our packets get silently dropped/ignored — indistinguishable from a software lockup on our side, but nothing to fix in code. **Diagnostic takeaway: if Dr.One becomes unresponsive and no firmware change preceded it, check the battery before debugging code** — we have no telemetry from the drone to detect this automatically. Echoes the same suspicion already on file for Maritaca Force 1's ground-rotation issue above — weak-battery-causes-erratic-behavior is a recurring pattern across both drones in this family.

- **Dr.One re-arms when B is pressed after a stick-landing (downgraded to likely battery-related, 2026-06-19)** — previously tracked as an unfixable protocol-level ambiguity (`TakeOff` toggle direction unknown without telemetry; double-click mitigation tried and reverted, see git history). User now reports this resurfaced specifically during a low-battery test session, matching the same low-battery-causes-erratic-Dr.One-behavior pattern confirmed above. Suspected mechanism: low voltage may make the drone's ground/optical-flow sensor trigger spurious self-disarms more readily, increasing how often the "drone landed itself without our knowledge" precondition occurs — not a new firmware bug. Removed from Open Issues; revisit only if it reproduces again on a confirmed-fresh battery.

- **BT gamepad (8BitDo/Switch-style controllers) — CLOSED, hardware-incompatible, not a software gap (2026-06-20)**: Four physical controllers tried across many pairing attempts (8BitDo SF30 Pro, Xbox One S, 8BitDo Zero 2, 8BitDo Ultimate 2C), none ever detected as a BLE-advertising HID device by our scanner, by LightBlue, or by a dedicated manufacturer-ID diagnostic. Two of them (Zero 2, Ultimate 2C) showed up instantly as **"Pro Controller"** in Android's native Bluetooth pairing screen — which initially looked like a timing/interference problem, but turned out to be the actual answer.

  **Root cause (confirmed by 4 independent sources — Espressif's own ESP32-S3 docs, an arduino-esp32 GitHub issue, the ESP32 forum, and the Bluepad32 project's own FAQ/docs):**
  1. **Nintendo Switch controllers — and anything emulating a "Pro Controller," which is exactly what 8BitDo's Switch mode does — use Bluetooth Classic (BR/EDR), not BLE.** Android's system Bluetooth screen finding them instantly (as "Pro Controller") makes total sense once you know it primarily discovers Classic devices, not BLE.
  2. **The M5Stack AtomS3's chip (ESP32-S3) has no Bluetooth Classic radio at all — BLE-only at the silicon level.** Only the original ESP32 (not S3/C3/C6/H2) has the dual-mode radio needed. This is confirmed directly in Bluepad32's own docs: *"Only BLE gamepads are supported on ESP32-S3, since BR/EDR is not supported... Controllers like Switch, Wii, DualSense, DualShock, etc. only talk BR/EDR."*

  This means every one of the four "failed" tests was the **expected, correct outcome** — not a wrong controller, wrong pairing mode, or wrong timing. **No 8BitDo controller's Switch-emulation mode can ever work with the AtomS3**, full stop — the hardware physically cannot receive that radio protocol. A suggestion (from an AI assistant, unverified) to install the Bluepad32 library was also checked and ruled out for the same reason: Bluepad32 itself cannot do BR/EDR on ESP32-S3 either, so swapping libraries doesn't add hardware that isn't on the chip. The only way to ever support Switch-style controllers would be a hardware change to an original-ESP32-based board — out of scope for this feature. **The 8BitDo Switch-mode `parseReport()` code in `ble_gamepad.cpp` remains coded but is now known to be permanently unverifiable on this hardware** — leave it as-is (harmless, never triggered) rather than removing it, in case a genuinely-BLE controller (not Switch-emulation) is found later.

---

## Accel Controller Tuning Parameters

All in `src/control/accel_controller.h`:

```cpp
// Roll / Pitch (tilt)
static constexpr float MAX_TILT_DEG    = 20.0f;
static constexpr float TILT_DEAD_ZONE  = 10.0f;
static constexpr float TILT_EXPO       =  0.5f;

// Pitch (forward/backward tilt — drives drone pitch directly, NOT throttle)
static constexpr float MAX_PITCH_DEG   = 20.0f;
static constexpr float PITCH_DEAD_ZONE = 10.0f;
static constexpr float PITCH_EXPO      =  0.5f;

// Slew rate limiter
// BOARD_STICKC_PLUS2 overrides SLEW_RATE to 6.0 via preprocessor conditional
// (BT+WiFi coexistence halves the effective loop rate vs AtomS3's clean 25 Hz)
static constexpr float SLEW_RATE       =  3.0f;  // units/frame at 25 Hz, roll/pitch (AtomS3); 6.0 on StickC Plus2
static constexpr float YAW_SLEW_RATE   =  4.75f; // units/frame at 25 Hz, yaw mode — snappier but calmer than roll/pitch
static constexpr float YAW_DEAD_ZONE   =  7.0f;  // degrees, yaw mode — smaller than TILT_DEAD_ZONE

// Throttle (neutral/hover)
static constexpr float THROTTLE_INIT   = 128.0f;

// Low-pass filter
static constexpr float ANGLE_ALPHA     =  0.25f;
```

**Axis sign conventions:**

- Roll: negated (`-_filteredRoll`) — drone nose faces away from user, so left/right is mirrored
- Pitch: NOT negated, directly mapped — tilt forward = fly forward

### Yaw — single-click toggle, reuses roll's tilt gesture (ACCEL mode)

Yaw is **not** gyro-rate based — an earlier implementation drove yaw from the gyroscope
Z-axis rotation rate (physically twisting the board flat), but this proved a hard gesture
to control reliably and was replaced (2026-06-17).

Single-click the screen button while Flying to toggle **yaw mode** on/off
(`FlightController::_yawModeActive`, persistent until clicked again — not a hold gesture).
While ON, the same left/right tilt that normally drives roll is routed to **yaw** instead;
roll is suppressed to neutral (`0x80`) for the duration. Pitch and throttle are unaffected
and still work normally at the same time. Click again to switch back to roll.

Yaw mode uses its own tuning (`YAW_DEAD_ZONE = 7.0f`, `YAW_SLEW_RATE = 4.75f`) — a smaller
dead zone and faster slew than plain roll (`TILT_DEAD_ZONE = 10.0f`, `SLEW_RATE = 3.0f`),
since yaw is a deliberate momentary gesture rather than sustained tilt and benefits from a
snappier feel. `YAW_SLEW_RATE` was reduced 5% (5.0 → 4.75, 2026-07-01) after hardware
testing confirmed calmer yaw response on both boards. Implemented in `AccelController::update(imu, out, yawModeActive)` —
`_currentRoll`'s slewed value is directly reused as the yaw output when active, no separate
yaw filter state.

**Side effect:** this also resolves the previous "drone rotates on ground before takeoff"
issue (gyro bias suspected) — yaw can now only ever be non-neutral during this explicit,
deliberate gesture; there is no gyro-rate path left for ambient drift to leak through.

In BluetoothControl mode, yaw is unaffected by this — the right stick drives it directly via
`GamepadController`, gated by the same single-click toggle (`_yawEnabled`) as before.

### Throttle — button-hold gesture, altitude-hold on both drones

Throttle is NOT tilt-based. Both Maritaca (black, WIFI_8K_) and Dr.One (grey, FLOW-WIFI)
run altitude-hold firmware: `0x80` = hold current altitude, any deviation is sent every
frame as a continuous climb/descend rate. Throttle is driven by the screen-button hold
gesture in `FlightController::handleButton()` / `runState()` (Flying, ACCEL branch):

- Press-and-hold (first gesture, `_clickCount == 0` when hold threshold is reached) → throttle UP (climb).
- Click once, then press-and-hold within the double-click window (`DOUBLE_CLICK_MS = 1000`) → throttle DOWN (descend).
- While held, `_accel.adjustThrottle(±rate)` ramps the throttle accumulator per frame (`src/control/flight_controller.h`): `THROTTLE_HOLD_RATE_UP = 0.3f` (≈ 7.5 units/sec at 25 Hz) for climb, `THROTTLE_HOLD_RATE_DOWN = 0.10f` (≈ 2.5 units/sec) for descend — descend is gentler so the drone settles back to hover without an overshoot dip on release.
- On release, `cs.throttle` is forced to `0x80` and `AccelController::resetThrottle()` resets the accumulator back to neutral — the drone's own altitude hold then maintains the new altitude.

The same snap-to-hover pattern applies in BT Gamepad mode via `GamepadController` — see GamepadController tuning section below.

---

## BT Gamepad — BleGamepad & GamepadController

### BLE library

Uses the **built-in ESP32 BLE Arduino** library (`<BLEDevice.h>`) that ships with the `espressif32` platform — no extra `lib_deps` entry required. All BLE includes are confined to `src/bt/ble_gamepad.cpp` (same isolation principle as M5Unified in m5atoms3.cpp).

### Scan parameters

Active scan, interval=200, window=180 (~90% duty cycle). 5 s timed windows, auto-restarted every 6 s. HID descriptor (0x2A4B) and all characteristics logged on connect.

---

### iPega PG-9021S (confirmed working — branch `support-to-ipega`)

**Pairing:** Turn off controller → hold **HOME + A** until LED flashes. This activates the digitizer/touchscreen mode which works without BLE bonding. Do NOT use HOME+X (Android Standard Gamepad) — it requires BLE bonding that the ESP32 stack doesn't set up.

**HID report format:** 17 bytes. HID Usage Page 0x0D (Digitizer), Usage 0x04 (Touch Screen) — multi-touch panel with 4 contact blocks × 4 bytes + 1 contact-count byte.

Per 4-byte contact block (little-endian bit-packed):

```text
bit 0:      Tip Switch (1 = finger touching)
bit 1:      In Range
bits 2–3:   padding
bits 4–7:   Contact ID (always 0 for both sticks — split by Y coordinate instead)
bits 8–19:  X coordinate (0–1200)
bits 20–31: Y coordinate (0–2200, top=0)
```

Extraction:

```cpp
X = byte[1] | ((byte[2] & 0x0F) << 8)
Y = (byte[2] >> 4) | (byte[3] << 4)
```

**Orientation:** Screen is portrait internally, held in landscape. Physical UP/DOWN → X axis (UP = X increases). Physical LEFT/RIGHT → Y axis (LEFT = Y increases).

**Stick area split:** Both sticks report Contact ID 0 — distinguished by Y coordinate:

- Y < 1000 → left stick area (portrait-top = landscape-left)
- Y ≥ 1000 → right stick area (portrait-bottom = landscape-right)

**Tuned axis mapping (all working):**

| Axis | Stick | Coordinate | Center | Range |
| --- | --- | --- | --- | --- |
| Throttle | Left UP/DOWN | X | 523 | ±300 |
| Yaw | Left LEFT/RIGHT | Y | 500 | ±240 |
| Pitch | Right UP/DOWN | X | 533 | ±145 |
| Roll | Right LEFT/RIGHT | Y | 1650 | ±120 |

Sign conventions: `throttle = (x - LX_CTR) / RANGE_LX` (UP → positive → throttleUp), `yaw = (y - LY_CTR) / RANGE_LY`, `pitch = (x - RX_CTR) / RANGE_RX` (UP → positive → forward), `roll = (y - RY_CTR) / RANGE_RY`.

**Button detection:** Each button press appears as a contact at a fixed (x,y) in the 17-byte report (the controller emulates PUBG touchscreen positions — HOME+A is an undocumented "Direct Play" mode added by the 2019 firmware upgrade). `parseReport()` checks each contact against a known-position table (±30 tolerance) before the stick Y-zone classifier. Matches set bits in `GamepadAxes::buttons` (`GamepadBtn` namespace). `FlightController::handleGamepadButtons()` detects rising edges and fires drone commands.

**iPega button positions and drone command mapping:**

| Button | x | y | Zone | Drone command |
| --- | --- | --- | --- | --- |
| A | 332 | 2044 | right | Arm + takeoff (Idle + WiFi only) |
| B | 780 | 1281 | right | Land (Flying only) |
| X | 241 | 1270 | right | Emergency stop (any state) |
| Y | 818 | 2020 | right | Flip 360° (Flying only) |
| D-pad UP | 723 | 544 | left | Headless mode toggle (Flying, stick clear) |
| D-pad DOWN | 352 | 562 | left | Calibrate gyro one-shot (Flying, stick clear) |
| D-pad LEFT | 534 | 379 | left | Toggle screen on/off (HUD only) |
| D-pad RIGHT | 536 | 740 | left | Spare |
| LT | 1173 | 416 | left | Lock motors (Flying only) |
| R1 | 578 | 2041 | right | Unlock motors (Flying only) |

D-pad buttons are inside the left stick coordinate zone. A `stickClear` guard (`|roll|<0.15 && |pitch|<0.15`) prevents accidental triggers while the stick is deflected. One-shot commands (Flip, CaliGyro, Lock, Unlock) are held for 200 ms then cleared. Headless state is cleared on `enterState()`.

**Screen lifecycle (BT gamepad mode):** BT status screen → screen ON. BT + WiFi connected 1.5 s → HUD activates → screen auto-OFF. D-pad LEFT (HUD only) → toggle. BT or WiFi disconnects → screen auto-ON. Implemented via `Display::sleep()`/`wake()` (`DisplayHal::setBrightness`) in `main.cpp`.

**Broken/unavailable buttons:** L1, RT, L3, R3 (hardware — no contacts generated); SELECT, START (reserved by controller firmware for mode-switching combos — never appear in HID reports).

---

### 8BitDo Switch-mode pairing (coded, confirmed permanently untestable on this hardware — 2026-06-20)

**This code path can never be exercised on the AtomS3.** Nintendo Switch controllers — and 8BitDo's Switch-emulation mode, which is what "X+Start" (or model-specific equivalents) puts the controller into — use Bluetooth Classic (BR/EDR), not BLE. The AtomS3's chip (ESP32-S3) has no BR/EDR radio hardware at all (confirmed via Espressif's own docs and the Bluepad32 project's FAQ — only the original ESP32 has the dual-mode radio). Four controllers (SF30 Pro, Xbox One S, Zero 2, Ultimate 2C) were tried across many pairing modes; none could ever have worked, regardless of model or technique. See the Resolved Issues entry above for the full investigation. The code below is left in place (harmless, never triggered) in case a genuinely-BLE gamepad — not a Switch-emulating one — is found later.

Hold **X + Start** until the LED rotates to enter Switch/BLE pairing mode. The controller advertises the HID service (UUID 0x1812) which `BleGamepad` scans for.

**Note:** Original SN30 Pro (non-plus) does NOT have BLE — all modes use Bluetooth Classic. Only SN30 Pro+, Pro 2, and Ultimate Bluetooth support BLE HID. *(This distinction turned out to be moot — see note above: even "BLE-capable" 8BitDo models use BR/EDR specifically for Switch-mode, which is unreachable on this hardware regardless.)*

**HID report format (7 or 8 bytes):** Some controllers prepend a 1-byte Report ID (`0x01`) — detected by `len == 8 && data[0] == 0x01`; offset `o` is set to 1 in that case, otherwise 0.

```text
data[o+0]  Buttons[7:0]  B=0x01 A=0x02 Y=0x04 X=0x08 L=0x10 R=0x20 ZL=0x40 ZR=0x80
data[o+1]  Buttons[15:8] -=0x01 +=0x02 L3=0x04 R3=0x08 Home=0x10 Capture=0x20
data[o+2]  HAT  (0=N 2=E 4=S 6=W 8=center)
data[o+3]  Left stick X   (0–255, center≈128)
data[o+4]  Left stick Y   (0–255, center≈128, up=0 → invert)
data[o+5]  Right stick X  (0–255, center≈128)
data[o+6]  Right stick Y  (0–255, center≈128)
```

First 200 raw reports are dumped to Serial (`[BLE] report[N] len=N: XX XX …`) to allow byte-offset verification.

**Axis mapping:** Left stick X → roll, left stick Y → pitch (inverted), right stick X → yaw. ZR = throttle up, ZL = throttle down. Throttle is rate-based — hold ZR to climb, hold ZL to descend, release both to hold altitude.

**Update (2026-06-27): this hardware ceiling is now bypassed, on a different board.** The M5StickC Plus2's chip has the BR/EDR radio the AtomS3 lacks. See `bt-host` below — Switch-mode 8BitDo controllers (the exact Zero 2 from the investigation above) now work, just not on the AtomS3 and not via this `BleGamepad`/BLE code path.

### GamepadController tuning parameters

Roll/pitch/yaw tuning is **per-drone** via the `GamepadConfig` struct returned by
`DroneProtocolBase::gamepadConfig()`. Each drone overrides the values it needs;
`THROTTLE_RATE_MAX` and `THROTTLE_INIT` are shared (throttle handling is identical
across drones or overridden in `FlightController` for Dr.One anyway).

**Maritaca Force 1 (black drone — default `GamepadConfig`):**

```cpp
// src/comm/drone_protocol_base.h — GamepadConfig default constructor
float deadZone = 0.12f;  // 12% of full scale
float expo     = 0.40f;  // expo curve blending
float slewRate = 8.0f;   // units/frame (~630 ms full throw)
```

**Dr.One (grey drone — `FlowWifiProtocol::gamepadConfig()` override):**

```cpp
// src/comm/flow_wifi_protocol.h
float deadZone = 0.12f;   // same dead zone
float expo     = 0.40f;   // same expo
float slewRate = 128.0f;  // ~instant (≤40 ms full throw)
```

`slewRate=8` causes up to 630 ms of lag after releasing the stick on Dr.One (its
optical-flow flight controller executes the residual command faithfully, unlike the
black drone). `slewRate=128` reaches any target in ≤1 frame (40 ms) — the drone's
internal PID provides the smoothing instead.

**Shared constants in `src/control/gamepad_controller.h`:**

```cpp
static constexpr float THROTTLE_RATE_MAX = 0.6f;   // units/frame throttle change
static constexpr float THROTTLE_INIT     = 128.0f; // throttle value when Flying begins
```

**Throttle (both drones, altitude-hold):** while the iPega left stick UP/DOWN (or
8BitDo ZL/ZR) is deflected beyond `0.05f` (`|throttleUp - throttleDown| > 0.05f`),
`_throttle` ramps by `±THROTTLE_RATE_MAX` per frame. The moment the stick/triggers
return to center, `_throttle` snaps back to `THROTTLE_INIT` (`0x80`) instead of
holding the accumulated value — the drone's altitude hold then maintains the new
altitude. For Dr.One, `FlightController::runState()` additionally overrides
`cs.throttle` directly from the current trigger state (`0x80 + rate * 0x7F`),
independent of this accumulator.

---

## bt-host — Bluepad32 BT Gamepad Build (M5StickC Plus2, branch `support-bluepad32-8bitdo`)

**Working, confirmed flying both drones with a real 8BitDo Zero 2 (2026-06-27).**
**Display/HUD/battery, AccelControl/tilt mode, physical BtnA gestures, and PNG logo
all working (2026-06-29/30). All AccelControl tuning confirmed on real hardware and
BtnB restart wired (2026-07-01)** — see subsections below.

A **separate, parallel firmware build** living at `bt-host/` (repo root,
sibling to `src/`) — not an environment within the main `platformio.ini`/`src/`
project, and not flashed alongside the main firmware. It gives the M5StickC Plus2
(whose ESP32-PICO-V3-02 has a genuine dual-mode BLE+BR/EDR radio, unlike the
AtomS3's BLE-only ESP32-S3) the ability to read 8BitDo/Switch-mode controllers via
**Bluepad32** and fly the drone — something structurally impossible on the AtomS3
(see the 8BitDo Switch-mode section above). Now has full feature parity with the
main firmware's BT gamepad mode, including AccelControl/tilt, mode-select screen,
and physical BtnA button gestures.

### Why a separate build, not a mode in the main firmware

Bluepad32 requires `framework = espidf` (ESP-IDF native, component-based `main/`
project layout), not `framework = arduino` (the main firmware's Arduino-sketch
`src/` layout) — PlatformIO's `src_filter` doesn't work under ESP-IDF, and the two
frameworks expect incompatible source layouts. So this is flashed **instead of**
the main firmware on the same physical board, not a runtime mode switch.

### Architecture: shares the main firmware's logic, doesn't duplicate it

`bt-host/main/CMakeLists.txt` lists `src/comm/{wifi_manager,
drone_protocol,flow_wifi_protocol}.cpp` and `src/control/{gamepad_controller,
accel_controller,flight_controller}.cpp` **directly via relative path**
(`../../src/...`), with `../../src` added to `INCLUDE_DIRS` — one copy of the
protocol/control logic, shared by both builds. Works because that code only needs
`millis()`/`WiFiUdp`/`IPAddress`/`Serial` (present via the `arduino-esp32` core,
included as an ESP-IDF component regardless of framework).

New, build-specific files:

| File | Role |
| --- | --- |
| `bt-host/main/bp32_gamepad.h/.cpp` | `Bp32Gamepad` — mirrors `BleGamepad`'s shape (`begin()`/`update()`/`axes()`), wraps Bluepad32's `ControllerPtr` into the same `GamepadAxes` struct |
| `bt-host/main/sketch.cpp` | `setup()`/`loop()` — full feature parity with main firmware's BT mode; mode-select screen at boot; AccelControl wired to real IMU via `kImu`; BtnA gestures via real `kButton`; board-specific throttle rates via `FlightDeps` |
| `bt-host/main/idf_component.yml` | Fetches `arduino`/`bluepad32` on demand instead of vendoring (see below) |
| `bt-host/components/{btstack,bluepad32_arduino,cmd_nvs,cmd_system,M5Unified,M5GFX}` | Still vendored — see "Vendored vs. fetched dependencies" |

**Axis mapping** (`Bp32Gamepad::update()`) — matches the iPega convention exactly
(confirmed correct by the user after an initial backwards mapping felt wrong on
hardware): left stick X → yaw, left stick Y → throttle up/down (0.08 deadband,
matches `IPEGA_THROTTLE_DEADBAND`); right stick X/Y → roll/pitch. A/B/X/Y map
straight to `GamepadBtn`; D-pad from Bluepad32's `dpad()` bitmask; LT/R1
approximated from L1/R1 shoulder buttons (untested on real hardware — only
A/B/X/Y and axes confirmed so far).

### Display support (M5Unified/M5GFX, added 2026-06-29)

**M5Unified and M5GFX both ship as real ESP-IDF components**, not just
Arduino-framework libraries — their own upstream `CMakeLists.txt` declares plain
ESP-IDF `COMPONENT_REQUIRES` and even has a ready-made, commented-out line for
exactly this situation:

```cmake
### If you use arduino-esp32 components, please activate next comment line.
# list(APPEND COMPONENT_REQUIRES arduino-esp32)
```

That line hardcodes the component name `arduino-esp32`, but this project fetches
the Arduino core under the name `arduino` (matching the upstream Bluepad32
template's convention). Vendored `components/M5Unified` and `components/M5GFX`
(cloned at tags `0.2.17`/`0.2.24` to match the main firmware's `lib_deps`) have
that line changed to `list(APPEND COMPONENT_REQUIRES arduino)` instead — not a
deeper incompatibility, just a naming mismatch between two upstream conventions.
M5Unified's `CMakeLists.txt` also auto-detects a sibling `components/M5GFX`
checkout by that exact capitalized name to decide its own dependency name, so the
vendored folder names matter.

`src/hal/m5stickcplus2.h/.cpp` and `src/ui/display.h/.cpp` are reused **unchanged**
via the same relative-path pattern as the comm/control sources — no new HAL or
display code was needed. `sketch.cpp`'s `loop()` replicates `main.cpp`'s BT-mode
display flow (the `showBtScreen` transition logic, `markDirty()`/`sleep()`/
`wake()`, the Idle+BT axis preview) almost verbatim, mapping `Bp32Status`'s two
states onto the two `BleStatus` values `drawBtStatus()` actually uses (Bluepad32
has no separate "connecting" phase the way the BLE scan/connect callbacks did).

**CJK font data (efont/IPA, ~118MB of vendored *source*) was initially excluded**
on the assumption it would bloat the firmware — turned out to be the wrong call.
`lgfx_fonts.cpp` `#include`s those headers unconditionally with no opt-out macro,
so excluding them is a compile error, not just dead code. Restored them and
measured instead: M5Unified+M5GFX (fonts included) added only **~210KB** to the
compiled firmware (1.34MB → 1.55MB) — verbose C array literal source compiles down
to much more compact binary data than its on-disk text size suggests. Don't assume
source size predicts binary size; measure first.

**Real bug found: WiFi/system event-loop task stack overflow.** Adding
M5Unified/M5GFX (more code, deeper call chains) pushed the event-loop task's
default 2304-byte stack (`CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE`) over the edge —
crashed and rebooted with `assert failed: spinlock_acquire ... (lock->count == 0)`
inside `esp_event_loop_run`, every time, right as the "got IP" WiFi event fired
(decoded via `xtensa-esp32-elf-addr2line` against the build's own `firmware.elf` —
don't guess at a crash site from a raw backtrace when the toolchain can decode it
exactly). Classic stack-overflow-corrupts-adjacent-memory signature: crash site
(deep in FreeRTOS/event-loop internals) had nothing to do with the actual cause
(more stack pressure from unrelated new code elsewhere). Fixed by doubling it in
`sdkconfig.defaults`: `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096`.

### Two coexistence bugs found and fixed (both apply to any future BT+WiFi work)

1. **WiFi reconnect race.** `WifiManager::update()`'s fixed 5 s retry (tuned for
   the AtomS3, no concurrent BT, connects in 1-2 s) fired *while* the first
   connection attempt was still resolving once BT coexistence stretched initial
   WiFi association to 10-25+ s — visible as repeated `STA connect failed!
   0x3007: ESP_ERR_WIFI_CONN`. Fixed with a new, backward-compatible
   `WifiManager::setReconnectInterval(uint32_t ms)` setter in
   `src/comm/wifi_manager.h/.cpp` (default unchanged at 5000 ms — the main
   firmware's behavior is identical either way); `bt-host` calls
   `wifi.setReconnectInterval(30000)` before `begin()`.
2. **UDP sends silently failing even with WiFi shown connected**
   (`endPacket(): could not send data: 12`, roughly every ~800ms matching the Idle
   keepalive cadence) — WiFi modem sleep periodically cedes the radio to BT for
   coexistence, and a send mid-cede fails outright. Fixed with
   `WiFi.setSleep(false)` right after `wifi.begin()` in
   `bt-host/main/sketch.cpp` only (not the shared `WifiManager` — more
   power use for more consistent radio access is a trade-off specific to a build
   that's always running BT concurrently, not the main firmware's optional BT
   mode).

### AccelControl/tilt mode, BtnA gestures, per-board tuning, and BtnB restart (2026-06-30/07-01)

AccelControl mode, the 3-second boot mode-select screen, and physical BtnA button
gestures are all wired up identically to the main firmware (mirroring `main.cpp`)
with two board-specific additions:

**IMU axis correction — full swap + sign (confirmed on real hardware):**
The M5StickC Plus2's MPU6886 is physically rotated 90° relative to the AtomS3
inside the device. After extensive on-hardware testing, the correct mapping
is a full swap with sign corrections in `kImu.getAccel` inside `m5stickcplus2.cpp`:
```cpp
float tmp = *ax;
*ax = -*ay;   // pitch input uses original ay, negated
*ay = tmp;    // roll input uses original ax (no extra negation)
```
This also happens to produce the preferred ergonomic control orientation for holding
the device in portrait mode — forward/back tilt drives pitch, left/right tilt drives
roll, matching the main firmware's AtomS3 feel.

**Per-board AccelControl throttle rates:**
The AtomS3's tuned rates (0.3f up / 0.10f down, ≈7.5 and 2.5 units/sec at 25 Hz)
felt too slow on the StickC Plus2's physical button. `FlightDeps` was extended with
`throttleRateUp`/`throttleRateDown` fields (defaulting to the AtomS3 values via
`DEFAULT_THROTTLE_RATE_UP/DOWN` named constants in `flight_controller.h`) so each
board passes its own rates without touching shared `FlightController` code.
`bt-host` uses `STICKC_THROTTLE_RATE_UP = 1.0f` / `STICKC_THROTTLE_RATE_DOWN = 0.5f`
(≈3× faster, confirmed better feel on hardware).

**BtnB (right-side button) — firmware restart (2026-07-01):**
`kButtonReset` HAL (`src/hal/m5stickcplus2.h/.cpp`) maps `M5.BtnB` to a `ButtonHal`
instance. A single release triggers `ESP.restart()` in `sketch.cpp`. Emergency stop
remains triple-click on BtnA as before — BtnB is purely a soft reset for quick
firmware restarts without needing the power button.

**Per-board AccelControl SLEW_RATE override (2026-07-01):**
Bluepad32 + WiFi coexistence reduces the bt-host loop's effective frame rate to roughly
half the AtomS3's clean 25 Hz, making `SLEW_RATE = 3.0f` feel sluggish on roll/pitch.
`accel_controller.h` uses a `#if defined(BOARD_STICKC_PLUS2)` conditional to set
`SLEW_RATE = 6.0f` for this build only (AtomS3 unchanged at 3.0). Confirmed correct
feel on hardware after testing at 4.5 (still slow) → 6.0 (matches AtomS3 feel).
`YAW_SLEW_RATE` is **not** board-specific — shared value reduced to 4.75 (–5%) for
both boards after hardware testing.

**Portrait mode and PNG logo (2026-06-30):**
The M5StickC Plus2 display in landscape (rotation 1, 240×135) left the screen
feeling wrong for the preferred hand grip. Switched to portrait (rotation 0, 135×240)
in `m5stickcplus2.cpp` (`ROTATION_PORTRAIT = 0`). `display.cpp` updated to
`W = 135` for `BOARD_STICKC_PLUS2` (portrait width, close to AtomS3's 128px, so
the same HUD layout mostly fits) — the extra 112px height below the HUD is used
for a PNG logo (`resources/images/popolon.png`, converted to `src/resources/popolon_png.h`
via a Python script). Drawn via `M5.Display.drawPng()` with explicit scale factors
(`LOGO_SIZE/POPOLON_PNG_W/H`) on both the BT status screen and the flight HUD full
redraws only (`_btScreenReady` / `_needsFullRedraw` gates) — zero impact on the
10 Hz dynamic update ticks or the 100 Hz control loop.

### Vendored vs. fetched dependencies

`bt-host/main/idf_component.yml` declares `arduino`
(`espressif/arduino-esp32`, pinned to commit `ac961f671abd5ae1da0a15fd4bee71ed807c2cf3`)
and `bluepad32` (`gitlab.com/ricardoquesada/bluepad32` — **the real canonical
repo; GitHub's `ricardoquesada/bluepad32` is a stale mirror, last tagged
`release_v3.10.3`** — tag `4.2.0`, `path: src/components/bluepad32`) as git
dependencies, fetched into the gitignored `managed_components/` instead of
vendored. **`btstack` could not be externalized this way** — the vendored copy has
ESP32 port files (`CMakeLists.txt`, `Kconfig`, `btstack_port_esp32*.c`) that don't
exist in raw `bluekitchen/btstack`; it's template-specific wrapper code, stays
vendored (13MB). `bluepad32_arduino` (104KB) has no independent upstream either,
also stays vendored. This cut the repo's vendored footprint from ~79MB to ~14MB.
`M5Unified`/`M5GFX` (added later for display support, ~12MB combined) are also
vendored rather than fetched — both need the same one-line `arduino-esp32` →
`arduino` patch (see "Display support" above), which a registry fetch can't apply.

**Gotcha:** ESP-IDF's component manager does a bare-repo mirror clone into
`~/Library/Caches/Espressif/ComponentManager/` (a fixed user-level cache, **not**
scoped by `PLATFORMIO_CORE_DIR`). On a machine with Git 2.38+'s
`safe.bareRepository=explicit` default this fails outright. Fix it *without*
touching global git config — scope it to just the build command:
`GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=safe.bareRepository GIT_CONFIG_VALUE_0=all`.
If a retry then fails with `git config --get remote.origin.url`, that's stale
half-initialized state from the earlier failure — clear
`~/Library/Caches/Espressif/ComponentManager/` and try again.

### Build & flash

```bash
cd bt-host
PLATFORMIO_CORE_DIR="$(pwd)/.piocore" \
  GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=safe.bareRepository GIT_CONFIG_VALUE_0=all \
  pio run
```

**Always use an isolated `PLATFORMIO_CORE_DIR`** — the pioarduino-forked platform
package this build needs shares the name `"espressif32"` with the official
platform the main firmware uses; installing it into the default `~/.platformio/`
cache silently overwrites the main firmware's platform and breaks its build (this
happened once — fixed via `pio platform uninstall espressif32 && pio platform
install espressif32@7.0.1`. Verify the main firmware's AtomS3 *and* StickC Plus2
environments still build after any global-cache-adjacent operation).

`pio run -t upload` only flashes the app binary under `framework = espidf` — must
flash bootloader+partitions+app together manually after any change:

```bash
esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX --baud 460800 \
  write-flash --flash-mode dio --flash-freq 40m --flash-size 4MB \
  0x1000 .pio/build/esp32dev/bootloader.bin \
  0x8000 .pio/build/esp32dev/partitions.bin \
  0x10000 .pio/build/esp32dev/firmware.bin
```

`pio device monitor` fails in a sandboxed/non-TTY shell
(`termios.error: Operation not supported by device`) — read the serial port
directly with a small pyserial script instead, explicitly setting
`dtr=False; rts=False` before reading (the USB-serial adapter's auto-reset wiring
otherwise holds the chip in reset/bootloader mode while the port is open).

### Other ESP-IDF-native build quirks hit along the way

- Platform's pinned `tool-esptoolpy` (`v5.0.0-dev1`) has a packaging bug —
  override via `platform_packages` to `v5.3.0`.
- `board_build.embed_txtfiles` needed for transitively-pulled `esp_insights`/
  `esp_rainmaker` cert files (a dependency of the `arduino` component, unrelated
  to Bluepad32 itself).
- Combining WiFi + Bluepad32/BTstack overflows IRAM by ~11KB — fix is
  `CONFIG_ESP_WIFI_IRAM_OPT=n` / `CONFIG_ESP_WIFI_RX_IRAM_OPT=n` in
  `sdkconfig.defaults` (ESP-IDF 5.4 dropped the `CONFIG_ESP32_*` prefix these used
  to have — the old name silently no-ops).
- Default 1MB partition is too small once WiFi is added (~1.3MB firmware) — needed
  both `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` *and* PlatformIO's
  `board_build.partitions = partitions.csv`, plus a full `pio run -t clean`
  rebuild (CMake doesn't notice a `partitions.csv` added after the first
  configure pass).

---

## Drone Identification — Confirmed Hardware

The physical drone in use is an **E88 clone / WIFI_8K_ variant**, identified by:

| Property | Value |
| --- | --- |
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
| --- | --- | --- | --- |
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

```text
[ 0x66 | Roll | Pitch | Throttle | Yaw | Cmd | XOR | 0x99 ]
```

| Byte | Value | Description |
| --- | --- | --- |
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
| --- | --- | --- | --- |
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

```text
[ 0xAA | 0x80 | 0x80 | 0x00 | 0x80 | 0x00 | 0x80 | 0x55 ]
```

- Header `0xAA` and footer `0x55` are bitwise complements of `0x66`/`0x99`
- Likely keeps the connection alive so the drone doesn't timeout
- The AtomS3 should send this when not actively controlling

#### Command Byte Flags

| Bit | Value | Function |
| --- | --- | --- |
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

```text
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
| --- | --- | --- | --- |
| Drone IP | `192.168.0.1` | `192.168.1.1` | **`192.168.4.153`** |
| Control port | UDP 50000 | UDP 7099 | **UDP 8090** |
| Packet format | 8-byte `66…99` | ~2 bytes | **8-byte `66…99` (same as E58)** |
| Packet rate | ~20 Hz | Unknown | **~25 Hz** |
| Handshake | Port 40000 | Unknown | **None required** |
| Video | UDP 8800 | RTSP 7070 | **UDP 8080 (TZH/MJPEG)** |
| Keepalive | None documented | None documented | **`AA…55` every ~790 ms** |

---

## FLOW-WIFI Variant — Fully Reverse-Engineered (branch `support-flow-wifi-drone`)

Second physical drone — grey E88 clone with motorised front camera. Different internal MCU and firmware from the black WIFI_8K_ variant.

### FLOW-WIFI Hardware

| Property | Value |
| --- | --- |
| **WiFi SSID** | `FLOW-WIFI-304BA` |
| **Drone IP** | `192.168.169.1` |
| **Client IP (phone)** | `192.168.169.2` (DHCP assigned) |
| **MAC address** | `c4:d7:fd:d5:04:ba` |
| **WiFi channel** | 1 |
| **Android app** | KY UFO |

### FLOW-WIFI Port Map

| Port | Status | Purpose |
| --- | --- | --- |
| UDP **8800** | **CONFIRMED OPEN** | Primary control channel (88-byte packets) |
| UDP 7099 | Open | Secondary keepalive (`01 01` every ~1 s) |

### Control Protocol — UDP 8800 (FULLY reverse-engineered)

The app sends **88-byte** UDP packets to `192.168.169.1:8800` at 25 Hz.

```text
Outer 88-byte payload:
  [ 18 bytes header                    ]
  [ 20-byte inner control packet       ]  ← offset 18
  [ 50 bytes zeros                     ]

Outer header (18 bytes):
  EF 02 | [payload_len LE16=0x0058] | 02 02 | 00 01 | 00 00 | 00 00
  | [seq_counter LE16] | 00 00 | [inner_len LE16=0x0014]

Inner 20-byte control packet:
  [ 0x66 | 0x14 | Roll | Pitch | Throttle | Yaw | Cmd | 0x02 | 10×0x00 | XOR | 0x99 ]
```

| Byte | Value | Description |
| --- | --- | --- |
| 0 | `0x66` | Header (fixed — same as E58) |
| 1 | `0x14` | Inner packet length = 20 |
| 2 | 0–254 | Roll (neutral = `0x80`) |
| 3 | 0–254 | Pitch (neutral = `0x80`) |
| 4 | 0–254 | Throttle (neutral = `0x80`) |
| 5 | 0–254 | Yaw (neutral = `0x80`) |
| 6 | flags | Command byte |
| 7 | `0x02` | Constant |
| 8–17 | `0x00` | Padding (10 bytes) |
| 18 | computed | `Roll ^ Pitch ^ Throttle ^ Yaw ^ Cmd ^ 0x02` |
| 19 | `0x99` | Footer (fixed — same as E58) |

**Checksum confirmed:** `Roll ^ Pitch ^ Throttle ^ Yaw ^ Cmd ^ 0x02` (includes Cmd byte — different from E58 which only XORs axes).

**Command byte flags (confirmed):**

| Value | Constant | Function |
| --- | --- | --- |
| `0x01` | `DroneCmd::TakeOff` | TakeOff / Land toggle — first press = arm+takeoff, second press = land. Hold ~1 s. |
| `0x02` | `DroneCmdEx::EmergStop` | Emergency stop — confirmed from KY UFO PCAP capture. Hold ~1 s. |

**Namespace convention:** `DroneCmd` holds universal E58-family commands. `DroneCmdEx` holds drone-specific extensions not part of the base protocol. New drone-specific commands go in `DroneCmdEx`.

**No arm sequence required.** The drone auto-arms when it receives the TakeOff toggle (0x01). FlowWifiProtocol sets `supportsArmSequence() = false` so FlightController skips Calibrating/Arming and goes directly to Flying, firing TakeOff for 1 s on entry.

**Throttle control (altitude hold):** Grey drone has optical flow altitude hold. Throttle is mapped DIRECTLY (not rate-based): stick center = 0x80 = maintain altitude, UP = climb, DOWN = descend, release = hover. Implemented in `FlightController::runState()` Flying case by overriding `cs.throttle` when `!supportsArmSequence()`.

**Takeoff modes (grey drone):**

- **Button A** → enters Flying + sends TakeOff toggle (0x01) for 1 s → drone auto-takes off HIGH
- **D-pad RIGHT** → enters Flying WITHOUT TakeOff command → user does double-UP manually → lifts off LOW

### FLOW-WIFI Auto-detection

At boot, `WifiManager::scanForFirst()` scans for both known drone SSIDs and returns the index of whichever is found. This runs before the mode-select screen so the UI countdown is not frozen. `FlightController` is then constructed with the matching protocol (`DroneProtocol` or `FlowWifiProtocol`) via `DroneProtocolBase&`.

Key files: `src/comm/drone_protocol_base.h` (abstract interface), `src/comm/flow_wifi_protocol.h/.cpp` (implementation), `WifiManager::scanForFirst()` in `wifi_manager.h/.cpp`.

### Idle Session Maintenance

In Idle (`_state.active = false`), `FlowWifiProtocol::update()` does two things:

1. **8800 heartbeat** — sends one 88-byte control packet every **2 s** (`IDLE_HEARTBEAT_MS`) with `throttle=0x00, cmd=0x00`. This keeps the drone's port-8800 session alive so the first TakeOff command is accepted. Without it the drone ignores the arm command on fresh boot because it has never seen any 8800 traffic.

2. **7099 keepalive** — sends `[0x01, 0x01]` every **1 s** (`KEEPALIVE_INTERVAL_MS`) to the secondary keepalive port, matching KY UFO app behaviour.

`setIdle()` uses `throttle=0x00` (not `0x80`): the heartbeat sends whatever is in `_state`, so `0x80` would trigger altitude-hold re-arm on a grounded drone. `0x00` is safe for a disarmed drone on the ground.

### Secondary Keepalive — UDP 7099

Sent by the KY UFO app at ~1 Hz (now also implemented in `FlowWifiProtocol`):

```text
[ 0x01 | 0x01 ]
```

### PCAP captures

- `resources/pcap/ussnoriko_ch1_2026-05-31_23.45.05.180.pcap` — idle only, 3 button taps
- `resources/pcap/ussnoriko_ch1_2026-06-01_00.01.46.309.pcap` — short flight, axis mapping confirmed
- `resources/pcap/ussnoriko_ch1_2026-06-01_22.57.17.180.pcap` — controlled axis capture (one stick at a time)
- `resources/pcap/ussnoriko_ch1_2026-06-01_23.55.27.327.pcap` — takeoff and landing commands confirmed
- `resources/pcap/ussnoriko_ch1_2026-06-03_22.49.26.078.pcap` — emergency stop command confirmed (0x02)

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
