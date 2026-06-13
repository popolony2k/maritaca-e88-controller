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
#include "m5atoms3.h"

/**
 * @file m5atoms3.cpp
 * @brief The only file in the project that includes M5Unified.h.
 *
 * All HAL function pointers are non-capturing lambdas that decay to raw
 * function pointers at compile time, giving zero runtime overhead.
 */

/// LovyanGFX rotation constants (0=0°, 1=90°CW, 2=180°, 3=270°CW).
static constexpr uint8_t ROTATION_270 = 3;

/**
 * @defgroup battery_adc Battery voltage sense (Atomic Battery Base)
 *
 * The Atomic Battery Base's power IC (board sticker: ETA9085, not a genuine
 * IP5306) exposes no I2C registers — a full bus scan of both M5.In_I2C and
 * M5.Ex_I2C found nothing at 0x75 (only the onboard IMU at 0x68 on In_I2C).
 *
 * Battery voltage is instead sensed on GPIO8 ("ADC2" in the AtomS3
 * pins_arduino.h) through an onboard divider. ADC_DIVIDER_RATIO was derived
 * empirically from two points against the base's 4-LED indicator (battery on
 * power only, no USB):
 *   - 3 LEDs (75%): A2 ~1776 mV -> VBAT = 2.06 * 1.776 = 3.66 V
 *   - 2 LEDs (50%): A2 ~1726 mV -> VBAT = 2.06 * 1.726 = 3.56 V
 * Both land in their expected bucket below.
 *
 * batteryLevel() applies asymmetric hysteresis around each bucket boundary:
 * the level descends immediately at the raw boundary (no lag on low-battery
 * warnings — confirmed against the base's LED indicator: 1 LED / 25% lit at
 * VBAT ~3.45 V, below the 3.48 V boundary), but requires the voltage to climb
 * back past the boundary by BATTERY_HYSTERESIS before ascending again. This
 * prevents bounce-back flicker when the voltage recovers slightly (noise or
 * reduced load) without lagging behind a real discharge.
 * @{
 */
static constexpr uint8_t ADC_SAMPLES        = 8;     ///< Averaged per read to smooth ~10 mV ADC jitter.
static constexpr float   ADC_DIVIDER_RATIO  = 2.06f; ///< Empirically derived — see calibration notes above.
static constexpr float   BATTERY_HYSTERESIS = 0.03f; ///< Volts a boundary must be re-crossed by before the level changes.

/**
 * @brief Read the battery cell voltage via the GPIO8 divider.
 * @return Battery voltage in volts (averaged over ADC_SAMPLES reads).
 */
static float batteryVoltage() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; i++) sum += analogReadMilliVolts(ADC2);
    return (sum / (float)ADC_SAMPLES) / 1000.0f * ADC_DIVIDER_RATIO;
}

/**
 * @brief Read the battery charge level from the GPIO8 voltage divider.
 * @return One of 0, 25, 50, 75, 100 (percent). Asymmetric hysteresis
 *         (BATTERY_HYSTERESIS) prevents bounce-back flicker on recovery
 *         without lagging behind a real discharge.
 */
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
/** @} */

const BoardHal kBoard {
    .begin           = [] { auto cfg = M5.config(); M5.begin(cfg); },
    .update          = [] { M5.update(); },
    .getBatteryLevel = [] { return batteryLevel(); },
    .isCharging      = [] { return false; },  // no charging-status signal available on this hardware
};

const DisplayHal kDisplay {
    .begin        = [] {
        M5.Display.setBrightness(128);
        M5.Display.setRotation(ROTATION_270);
        M5.Display.setTextSize(1);
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

const ButtonHal kButton {
    .wasPressed  = []()            -> bool { return M5.BtnA.wasPressed(); },
    .wasReleased = []()            -> bool { return M5.BtnA.wasReleased(); },
    .pressedFor  = [](uint32_t ms) -> bool { return (bool)M5.BtnA.pressedFor(ms); },
};
