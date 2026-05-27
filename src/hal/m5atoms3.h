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
 * @brief Concrete HAL instances for the M5Stack AtomS3.
 *
 * Include this header only in main.cpp. All other modules receive HAL
 * references via constructor injection, keeping M5Unified out of the
 * business-logic files.
 */
extern const BoardHal   kBoard;   ///< Board lifecycle and power management.
extern const DisplayHal kDisplay; ///< 128×128 GC9107 LCD, rotated 270°.
extern const ImuHal     kImu;     ///< Built-in accelerometer + gyroscope.
extern const ButtonHal  kButton;  ///< Front screen button (BtnA, GPIO 41).
