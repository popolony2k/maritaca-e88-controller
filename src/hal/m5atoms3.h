#pragma once
#include "hal.h"

// Concrete HAL implementation for M5Stack AtomS3.
// Include this header in main.cpp only — keep M5Unified out of all other files.

extern const BoardHal   kBoard;
extern const DisplayHal kDisplay;
extern const ImuHal     kImu;
extern const ButtonHal  kButton;
