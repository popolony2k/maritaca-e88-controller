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
/// Native panel is 135(w)x240(h) portrait at rotation 0; rotation 1 gives a
/// 240x135 landscape framebuffer to match this project's HUD orientation.
/// TODO: confirm on physical hardware which rotation reads "upright" given
/// how the unit will actually be held — flagged in the porting plan.
static constexpr uint8_t ROTATION_LANDSCAPE = 1;

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
        M5.Display.setRotation(ROTATION_LANDSCAPE);
        M5.Display.setTextSize(DEFAULT_TEXT_SIZE);
        M5.Display.setTextDatum(TL_DATUM);
    },
    .setBrightness = [](uint8_t v)                                   { M5.Display.setBrightness(v); },
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

// BtnB (side button) is intentionally unused — this first port keeps the
// existing single-button gesture scheme identical to the AtomS3. Wiring
// BtnB in as a dedicated control is a deliberate follow-up, not part of
// keeping behavior unchanged during the initial port.
const ButtonHal kButton {
    .wasPressed  = []()            -> bool { return M5.BtnA.wasPressed(); },
    .wasReleased = []()            -> bool { return M5.BtnA.wasReleased(); },
    .pressedFor  = [](uint32_t ms) -> bool { return (bool)M5.BtnA.pressedFor(ms); },
};
