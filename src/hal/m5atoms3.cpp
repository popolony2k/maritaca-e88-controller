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

const BoardHal kBoard {
    .begin  = [] { auto cfg = M5.config(); M5.begin(cfg); },
    .update = [] { M5.update(); },
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
    .wasReleased = []()            -> bool { return M5.BtnA.wasReleased(); },
    .pressedFor  = [](uint32_t ms) -> bool { return (bool)M5.BtnA.pressedFor(ms); },
};
