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
#include <M5Unified.h>
#include "m5stickcplus2.h"

/**
 * @file m5stickcplus2.cpp
 * @brief HAL implementation for the M5StickC Plus2. Mutually exclusive with
 * m5atoms3.cpp (see platformio.ini build_src_filter) — only one of the two
 * is ever compiled into a given firmware image.
 *
 * Unlike the AtomS3 (paired with the Atomic Battery Base, whose power IC
 * exposes no I2C), the M5StickC Plus2 has a built-in battery behind a real
 * PMIC that M5Unified can query directly — no custom ADC voltage-divider
 * code needed here at all.
 */

/// LovyanGFX rotation constants (0=0°, 1=90°CW, 2=180°, 3=270°CW).
/// Native panel is 135(w)x240(h) portrait at rotation 0.
/// Rotation 0 = portrait (135×240), 90°CCW from previous landscape orientation —
/// confirmed preferred ergonomic hold on real hardware.
static constexpr uint8_t ROTATION_PORTRAIT = 0;

static constexpr uint8_t DEFAULT_BRIGHTNESS = 128; ///< Initial backlight level on begin().
static constexpr uint8_t DEFAULT_TEXT_SIZE  =   1;  ///< Normal (1x) text scale.

const BoardHal kBoard {
    .begin           = [] { auto cfg = M5.config(); M5.begin(cfg); },
    .update          = [] { M5.update(); },
    .getBatteryLevel = [] { return (int)M5.Power.getBatteryLevel(); },
    .isCharging      = [] { return M5.Power.isCharging() == m5::Power_Class::is_charging; },
};

const DisplayHal kDisplay {
    .begin        = [] {
        M5.Display.setBrightness(DEFAULT_BRIGHTNESS);
        M5.Display.setRotation(ROTATION_PORTRAIT);
        M5.Display.setTextSize(DEFAULT_TEXT_SIZE);
        M5.Display.setTextDatum(TL_DATUM);
    },
    .setBrightness = [](uint8_t v)                                   { M5.Display.setBrightness(v); },
    .fillScreen   = [](uint16_t c)                             { M5.Display.fillScreen(c); },
    .fillRect     = [](int x, int y, int w, int h, uint16_t c) { M5.Display.fillRect(x, y, w, h, c); },
    .drawRect     = [](int x, int y, int w, int h, uint16_t c) { M5.Display.drawRect(x, y, w, h, c); },
    .setTextColor = [](uint16_t fg, uint16_t bg)               { M5.Display.setTextColor(fg, bg); },
    .drawString   = [](const char* s, int x, int y)            { M5.Display.drawString(s, x, y); },
    .drawPng      = [](const uint8_t* d, size_t len, int x, int y, int w, int h, float sx, float sy) {
                        M5.Display.drawPng(d, len, x, y, w, h, 0, 0, sx, sy); },
};

const ImuHal kImu {
    // Both X and Y axes are swapped+negated vs the AtomS3: the StickC Plus2
    // MPU6886 is physically rotated 90° — swapping ax/ay remaps the tilt
    // gesture that previously drove roll to drive pitch and vice versa,
    // matching the ergonomic hold orientation the user confirmed on hardware.
    .getAccel = [](float* ax, float* ay, float* az) -> bool {
        bool ok = M5.Imu.getAccel(ax, ay, az);
        if (ok) {
            float tmp = *ax;
            *ax = -*ay;   // pitch input now uses original ay (negated)
            *ay = tmp;    // roll input now uses original ax (sign confirmed on hardware)
        }
        return ok;
    },
    .getGyro  = [](float* gx, float* gy, float* gz) -> bool { return M5.Imu.getGyro(gx, gy, gz); },
};

const ButtonHal kButton {
    .wasPressed  = []()            -> bool { return M5.BtnA.wasPressed(); },
    .wasReleased = []()            -> bool { return M5.BtnA.wasReleased(); },
    .pressedFor  = [](uint32_t ms) -> bool { return (bool)M5.BtnA.pressedFor(ms); },
};

// BtnB (right side button) — wired as a dedicated emergency stop so a single
// press can cut motors without requiring the triple-click gesture on BtnA.
const ButtonHal kButtonReset {
    .wasPressed  = []()            -> bool { return M5.BtnB.wasPressed(); },
    .wasReleased = []()            -> bool { return M5.BtnB.wasReleased(); },
    .pressedFor  = [](uint32_t ms) -> bool { return (bool)M5.BtnB.pressedFor(ms); },
};
