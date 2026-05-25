#pragma once
#include <Arduino.h>

// Standard RGB565 color constants — board-agnostic
namespace Rgb565 {
    constexpr uint16_t Black    = 0x0000;
    constexpr uint16_t White    = 0xFFFF;
    constexpr uint16_t Red      = 0xF800;
    constexpr uint16_t Green    = 0x07E0;
    constexpr uint16_t Cyan     = 0x07FF;
    constexpr uint16_t Yellow   = 0xFFE0;
    constexpr uint16_t Orange   = 0xFDA0;
    constexpr uint16_t DarkGrey = 0x7BEF;
    constexpr uint16_t Navy     = 0x000F;
}

// Board lifecycle
struct BoardHal {
    void (*begin)  ();
    void (*update) ();
};

// Display surface — all setup (brightness, rotation, text size) is done in begin()
struct DisplayHal {
    void (*begin)        ();
    void (*fillScreen)   (uint16_t color);
    void (*fillRect)     (int x, int y, int w, int h, uint16_t color);
    void (*drawRect)     (int x, int y, int w, int h, uint16_t color);
    void (*setTextColor) (uint16_t fg, uint16_t bg);
    void (*drawString)   (const char* s, int x, int y);
};

// IMU sensor
struct ImuHal {
    bool (*getAccel) (float* ax, float* ay, float* az);
    bool (*getGyro)  (float* gx, float* gy, float* gz);
};

// Button input
struct ButtonHal {
    bool (*wasReleased) ();
    bool (*pressedFor)  (uint32_t ms);
};
