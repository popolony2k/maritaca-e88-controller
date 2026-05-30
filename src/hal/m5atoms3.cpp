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
 * @defgroup ip5306 IP5306 Battery IC (Atomic Battery Base)
 *
 * Direct I2C access via M5Unified's internal bus. M5Unified does not expose
 * battery level for the AtomS3 + Atomic Battery Base combination, so we read
 * the IP5306 registers directly.
 *
 * Note: REG_STA1 level only updates at power-on or physical button press —
 * it is a snapshot, not a live reading.
 * @{
 */
static constexpr uint8_t  IP5306_ADDR     = 0x75;
static constexpr uint8_t  IP5306_REG_STA0 = 0x70; ///< Bit 3: charging flag.
static constexpr uint8_t  IP5306_REG_STA1 = 0x78; ///< Bits [2:1]: 00=25% 01=50% 10=75% 11=100%.
static constexpr uint32_t IP5306_I2C_FREQ = 400000;

/**
 * @brief Read a single register from the IP5306 via M5Unified's I2C bus.
 * @param reg Register address.
 * @return Register value, or -1 on bus error.
 */
static int ip5306ReadReg(uint8_t reg) {
    return M5.In_I2C.readRegister8(IP5306_ADDR, reg, IP5306_I2C_FREQ);
}

/**
 * @brief Read the battery charge level from the IP5306.
 * @return One of 25, 50, 75, 100 (percent), or -1 if the IC is unreachable.
 */
static int ip5306BatteryLevel() {
    int val = ip5306ReadReg(IP5306_REG_STA1);
    if (val < 0) return -1;
    static constexpr int levels[] = { 25, 50, 75, 100 };
    return levels[(val >> 1) & 0x03];
}

/**
 * @brief Check whether the battery is currently charging.
 * @return True when VBUS charging current is flowing (bit 3 of REG_STA0).
 */
static bool ip5306IsCharging() {
    int val = ip5306ReadReg(IP5306_REG_STA0);
    if (val < 0) return false;
    return (val & 0x08) != 0;
}
/** @} */

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
