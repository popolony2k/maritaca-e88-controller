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
