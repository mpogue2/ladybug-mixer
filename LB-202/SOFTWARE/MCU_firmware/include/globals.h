#ifndef __GLOBALS_H__
#define __GLOBALS_H__

// FEATURE SELECTION ---------
// #define DEBUG 1
#define INCLUDE_PREFERENCES

// #define INCLUDE_TEST_POINT

// CLOCK DIVIDER CONFIGURATION ---------
// NOTE: __CONF_CLKDIV is set in platformio.ini to ensure all source files
// see the same value during compilation (important for SYS_Delay calibration)
//
// Current setting (see platformio.ini):
//   __CONF_CLKDIV = 0x00  =>  No division (17.5 MHz) - DEBUG mode
//   __CONF_CLKDIV = 0x02  =>  Divide by 2 (8.75 MHz) - STEP 1 ✓ (5.4mA)
//   __CONF_CLKDIV = 0x04  =>  Divide by 4 (4.375 MHz) - STEP 2 (testing...)

// FIRMWARE VERSION ---------
#define FW_MAJOR 1
#define FW_MINOR 1
#define FW_PATCH 1

// VERSION DISPLAY TIMING ---------
// Scaling factor for version display delays (0.5 = twice as fast, 2.0 = half as
// fast)

#ifdef DEBUG
// Slower version display when in debug mode (MCU clock is fast)
  #define VERSION_DISPLAY_TIMING_SCALE 1
#else
// Faster version display when not in debug mode
  #define VERSION_DISPLAY_TIMING_SCALE 0.5
#endif

// for unused variables
#define UNUSED(x) (void)(x)

#endif // __GLOBALS_H__
