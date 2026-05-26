#include <Arduino.h>
#include <M5Unified.h>
#include "m5atoms3.h"

// The only file in the project that includes M5Unified.h.
// All function pointers are non-capturing lambdas — they decay to raw
// function pointers at compile time with zero runtime overhead.

// LovyanGFX rotation values: 0=0°, 1=90°CW, 2=180°, 3=270°CW.
static constexpr uint8_t ROTATION_0   = 0;
static constexpr uint8_t ROTATION_90  = 1;
static constexpr uint8_t ROTATION_180 = 2;
static constexpr uint8_t ROTATION_270 = 3;

// Direct IP5306 I2C access via M5Unified's internal bus — M5Unified does
// not expose battery level for the AtomS3 + Atomic Battery Base combination.
// IP5306 address: 0x75. REG_READ0=0x70 (charging), REG_READ1=0x78 (level).
static constexpr uint8_t  IP5306_ADDR     = 0x75;
static constexpr uint8_t  IP5306_REG_STA0 = 0x70;
static constexpr uint8_t  IP5306_REG_STA1 = 0x78;
static constexpr uint32_t IP5306_I2C_FREQ = 400000;

static int ip5306ReadReg(uint8_t reg) {
    return M5.In_I2C.readRegister8(IP5306_ADDR, reg, IP5306_I2C_FREQ);
}

static int ip5306BatteryLevel() {
    int val = ip5306ReadReg(IP5306_REG_STA1);
    if (val < 0) return -1;
    // bits [2:1]: 00=25%, 01=50%, 10=75%, 11=100%
    static constexpr int levels[] = { 25, 50, 75, 100 };
    return levels[(val >> 1) & 0x03];
}

static bool ip5306IsCharging() {
    int val = ip5306ReadReg(IP5306_REG_STA0);
    if (val < 0) return false;
    return (val & 0x08) != 0;  // bit 3: VBUS charging current flowing
}

const BoardHal kBoard {
    .begin           = [] { auto cfg = M5.config(); M5.begin(cfg); },
    .update          = [] { M5.update(); },
    .getBatteryLevel = [] { return ip5306BatteryLevel(); },
    .isCharging      = [] { return ip5306IsCharging(); },
};

const DisplayHal kDisplay {
    .begin        = [] {
        M5.Display.setBrightness(128);
        M5.Display.setRotation(ROTATION_270);
        M5.Display.setTextSize(1);
        M5.Display.setTextDatum(TL_DATUM);
    },
    .fillScreen   = [](uint16_t c)                             { M5.Display.fillScreen(c); },
    .fillRect     = [](int x, int y, int w, int h, uint16_t c) { M5.Display.fillRect(x, y, w, h, c); },
    .drawRect     = [](int x, int y, int w, int h, uint16_t c) { M5.Display.drawRect(x, y, w, h, c); },
    .setTextColor = [](uint16_t fg, uint16_t bg)               { M5.Display.setTextColor(fg, bg); },
    .drawString   = [](const char* s, int x, int y)            { M5.Display.drawString(s, x, y); },
};

const ImuHal kImu {
    .getAccel = [](float* ax, float* ay, float* az) -> bool { return M5.Imu.getAccel(ax, ay, az); },
    .getGyro  = [](float* gx, float* gy, float* gz) -> bool { return M5.Imu.getGyro(gx, gy, gz); },
};

const ButtonHal kButton {
    .wasPressed  = []()            -> bool { return M5.BtnA.wasPressed(); },
    .wasReleased = []()            -> bool { return M5.BtnA.wasReleased(); },
    .pressedFor  = [](uint32_t ms) -> bool { return (bool)M5.BtnA.pressedFor(ms); },
};
