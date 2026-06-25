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
#include "hal.h"

/**
 * @brief Concrete HAL instances for the M5StickC Plus2.
 *
 * Include this header only in main.cpp (selected at build time via the
 * BOARD_STICKC_PLUS2 macro — see platformio.ini). All other modules receive
 * HAL references via constructor injection, keeping M5Unified out of the
 * business-logic files. Mutually exclusive with m5atoms3.h/.cpp — both
 * define identically-named globals, so only one may be compiled per
 * PlatformIO environment (enforced by build_src_filter).
 */
extern const BoardHal   kBoard;   ///< Board lifecycle and power management (built-in PMIC battery).
extern const DisplayHal kDisplay; ///< 135×240 LCD (ST7789-family), rotated for landscape HUD.
extern const ImuHal     kImu;     ///< Built-in accelerometer + gyroscope.
extern const ButtonHal  kButton;  ///< Front button (BtnA) — side button (BtnB) intentionally unused.
