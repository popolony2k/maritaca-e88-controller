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
#pragma once
#include <Arduino.h>

/**
 * @brief Board-agnostic RGB565 color constants.
 */
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

/**
 * @brief Hardware abstraction for board lifecycle and power management.
 *
 * Implemented as a struct of function pointers filled with non-capturing
 * lambdas in m5atoms3.cpp, giving zero runtime overhead (decay to raw
 * function pointers at compile time).
 */
struct BoardHal {
    void (*begin)           ();                ///< Board init — call once in setup().
    void (*update)          ();                ///< Poll button state — call every loop().
    int  (*getBatteryLevel) ();                ///< Battery level: 0/25/50/75/100 %, or -1 if unavailable.
    bool (*isCharging)      ();                ///< True when VBUS charging current is flowing.
};

/**
 * @brief Hardware abstraction for the 128×128 LCD display.
 *
 * All one-time setup (brightness, rotation, text size) is performed in begin().
 * Callers only draw — they never configure the display directly.
 */
struct DisplayHal {
    void (*begin)        ();                                          ///< Display init — call once in setup().
    void (*setBrightness)(uint8_t level);                            ///< Set backlight brightness (0 = off, 128 = default).
    void (*fillScreen)   (uint16_t color);                           ///< Fill entire screen with a solid color.
    void (*fillRect)     (int x, int y, int w, int h, uint16_t color); ///< Fill a rectangle.
    void (*drawRect)     (int x, int y, int w, int h, uint16_t color); ///< Draw a rectangle outline.
    void (*setTextColor) (uint16_t fg, uint16_t bg);                 ///< Set foreground and background text colors.
    void (*drawString)   (const char* s, int x, int y);              ///< Draw a string at pixel coordinates (x, y).
};

/**
 * @brief Hardware abstraction for the IMU sensor (accelerometer + gyroscope).
 */
struct ImuHal {
    bool (*getAccel) (float* ax, float* ay, float* az); ///< Read accelerometer (g). Returns false on failure.
    bool (*getGyro)  (float* gx, float* gy, float* gz); ///< Read gyroscope (deg/s). Returns false on failure.
};

/**
 * @brief Hardware abstraction for a single button input.
 */
struct ButtonHal {
    bool (*wasPressed)  ();             ///< True for one loop after button is pressed.
    bool (*wasReleased) ();             ///< True for one loop after button is released.
    bool (*pressedFor)  (uint32_t ms); ///< True if button has been held for at least @p ms milliseconds.
};
