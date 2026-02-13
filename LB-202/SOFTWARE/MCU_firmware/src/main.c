// +-----------------------------------------------+
// | MAINBOARD1: Firmware for Mixer Board V3.0     |
// |                                               |
// | Copyright (c) 2025,2026 Michael Pogue         |
// | License: GPL V3                               |
// +-----------------------------------------------+

#include "fw_hal.h"
#include "globals.h"
#include <stdint.h>

#ifdef INCLUDE_PREFERENCES
#include "preferences.h"
#endif

/*
 * ============================================================================
 * HARDWARE PINOUT - STC8G1K08-QFN20 Custom Audio Mixer Board
 * ============================================================================
 *
 * MCU: STC8G1K08-20/16PIN
 * Package: QFN20
 * Clock: 17.5 MHz (internal RC oscillator)
 * Flash: 8 KB code + 4 KB EEPROM (IAP)
 * RAM: 1.25 KB
 *
 * PIN ASSIGNMENTS:
 * ----------------
 * P1.1 (Pin 20) - I2C SCL (SSD1306 OLED Display) - Output Push-Pull
 * P1.2 (Pin 19) - I2C SDA (SSD1306 OLED Display) - Quasi-Bidirectional
 *
 * P1.5 (Pin  1) - Switch 1 Input (Active Low, Internal Pullup) - Input HIP
 * P1.6 (Pin  2) - Switch 2 Input (Active Low, Internal Pullup) - Input HIP
 *
 * P3.5 (Pin 13) - PWM CCP0 (Blue LED, Active Low)  - Output Push-Pull
 * P3.6 (Pin 14) - PWM CCP1 (Green LED, Active Low) - Output Push-Pull
 * P3.7 (Pin 15) - PWM CCP2 (Red LED, Active Low)   - Output Push-Pull
 *
 * P1.0 (Pin 16) - BATTMON (Battery Monitor Input, 0-2.7V) - Input ADC0
 * P1.4 (Pin 17) - OUTMON (Audio Signal Input, 0-5V) - Input ADC4
 *
 * P1.7 (Pin  3) - VOL_ADC (Remote Volume Control Input, 0-3.3V) - Input ADC7
 * P3.2 (Pin 10) - VOL_CLK  (Attenuator) - Output Push-Pull
 * P3.3 (Pin 11) - VOL_DATA (Attenuator) - Output Push-Pull
 * P3.4 (Pin 12) - VOL_LOAD (Attenuator) - Output Push-Pull
 *
 * DEBUG (when DEBUG is defined):
 * P3.0 (Pin 8)  - UART1 RX (115200 baud)
 * P3.1 (Pin 9)  - UART1 TX (115200 baud)
 * P1.3 (Pin 18) - TEST OUTPUT (Toggled in main loop)
 *
 * DISPLAY (when INCLUDE_DISPLAY is defined):
 * - SSD1306 OLED: 128x32 pixels, I2C address 0x78
 * - I2C Clock: ~265 KHz (safe for SSD1306)
 *
 * EEPROM (when INCLUDE_PREFERENCES is defined):
 * - Last 512-byte sector (0x0E00-0x0FFF) reserved for preferences
 * - Log-structured storage with wear leveling
 *
 * POWER MANAGEMENT:
 * - IDLE mode enabled in main loop (CPU stops, peripherals continue)
 * - Clock divider = 4 in non-DEBUG builds (quarter speed for power saving)
 * - Timer0 interrupt wakes CPU at 100 Hz
 *
 * ============================================================================
 */

#define TIMER_FREQUENCY_HZ 100
#define RVC_UPDATE_FREQUENCY_TICKS                                             \
  5 // Remote Volume Control update frequency (in timer ticks = 20X per second)
// NOTE: RVC_UPDATE_FREQUENCY_TICKS must be a divisor of TIMER_FREQUENCY_HZ

// PIN DEFINITIONS --------------------
#define PIN_BATTMON P10         // P1.0 - Battery Monitor ADC Input
#define ADCCHANNEL_BATTMON 0x00 // ADC Channel 0

#define PIN_TP1 P13         // P1.3 = test point for timing testing

#define PIN_RVC P17         // P1.7 - Remote Volume Control ADC Input
#define ADCCHANNEL_RVC 0x07 // ADC Channel 7

#define PIN_OUTMON P14      // P1.4 - Audio Output Monitor ADC Input
#define ADCCHANNEL_OUTMON 4 // ADC Channel 4

#define PIN_ATTEN_CLK P32  // P3.2 - Attenuator CLK
#define PIN_ATTEN_DATA P33 // P3.3 - Attenuator DATA
#define PIN_ATTEN_LOAD P34 // P3.4 - Attenuator LOAD

// Switch debouncing configuration
#define SWITCH_DEBOUNCE_COUNT                                                  \
  5 // Number of consistent reads required (50ms at 100Hz)
#define SWITCH_1_PIN GPIO_Pin_5
#define SWITCH_2_PIN GPIO_Pin_6

// LED calibration factors (relative to blue = 1.0)
#define LED_RED_CALIBRATION 0x49   // Red:   73/255 = 0.286
#define LED_GREEN_CALIBRATION 0x30 // Green: 48/255 = 0.188
#define LED_BLUE_CALIBRATION                                                   \
  0xFF // Blue: 255/255 = 1.0 (reference is the dimmest LED visually)

// LED override flash color for RVC mode indication
#define LED_OVERRIDE_FLASH_RED 0
#define LED_OVERRIDE_FLASH_GREEN LED_GREEN_CALIBRATION
#define LED_OVERRIDE_FLASH_BLUE 0

// +---------------------------------------------------------------+
// | GLOBAL VARIABLES                                              |
// +---------------------------------------------------------------+

// NOTE: BE CAREFUL WITH GLOBAL RAM USAGE!  DIRECT PAGE RAM IS ONLY 256 BYTES,
//  AND MOST OF THAT RAM IS ALREADY SPOKEN FOR.  If you get a linker error,
//  consider moving globals to XDATA, where there is 1K of (slower) RAM available,
//  or change globals to bitfields where possible.

// Remote Volume Control variables -------------------
uint8_t res = 0;         // attenuation result
uint8_t previousRes = 0; // previous attenuation result

typedef enum {
  RVC_DEFAULT_MODE_WITH_MUTE = 0,     // default curve (with MUTE)
  RVC_TRADITIONAL_MA220_MODE = 1      // traditional MA-220 curve (no MUTE)
} rvc_mode_t;

rvc_mode_t rvc_mode = RVC_DEFAULT_MODE_WITH_MUTE;

// Output monitor variables ---------------------------
// uint8_t OUTMONres = 0; // latest result of output audio monitor sampling

// TIMER VARIABLES
volatile uint8_t timer_ticks = 0; // cycles from 0 - TIMER_FREQUENCY_HZ

// RGB LED color cycling variables
volatile bool pulseDirectionUP = true;
volatile uint8_t red = 0;
// volatile uint8_t green = 0;            // NOT USED
// volatile uint8_t blue = 0;             // NOT USED

typedef enum {
  // IMPLEMENTED NOW:
  BATTERY_MONITOR_MODE = 0,
  VU_METER_MODE = 1,

  // SOLID MODES:
  SOLID_RED_MODE = 2,
  SOLID_GREEN_MODE = 3,
  SOLID_BLUE_MODE = 4,
  SOLID_WHITE_MODE = 5,

  EDIT_SW1_MODE = 6,
  EDIT_SW2_MODE = 7
} led_mode_t;

volatile led_mode_t led_mode = BATTERY_MONITOR_MODE; // current LED mode
#define LAST_LED_MODE SOLID_WHITE_MODE

// LED override mechanism for temporary patterns (e.g., RVC mode change indicator)
volatile uint8_t led_override_active = 0;      // 0 = normal operation, 1 = override active
volatile uint8_t led_override_state = 0;       // Current state in override pattern
volatile uint8_t led_override_tick_counter = 0; // Tick counter for timing within states
volatile uint8_t led_override_flash_count = 0; // Number of flashes to display (1 or 2)
volatile uint8_t led_override_flash_current = 0; // Current flash number (0-indexed)

// VU METER COLOR CALCULATION
// Smooth color transitions based on brightness value b (0-255)
// Green zone (b=0-191): Pure green, increasing brightness
// Yellow zone (b=192-223): Transition from green to yellow
// Red zone (b=224-255): Red only, increasing brightness
void calculate_vu_color(uint8_t b, uint8_t *r, uint8_t *g, uint8_t *blue) {
  if (b < 192) {
    // Green zone: b = 0-191
    // Linear interpolation from (0,0,0) to (0,33,0)
    *r = 0;
    *g = ((uint16_t)b * 33) / 191;
    *blue = 0;
  } else if (b < 224) {
    // Yellow zone: b = 192-223
    // Linear interpolation from (0,33,0) to (50,40,0)
    uint16_t t = b - 192;  // 0 to 31
    *r = ((uint16_t)t * 50) / 31;
    *g = 33 + ((uint16_t)t * 7) / 31;
    *blue = 0;
  } else {
    // Red zone: b = 224-255
    // Linear interpolation from (100,0,0) to (255,0,0)
    uint16_t t = b - 224;  // 0 to 31
    *r = 100 + ((uint16_t)t * 155) / 31;
    *g = 0;
    *blue = 0;
  }
}

// BATTERY MONITOR VARIABLES
uint8_t battmon_res = 255; // latest result of battery monitor sampling (255 = NOT SET YET)

#define GREEN_WATERMARK 99  // above this value, solid green
#define YELLOW_WATERMARK 92 // above this value, solid yellow
#define RED_WATERMARK 84 // above this value, solid red; below this, pulsing red

// VU METER VARIABLES
uint8_t abs_out_res =
    0; // absolute value of output monitor result centered around 0x80

uint8_t window[4] = {0, 0, 0, 0}; // window for peak detection
uint8_t window_index = 0; // index for peak detection window

// VU Meter configuration
// 1.0 second decay from full scale (at 20Hz)
#define VU_METER_DECAY_SAMPLES 20
#define VU_METER_FULL_SCALE 127 // Maximum possible value for abs_out_res
// Fixed point math: keep 8 bits of fractional precision for smooth decay
static uint16_t vu_display_val_fixed = 0;

// Logarithmic Volume Table (0-127 input -> 0-255 brightness/loudness)
// Maps linear signal amplitude to perceived loudness (approx 40dB range)
// Method: dB = 20 * log10(x/127). Map -40dB..0dB to 0..255.
const uint8_t volume_log_table[128] = {
    0,   16,   25,  48,  64,  76,  86,  95,  102, 108, 114, 120, 124, 129, 133,
    137, 140, 144, 147, 150, 153, 155, 158, 160, 163, 165, 167, 169, 171, 173,
    175, 177, 179, 180, 182, 184, 185, 187, 188, 190, 191, 192, 194, 195, 196,
    198, 199, 200, 201, 202, 203, 204, 206, 207, 208, 209, 210, 211, 212, 213,
    213, 214, 215, 216, 217, 218, 219, 220, 220, 221, 222, 223, 224, 224, 225,
    226, 227, 227, 228, 229, 229, 230, 231, 231, 232, 233, 233, 234, 235, 235,
    236, 237, 237, 238, 238, 239, 240, 240, 241, 241, 242, 242, 243, 243, 244,
    244, 245, 246, 246, 247, 247, 248, 248, 249, 249, 250, 250, 250, 251, 251,
    252, 252, 253, 253, 254, 254, 255, 255};
// END: Log Curve

// PREFERENCES VARIABLES
#ifdef INCLUDE_PREFERENCES
Preferences_t prefs; // preferences that were persisted in EEPROM
#endif

// RVC VARIABLES
// bool right_handed_volume_control = true; // default to right-handed

// Switches debouncing state (SW1, SW2)
volatile uint8_t switch1_debounce_counter = 0;
volatile uint8_t switch2_debounce_counter = 0;
volatile uint8_t switch1_state =
    1; // 1 = not pressed (pulled high), 0 = pressed
volatile uint8_t switch2_state =
    1; // 1 = not pressed (pulled high), 0 = pressed

// UTILS ================================
#define nop() __asm__(" nop");

#ifdef INCLUDE_TEST_POINT
// TEST POINT ===========================
void init_test_point() {
  GPIO_P1_SetMode(GPIO_Pin_3, GPIO_Mode_Output_PP);
  PIN_TP1 = 0; // initial state is LOW
}
#endif

// +---------------------------------------------------------------+
// | VU METER FUNCTIONS                                            |
// +---------------------------------------------------------------+
// Configure ADC4 (P1.4 - OUTMON) for audio signal monitoring
void init_VU_meter() {
  GPIO_P1_SetMode(GPIO_Pin_4, GPIO_Mode_Input_HIP); // Set ADC4(GPIO P1.4) HIP

  // *** Commented out because we do this in init_battmon() ****
  // ADC_SetClockPrescaler(0x01);    // ADC Clock = SYSCLK / 2 / (1+1) = SYSCLK
  // / 4 ADC_SetResultAlignmentLeft();   // Left alignment, high 8-bit in
  // ADC_RES ADC_SetPowerState(HAL_State_ON); // Turn on ADC power
}

// =============================================================
void set_rgb(uint8_t r, uint8_t g, uint8_t b); // forward declaration

void handle_VU_meter(void) {
  // READ ADC4 value with peak detection --------
  // Sample multiple times to catch peaks in audio waveform
  // For 1kHz audio (1ms period), taking 20 samples ensures we catch the peak

  uint8_t peak_value = 0;
  ADC_SetChannel(ADCCHANNEL_OUTMON); // Channel: ADC4 (P1.4 - RVC)

  // Take 20 samples and keep the maximum
  for (uint8_t i = 0; i < 20; i++) {
    ADC_Start();
    nop();
    nop();

    while (!ADC_SamplingFinished())
      ; // WAIT...

    ADC_ClearInterrupt();
    uint8_t OUTMONres = ADC_RES;
    int8_t out_res = OUTMONres - 0x80; // audio is centered around 0x80
    uint8_t abs_val = (out_res < 0) ? -out_res : out_res;

    // Keep track of peak
    if (abs_val > peak_value) {
      peak_value = abs_val;
    }
  }

  abs_out_res = peak_value;

#ifdef DEBUG
  // PRINT ADC for VU METER -----
  // UART1_TxHex(abs_out_res); // print out the OUTPUT MONITOR/VU METER value (peak
  //                           // is about 0x7F)
  // UART1_TxString(",");
#endif
}

// +---------------------------------------------------------------+
// | REMOTE VOLUME CONTROL FUNCTIONS                               |
// +---------------------------------------------------------------+
// Configure ADC7 (P1.7 - RVC) for battery voltage monitoring
void setAttenuation(uint8_t attenInDB); // forward declaration

void init_RVC() {
  GPIO_P1_SetMode(GPIO_Pin_7, GPIO_Mode_Input_HIP); // Set ADC7(GPIO P1.7) HIP

  // *** Commented out because we do this in init_battmon() ****
  // ADC_SetClockPrescaler(0x01);    // ADC Clock = SYSCLK / 2 / (1+1) = SYSCLK
  // / 4 ADC_SetResultAlignmentLeft();   // Left alignment, high 8-bit in
  // ADC_RES ADC_SetPowerState(HAL_State_ON); // Turn on ADC power

  GPIO_P3_SetMode(GPIO_Pin_2, GPIO_Mode_Output_PP); // VOL_CLK
  GPIO_P3_SetMode(GPIO_Pin_3, GPIO_Mode_Output_PP); // VOL_DATA
  GPIO_P3_SetMode(GPIO_Pin_4, GPIO_Mode_Output_PP); // VOL_LOAD

  PIN_ATTEN_LOAD = 1; // LOAD
  PIN_ATTEN_CLK = 1;  // CLK
  PIN_ATTEN_DATA = 1; // DATA

  setAttenuation(15); // low output until AFTER the firmware version flashes,
                      // then RVC-specified output
}

// ============================================================
// Fast lookup table method to convert RVC (0-133) to Attenuation (0 to 63)
//
// This curve is based on following measurements of the traditional MA-200 attenuation 
//    vs remote volume control position.  It does not have a MUTE feature,
//    like our default attenuation curve.
//
// RVC,attenuation_dB
// 1,0
// 0.875,-1.5010471526555516
// 0.75,-4.4084218269296525
// 0.625,-6.363435814235585
// 0.5,-8.636989461747557
// 0.375,-10.011288052965444
// 0.25,-10.936623787957386
// 0.125,-11.527373590991886
// 0,-11.840504365156153

// Lookup table generated from CSV data with minimized quantization error
static const int8_t atten_lookup[134] = {
    -12, -12, -12, -12, -12, -12, -12, -12, -12, -12,  // RVC 0-9
    -12, -12, -12, -12, -12, -12, -12, -12, -11, -11,  // RVC 10-19
    -11, -11, -11, -11, -11, -11, -11, -11, -11, -11,  // RVC 20-29
    -11, -11, -11, -11, -11, -11, -11, -11, -11, -11,  // RVC 30-39
    -11, -11, -10, -10, -10, -10, -10, -10, -10, -10,  // RVC 40-49
    -10, -10, -10, -10, -10, -10, -10,  -9,  -9,  -9,  // RVC 50-59
     -9,  -9,  -9,  -9,  -9,  -9,  -9,  -9,  -8,  -8,  // RVC 60-69
     -8,  -8,  -8,  -8,  -8,  -8,  -7,  -7,  -7,  -7,  // RVC 70-79
     -7,  -7,  -7,  -6,  -6,  -6,  -6,  -6,  -6,  -6,  // RVC 80-89
     -6,  -5,  -5,  -5,  -5,  -5,  -5,  -5,  -5,  -5,  // RVC 90-99
     -4,  -4,  -4,  -4,  -4,  -3,  -3,  -3,  -3,  -3,  // RVC 100-109
     -3,  -2,  -2,  -2,  -2,  -2,  -2,  -1,  -1,  -1,  // RVC 110-119
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,   0,   0,  // RVC 120-129
      0,   0,   0,   0                                 // RVC 130-133
};

uint8_t rvc_to_hilton_attenuation(uint8_t rvc)
{
    if (rvc > 133) {
      rvc = 133;
    }
    // NOTE: if needed, we can provide more guard band at the top by adding an offset here
    return (uint8_t)(-atten_lookup[rvc]);  // 134-byte lookup table (returns POSITIVE attenuation 0 - 63dB)
}

// =============================================================
void handle_RVC(bool force) {
#define ATTEN_STEP_DELAY_US 50

  // READ ADC7 value --------
  ADC_SetChannel(ADCCHANNEL_RVC); // Channel: ADC7 (P1.0 - RVC)
  ADC_Start();
  nop();
  nop();

  while (!ADC_SamplingFinished())
    ; // WAIT...

  ADC_ClearInterrupt();
  uint8_t RVCval = ADC_RES;

#ifdef DEBUG
  // // PRINT ADC for RVC -----
  // UART1_TxHex(RVCval); // print out the remote volume control value
  // UART1_TxString(",");
#endif

  if (RVCval > 0xE0) {
    // no Remote Volume Control at all, so result attenuation (res) is 0dB
    res = 0;
  } else if (rvc_mode == RVC_TRADITIONAL_MA220_MODE) {
    // we have a Remote Volume Control plugged in, use traditional MA-220 curve
    // Reverse RVCval so that 0 = loudest (0dB), matching the Default curve direction
    uint8_t rvc_reversed = (RVCval >= 133) ? 0 : (133 - RVCval);
    res = rvc_to_hilton_attenuation(rvc_reversed); // range(rvc_reversed)=0-133 -> range(res) = 0-12
  } else if (rvc_mode == RVC_DEFAULT_MODE_WITH_MUTE) {
    // we have a Remote Volume Control plugged in

    // RVCval range = {0, 133 or so (depends on tolerance of pot)}
    uint16_t top = (RVCval << 8) - RVCval; // (RVCval * 255)
    uint16_t bot = (255 - RVCval);         // (255 - RVCval)
    uint16_t linearPotVal =
        top / bot; // range: {0, 256}
                   // NOTE: this divide should be fast, because STC8G1K08A has
                   // MCU16 HW divider REVISIT: STC8G1K08 does NOT have hardware
                   // divider, so this is a software divide

    if (linearPotVal > 255) {
      linearPotVal = 255; // cap this, just in case
    }

    /* ******************** NEW ALGORITHM ******************** */
    // ***** J1 IN = LEFT-HANDED or "REVERSED" VOL CTRL DIRECTION
    uint8_t A = 3 * 64; // turn pot 3/4 of the way
    uint8_t B = 14;     //  to attenuate by 12dB @ knee of the curve
    uint8_t C = 48;     // then drop to mute
    uint8_t D = 236;    //  at the end of the pot travel (guard band)
    uint8_t E = 6;      //  at the beginning of the pot travel (guard band)

    // FUTURE: deal with left-handed vs right-handed volume control
    // if (!right_handed_volume_control) {
    //   // ***** LEFT-HANDED or "REVERSED" VOL CTRL DIRECTION
    //   // A = 3*64;  // these are the same for both cases, so commented them out
    //   // B = 14;
    //   // C = 48;
    //   // D = 236;
    //   E = 11;
    //   linearPotVal = 255 - linearPotVal; // 255 - 0
    // }

    // the newest algorithm -----------------
    if (linearPotVal < E) {
      res = 0; // result = no attenuation
    } else if (linearPotVal > D) {
      res = 64; // attenuation = MAX
    } else if (linearPotVal <= A) {
      // intermediate result = uint16, attenuation is positive
      res = ((linearPotVal - E) * B) / A;
    } else { // x > A
      // intermediate result = uint16, attenuation is positive
      res = B + ((uint16_t)(linearPotVal - A) * (uint8_t)(C - B)) /
                    (uint8_t)(D - A);
    }
    /* ******************** END NEW ALGORITHM ******************** */

  } else {
    // unknown RVC mode, default to no attenuation
    res = 0; // this should never occur, but if it does, be safe
  } // end else

#ifdef DEBUG
  // PRINT ATTENUATION for RVC (negative sign omitted) -----
  // UART1_TxHex(res); // print out the resulting attenuation
  // UART1_TxString("\r\n");
#endif

  if (force) {
    setAttenuation(res); // one time force
  } else {
    // step it up or down in increments of 1 dB to avoid zipper effect
    if (previousRes < res) {
      // step UP to get to correct level of attenuation
      for (uint8_t a = previousRes + 1; a < res; a++) {
        setAttenuation(a);
#if ATTEN_STEP_DELAY_US > 0
        SYS_DelayUs(ATTEN_STEP_DELAY_US);
#endif
      }
      setAttenuation(res);
    } else if (res < previousRes) {
      // step DOWN to get to correct level of attenuation
      for (uint8_t a = previousRes - 1; a > res; a--) {
        setAttenuation(a);
#if ATTEN_STEP_DELAY_US > 0
        SYS_DelayUs(ATTEN_STEP_DELAY_US);
#endif
      }
      setAttenuation(res);
    } else {
      // else do nothing if old and new attenuation values are equal
      // setAttenuation(res);  // DEBUG DEBUG DEBUG
    }
  }
  previousRes = res; // remember where we are now (regardless of force or not)
}

// =============================================================
// 0 = 0dB attenuation, >=63 = MUTE
void setAttenuation(uint8_t attenInDB) {
  // Expanding all loops so that the entire cycle takes the minimal time
  //   to set the attenuation.  This completely eliminates popping!
  // This also allows us to return from servicing ASAP, to reduce power
  // consumption.
  PIN_ATTEN_CLK = 0;
  PIN_ATTEN_DATA = 0;
  PIN_ATTEN_LOAD =
      0; // SPEC: CLOCK must go down before LOAD goes down (2 cycles = 84ns)
  nop();
  nop();
  nop();
  nop();
  nop();
  nop();

  // ADDRESS = 0x00 ----------------------
  // SPEC: LOAD LOW to CLOCK HIGH >200ns ( 6 * 42 = 252ns)
  // SPEC: DATA VALID to CLOCK HIGH > 100ns ( 7 * 42 = 294ns)
  PIN_ATTEN_CLK = 1; // A7
  nop(); // these might not be strictly necessary, but it makes them more
         // visible on Saleae
  PIN_ATTEN_CLK = 0;
  nop();
  PIN_ATTEN_CLK = 1; // A6
  nop();
  PIN_ATTEN_CLK = 0;
  nop();
  PIN_ATTEN_CLK = 1; // A5
  nop();
  PIN_ATTEN_CLK = 0;
  nop();
  PIN_ATTEN_CLK = 1; // A4
  nop();
  PIN_ATTEN_CLK = 0;
  nop();
  PIN_ATTEN_CLK = 1; // A3
  nop();
  PIN_ATTEN_CLK = 0;
  nop();
  PIN_ATTEN_CLK = 1; // A2
  nop();
  PIN_ATTEN_CLK = 0;
  nop();
  PIN_ATTEN_CLK = 1; // A1
  nop();
  PIN_ATTEN_CLK = 0;
  nop();
  PIN_ATTEN_CLK = 1; // A0
  nop();             // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

  // DATA -----------------------------
  PIN_ATTEN_CLK = 0; // CLK low
  PIN_ATTEN_DATA = (attenInDB & 0x80) >> 7;
  nop();
  nop();
  nop();             // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
  PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
  nop();             // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

  PIN_ATTEN_CLK = 0; // CLK low
  PIN_ATTEN_DATA = (attenInDB & 0x40) >> 6;
  nop();
  nop();
  nop();             // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
  PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
  nop();             // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

  PIN_ATTEN_CLK = 0; // CLK low
  PIN_ATTEN_DATA = (attenInDB & 0x20) >> 5;
  nop();
  nop();
  nop();             // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
  PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
  nop();             // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

  PIN_ATTEN_CLK = 0; // CLK low
  PIN_ATTEN_DATA = (attenInDB & 0x10) >> 4;
  nop();
  nop();
  nop();             // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
  PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
  nop();             // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

  PIN_ATTEN_CLK = 0; // CLK low
  PIN_ATTEN_DATA = (attenInDB & 0x08) >> 3;
  nop();
  nop();
  nop();             // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
  PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
  nop();             // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

  PIN_ATTEN_CLK = 0; // CLK low
  PIN_ATTEN_DATA = (attenInDB & 0x04) >> 2;
  nop();
  nop();
  nop();             // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
  PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
  nop();             // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

  PIN_ATTEN_CLK = 0; // CLK low
  PIN_ATTEN_DATA = (attenInDB & 0x02) >> 1;
  nop();
  nop();
  nop();             // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
  PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
  nop();             // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

  PIN_ATTEN_CLK = 0; // CLK low
  PIN_ATTEN_DATA = (attenInDB & 0x01);
  nop();
  nop();
  nop();             // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
  PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
  nop();             // SPEC: DATA HOLD TIME > 50ns (3 cycles = 120ns)
  nop();

  // finish up ------
  PIN_ATTEN_LOAD = 1; // SPEC: CLOCK TO LOAD HIGH > 50ns (2 cycles = 84ns)
  PIN_ATTEN_DATA = 1; // DATA high
}

// +---------------------------------------------------------------+
// | BATTERY MONITOR FUNCTIONS                                     |
// +---------------------------------------------------------------+
// Configure ADC0 (P1.0 - BATTMON) for battery voltage monitoring
void init_battmon() {
  GPIO_P1_SetMode(GPIO_Pin_0, GPIO_Mode_Input_HIP); // Set ADC0(GPIO P1.0) HIP
  ADC_SetClockPrescaler(0x01);  // ADC Clock = SYSCLK / 2 / (1+1) = SYSCLK / 4
  ADC_SetResultAlignmentLeft(); // Left alignment, high 8-bit in ADC_RES
  ADC_SetPowerState(HAL_State_ON); // Turn on ADC power
}

void handle_battmon(void) {
  // READ ADC0 value --------
  ADC_SetChannel(ADCCHANNEL_BATTMON); // Channel: ADC0 (P1.0 - BATTMON)
  ADC_Start();
  nop();
  nop();

  while (!ADC_SamplingFinished())
    ; // WAIT...

  ADC_ClearInterrupt();
  battmon_res = ADC_RES; // latest battery monitor result

  // battery voltage = 0 - 12vdc, divider = 20Kohm/67Kohm = 0.3
  //   so, battery voltage 12v maps to (12 * 0.3)/5 * 255 => 184
  //   so, battery voltage  9v maps to ( 9 * 0.3)/5 * 255 => 138

  //   so, battery voltage  7v maps to ( 7 * 0.3)/5 * 255 => 107
  //   so, battery voltage  6v maps to ( 6 * 0.3)/5 * 255 =>  91

  //   so, battery voltage  4.6v maps to ( 4.6 * 0.3)/5 * 255 => 70 ( I get 70!)
  //   <-- USB Stick case

  // Here's our design choice:
  // 107 ==> 7.0V = HIGH WATER MARK = LED ON (below this, light starts flashing)
  //  91 ==> 6.0V = LOW WATER MARK = LED OFF 90%/ON 10%

  // right now with +5V in, I get about 70 on USB stick power, expected 76
  // [PASS]

#ifdef DEBUG
  // PRINT ADC for BATTMON -----
  // UART1_TxHex(result); // print out the ADC BATTMON result
  // UART1_TxString(",");
#endif
}

// +---------------------------------------------------------------+
// | LED CONTROL FUNCTIONS                                         |
// +---------------------------------------------------------------+
void init_leds(void) {
  // Set GPIO pin mode for BLUE LED on P3.5 (CCP0)
  GPIO_P3_SetMode(GPIO_Pin_5, GPIO_Mode_Output_PP);

  // Set GPIO pin mode for GREEN LED on P3.6 (CCP1)
  GPIO_P3_SetMode(GPIO_Pin_6, GPIO_Mode_Output_PP);

  // Set GPIO pin mode for RED LED on P3.7 (CCP2)
  GPIO_P3_SetMode(GPIO_Pin_7, GPIO_Mode_Output_PP);

  // Stop PCA/PWM
  PCA_SetCounterState(HAL_State_OFF);

  // Keep counter running in idle mode
  PCA_SetStopCounterInIdle(HAL_State_OFF);

  // Use SYSCLK as clock source
  PCA_SetClockSource(PCA_ClockSource_SysClk);

  // Turn off overflow interrupt
  PCA_EnableCounterOverflowInterrupt(HAL_State_OFF);

  // Set PCA0 (CCP0/P3.5) to PWM mode
  PCA_PCA0_SetWorkMode(PCA_WorkMode_PWM_NonInterrupt);

  // Set PCA1 (CCP1/P3.6) to PWM mode
  PCA_PCA1_SetWorkMode(PCA_WorkMode_PWM_NonInterrupt);

  // Set PCA2 (CCP2/P3.7) to PWM mode
  PCA_PCA2_SetWorkMode(PCA_WorkMode_PWM_NonInterrupt);

  // Set to 8-bit PWM
  PCA_PWM0_SetBitWidth(PCA_PWM_BitWidth_8);
  PCA_PWM1_SetBitWidth(PCA_PWM_BitWidth_8);
  PCA_PWM2_SetBitWidth(PCA_PWM_BitWidth_8);

  // Set initial duty cycle
  PCA_PCA0_SetCompareValue(0);
  PCA_PCA1_SetCompareValue(0);
  PCA_PCA2_SetCompareValue(0); // Start off (active low)

  // Route PCA outputs to P3.4/P3.5/P3.6/P3.7
  PCA_SetPort(PCA_AlterPort_P34_P35_P36_P37);

  // Start PWM
  PCA_SetCounterState(HAL_State_ON);
}

// Set RGB LED colors
// r, g, b: 0-255 (0 = off, 255 = full brightness for active-low LEDs)
void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  // LEDs are Active Low (0 = On, 255 = Off)
  // PCA Logic: Output Low when Counter < Compare.
  // Active Low LED: Low = ON.
  // So Higher Compare Value = Longer Low Time = Brighter.

  // BLUE LED on P3.5 (CCP0)
  PCA_PCA0_ChangeCompareValue(b);

  // GREEN LED on P3.6 (CCP1)
  PCA_PCA1_ChangeCompareValue(g);

  // RED LED on P3.7 (CCP2)
  PCA_PCA2_ChangeCompareValue(r);
}

// =============================================================
// Display firmware version on RGB LEDs at power-up
// RED flashes = major version
// GREEN flashes = minor version
// BLUE flashes = patch version
void display_version_on_leds(void) {
  uint8_t i;
  uint16_t flash_on_time = (uint16_t)(400 * VERSION_DISPLAY_TIMING_SCALE);
  uint16_t flash_off_time = (uint16_t)(400 * VERSION_DISPLAY_TIMING_SCALE);
  uint16_t between_colors_time = (uint16_t)(200 * VERSION_DISPLAY_TIMING_SCALE);
  uint16_t pause_time = (uint16_t)(1600 * VERSION_DISPLAY_TIMING_SCALE);

  // Flash RED for major version
  for (i = 0; i < FW_MAJOR; i++) {
    set_rgb(LED_RED_CALIBRATION, 0,
            0); // RED on (dimmed because green LED is very efficient)
    SYS_Delay(flash_on_time);
    set_rgb(0, 0, 0); // Off
    SYS_Delay(flash_off_time);
  }

  SYS_Delay(between_colors_time); // Short pause between colors

  // Flash GREEN for minor version
  for (i = 0; i < FW_MINOR; i++) {
    set_rgb(0, LED_GREEN_CALIBRATION,
            0); // GREEN on (dimmed because green LED is very efficient)
    SYS_Delay(flash_on_time);
    set_rgb(0, 0, 0); // Off
    SYS_Delay(flash_off_time);
  }

  SYS_Delay(between_colors_time); // Short pause between colors

  // Flash BLUE for minor version
  for (i = 0; i < FW_PATCH; i++) {
    set_rgb(0, 0,
            LED_BLUE_CALIBRATION); // BLUE on
    SYS_Delay(flash_on_time);
    set_rgb(0, 0, 0); // Off
    SYS_Delay(flash_off_time);
  }

  SYS_Delay(pause_time); // Pause = one flash time (on + off)
}

void show_rvc_mode_on_led(rvc_mode_t rvc_mode) {
  // Set up LED override state machine for RVC mode indication
  led_override_active = 1;
  led_override_state = 0;  // Start with initial pause
  led_override_tick_counter = 0;

  // Flash count: 1 flash for mode 0 (Default), 2 flashes for mode 1 (Traditional)
  led_override_flash_count = (rvc_mode == RVC_DEFAULT_MODE_WITH_MUTE) ? 1 : 2;
  led_override_flash_current = 0;

  // Turn off LED immediately to start the initial pause
  set_rgb(0, 0, 0);

#ifdef DEBUG
  UART1_TxString("RVC Mode: ");
  switch (rvc_mode) {
    case RVC_TRADITIONAL_MA220_MODE:
      UART1_TxString("Traditional MA-220 Mode (2 flashes)\r\n");
      break;
    case RVC_DEFAULT_MODE_WITH_MUTE:
      UART1_TxString("Default Mode with Mute (1 flash)\r\n");
      break;
    default:
      UART1_TxString("Unknown\r\n");
      break;
  }
#endif
}

void handle_led_override(void) {
  led_override_tick_counter++;

  switch (led_override_state) {
    case 0:  // Initial pause (500ms = 10 ticks @ 20Hz)
      set_rgb(0, 0, 0);  // LED off
      if (led_override_tick_counter >= 10) {
        led_override_state = 1;  // Move to flash-on state
        led_override_tick_counter = 0;
        led_override_flash_current = 0;
      }
      break;

    case 1:  // Flash ON (200ms = 4 ticks @ 20Hz)
      set_rgb(LED_OVERRIDE_FLASH_RED, LED_OVERRIDE_FLASH_GREEN, LED_OVERRIDE_FLASH_BLUE);  // Flash color
      if (led_override_tick_counter >= 4) {
        led_override_state = 2;  // Move to flash-off state
        led_override_tick_counter = 0;
      }
      break;

    case 2:  // Flash OFF (200ms = 4 ticks @ 20Hz)
      set_rgb(0, 0, 0);  // LED off
      if (led_override_tick_counter >= 4) {
        led_override_flash_current++;
        if (led_override_flash_current < led_override_flash_count) {
          // More flashes needed, return to flash-on
          led_override_state = 1;
          led_override_tick_counter = 0;
        } else {
          // All flashes complete, move to final pause
          led_override_state = 3;
          led_override_tick_counter = 0;
        }
      }
      break;

    case 3:  // Final pause (500ms = 10 ticks @ 20Hz)
      set_rgb(0, 0, 0);  // LED off
      if (led_override_tick_counter >= 10) {
        // Pattern complete, deactivate override
        led_override_active = 0;
        led_override_state = 0;
        led_override_tick_counter = 0;
        // LED will automatically resume normal led_mode on next handle_leds() call
      }
      break;
  }
}

void pulsing_red(void) {
  // Pulsing RED
  if (pulseDirectionUP) {
    if (red < LED_RED_CALIBRATION) {
      red += 6;
    } else {
      pulseDirectionUP = false;
    }
  } else {
    if (red > 0) {
      red -= 6;
    } else {
      pulseDirectionUP = true;
    }
  }
  set_rgb(red, 0, 0); // Pulsing RED
}

// 20Hz LED update handler - sets PWM target values for hardware PCA
// NOTE: LEDs are driven by PCA hardware PWM. This function just sets target values.
// Running at 20Hz (matching VU meter rate) saves power vs 100Hz with no UX degradation.
void handle_leds(void) {
  // Check if LED override is active (for temporary patterns like RVC mode indication)
  if (led_override_active) {
    handle_led_override();
    return;  // Skip normal LED mode processing
  }

  // LOW BATTERY OVERRIDE:
  // If battery voltage is below the low watermark, override all other modes
  // NOTE: to DEBUG this, temporarily add "false &&" to the below IF statement
  // so that we can test LED modes with power supplied by the ISP dongle.
  // if battmon_res is 255, then we haven't read the battery ADC yet.
  // NOTE: In ISP mode, we will always go to green then pulsing_red() after the 
  //   R-G-B version power-up sequence, before we go to the selected led_mode.  This will not happen
  //   in normal battery-powered operation, when it will go to GREEN if battery is good.
  if ((battmon_res != 255) && (battmon_res < RED_WATERMARK)) {
    pulsing_red();
    return;  // Exit early to avoid further processing
  }

  // Normal LED mode processing (battery is NOT below low watermark)
  switch (led_mode) {
  case BATTERY_MONITOR_MODE:
    // Battery monitor mode
    // LED brightness proportional to battery voltage
    // (handled in handle_battmon() if needed)
    if (battmon_res >= GREEN_WATERMARK) {
      set_rgb(0, LED_GREEN_CALIBRATION, 0); // Solid GREEN
    } else if (battmon_res >= YELLOW_WATERMARK) {
      set_rgb(LED_RED_CALIBRATION, LED_GREEN_CALIBRATION, 0); // Solid YELLOW
    } else if (battmon_res >= RED_WATERMARK) {
      set_rgb(LED_RED_CALIBRATION, 0, 0); // Solid RED
    } else {
      pulsing_red(); // Pulsing RED -- technically I'll never reach here due to LOW BATTERY OVERRIDE above
                     // unless we're debugging
    }
    // NOTE: below RED_WATERMARK is handled by LOW BATTERY OVERRIDE above
    break;

  case SOLID_RED_MODE:
    // Solid RED mode
    set_rgb(LED_RED_CALIBRATION, 0, 0);
    break;

  case SOLID_GREEN_MODE:
    // Solid GREEN mode
    set_rgb(0, LED_GREEN_CALIBRATION, 0);
    break;

  case SOLID_BLUE_MODE:
    // Solid BLUE mode
    set_rgb(0, 0, LED_BLUE_CALIBRATION);
    break;

  case SOLID_WHITE_MODE:
    // Solid WHITE mode
    // set_rgb(LED_RED_CALIBRATION, LED_GREEN_CALIBRATION, LED_BLUE_CALIBRATION); // TODO: reduce these to lower power?
    set_rgb(50, 20, 30);  // Dimmed white to reduce power consumption
    break;

  case VU_METER_MODE:
    // VU meter mode
    // Fast Attack: Jump immediately to new higher peaks
    // Slow Decay: Exponential decay (multiply by 0.9 each tick)

    { // Scope for local variables

      window[window_index] = abs_out_res; // Store current signal value
      window_index = (window_index + 1) % 4; // Circular buffer for 4 samples

      // calculate MAX of the last 4 samples
      uint16_t max_of_last_4 = 0;
      for (uint8_t i = 0; i < 4; i++) {
        if (window[i] > max_of_last_4) {
          max_of_last_4 = window[i];
        }
      }

      // uint16_t current_signal_fixed = (uint16_t)abs_out_res << 8;
      uint16_t current_signal_fixed = (uint16_t)max_of_last_4 << 8;
#ifdef DEBUG
      UART1_TxHex(current_signal_fixed >> 8);
      UART1_TxHex(current_signal_fixed & 0xFF);
      UART1_TxChar(',');
      UART1_TxHex(vu_display_val_fixed >> 8);
      UART1_TxHex(vu_display_val_fixed & 0xFF);
      UART1_TxChar(',');
#endif
      if (current_signal_fixed >= vu_display_val_fixed) {
        vu_display_val_fixed = current_signal_fixed; // Fast attack
#ifdef DEBUG
        UART1_TxChar('+');
#endif        
      } else {
        // Exponential decay: multiply by 0.9375
        // Use 32-bit math to avoid overflow: (value * 15) / 16
        uint32_t temp = (uint32_t)vu_display_val_fixed * 15;
        vu_display_val_fixed = (uint16_t)(temp / 16);
#ifdef DEBUG
        UART1_TxChar('-');
#endif

        // Snap to zero if below threshold (1.0 in 8.8 fixed point)
        if (vu_display_val_fixed < 256 * 1) {
#ifdef DEBUG
        UART1_TxChar('X');
#endif
          vu_display_val_fixed = 0;
        }
      }
#ifdef DEBUG
      UART1_TxHex(vu_display_val_fixed >> 8);
      UART1_TxHex(vu_display_val_fixed & 0xFF);
      UART1_TxChar(',');
#endif
      uint8_t display_val = (vu_display_val_fixed + 0x80) >> 8; // rounded, not truncated

      // Ensure display_val doesn't exceed 127
      if (display_val > 127)
        display_val = 127;

      // Lookup logarithmic brightness/loudness
      uint8_t b = volume_log_table[display_val];

      // Calculate smooth RGB values
      uint8_t r, g, blue;
      calculate_vu_color(b, &r, &g, &blue);

#ifdef DEBUG
      UART1_TxHex(b);
      UART1_TxChar(',');
      UART1_TxHex(r);
      UART1_TxChar(',');
      UART1_TxHex(g);
      UART1_TxString("\r\n");
#endif

      // Set LED with smooth color transitions
      set_rgb(r, g, blue);
    }
    break;

  case EDIT_SW1_MODE:
    // Edit mode for Switch 1
    break;

  case EDIT_SW2_MODE:
    // Edit mode for Switch 2
    break;

  default:
    // Unknown mode, turn off LEDs
    set_rgb(0, 0, 0);
    break;
  }
}

// ----------------------------------------------------------------
void welcome_to_ladybug(void) {
  display_version_on_leds();
}

// +---------------------------------------------------------------+
// | SWITCH CONTROL FUNCTIONS                                      |
// +---------------------------------------------------------------+

// Configure switch pins with pullups
void init_switches(void) {
  // Switch 1 on P1.5 and Switch 2 on P1.6
  // Switches short to ground when pressed, so we need pullups
  GPIO_P1_SetMode(SWITCH_1_PIN, GPIO_Mode_Input_HIP); // High-impedance input
  GPIO_P1_SetMode(SWITCH_2_PIN, GPIO_Mode_Input_HIP); // High-impedance input

  // Explicitly enable internal pullups
  GPIO_SetPullUp(GPIO_Port_1, SWITCH_1_PIN, HAL_State_ON);
  GPIO_SetPullUp(GPIO_Port_1, SWITCH_2_PIN, HAL_State_ON);
}

// =============================================================
// Switch down event handler
// Called from timer interrupt when a debounced switch press is detected
void on_switch_down(uint8_t switch_number) {
  switch_number = 3 - switch_number; // Swap switch numbers for user perspective
#ifdef DEBUG
  if (switch_number == 1) {
    UART1_TxString("LEFT SWITCH 1 (SW2) DOWN\r\n");
  } else if (switch_number == 2) {
    UART1_TxString("RIGHT SWITCH 2 (SW1) DOWN\r\n");
  }
#else
  UNUSED(switch_number); // currently no action on switch down
#endif
}

// =============================================================
// Switch UP event handler
// Called from timer interrupt when a debounced switch press is detected
// NOTE: In the schematic, SW1 is the switch closest to the front.
//       But, from the user's perspective, SW1 is the switch on the LEFT side.
void on_switch_up(uint8_t switch_number) {
  switch_number = 3 - switch_number; // Swap switch numbers for user perspective
#ifdef DEBUG
  if (switch_number == 1) {
    UART1_TxString("LEFT SWITCH 1 (SW2) UP\r\n");
  } else if (switch_number == 2) {
    UART1_TxString("RIGHT SWITCH 2 (SW1) UP\r\n");
  }
#endif

  switch (switch_number) {
  case 1:
    // SW1: Cycle LED modes
    led_mode =
        (led_mode + 1) % (LAST_LED_MODE + 1); // Cycle from 0 thru LAST_LED_MODE

    // If switching to VU_METER_MODE, start at max (red) and let it fade naturally
    if (led_mode == VU_METER_MODE) {
      vu_display_val_fixed = (uint16_t)VU_METER_FULL_SCALE << 8; // Start at max (red)
    }

    // the LED changes immediately in handle_leds(), so no need to special flash LEDs here

    prefs.vu_meter_mode_pref = led_mode & 0x3; // 2-bit LED mode preference
    Pref_Write(&prefs); // write updated LED preference to EEPROM
    break;
  case 2:
    // SW2: toggle normal RVC curve vs traditional MA-200 curve
    rvc_mode = (rvc_mode + 1) % 2; // toggle between 0 and 1

    show_rvc_mode_on_led(rvc_mode); // flash LED to indicate new RVC mode

    prefs.rvc_curve_pref = rvc_mode & 0x1; // 1-bit RVC curve preference
    Pref_Write(&prefs); // write updated LED preference to EEPROM
    break;
  default:
    // Unknown switch number
    break;
  }
}


// =============================================================
// 100Hz interrupt handler -- SWITCH handling
void handle_switches(void) {
  // Switch debouncing - read pins and debounce
  uint8_t switch1_raw = P15; // Read P1.5 (Switch 1)
  uint8_t switch2_raw = P16; // Read P1.6 (Switch 2)

  // Debounce Switch 1
  if (switch1_raw != switch1_state) {
    // Pin state differs from debounced state
    switch1_debounce_counter++;
    if (switch1_debounce_counter >= SWITCH_DEBOUNCE_COUNT) {
      // State has been stable for enough samples
      uint8_t old_state = switch1_state;
      switch1_state = switch1_raw;
      switch1_debounce_counter = 0;

      // Detect falling edge (button press): 1 -> 0
      if (old_state == 1 && switch1_state == 0) {
        on_switch_down(1);
      }
      // Detect rising edge (button press): 0 -> 1
      if (old_state == 0 && switch1_state == 1) {
        on_switch_up(1);
      }
    }
  } else {
    // Pin state matches debounced state, reset counter
    switch1_debounce_counter = 0;
  }

  // Debounce Switch 2
  if (switch2_raw != switch2_state) {
    // Pin state differs from debounced state
    switch2_debounce_counter++;
    if (switch2_debounce_counter >= SWITCH_DEBOUNCE_COUNT) {
      // State has been stable for enough samples
      uint8_t old_state = switch2_state;
      switch2_state = switch2_raw;
      switch2_debounce_counter = 0;

      // Detect falling edge (button press): 1 -> 0
      if (old_state == 1 && switch2_state == 0) {
        on_switch_down(2);
      }
      // Detect rising edge (button press): 0 -> 1
      if (old_state == 0 && switch2_state == 1) {
        on_switch_up(2);
      }
    }
  } else {
    // Pin state matches debounced state, reset counter
    switch2_debounce_counter = 0;
  }
}

// +---------------------------------------------------------------+
// | UART INITIALIZATION AND DEBUG FUNCTIONS                       |
// +---------------------------------------------------------------+
void init_uart(void) {
#ifdef DEBUG
  // Initialize UART1 for 115200 baud, 8N1
  UART1_Config8bitUart(UART1_BaudSource_Timer1, HAL_State_ON, 115200);
  UART1_SwitchPort(UART1_AlterPort_P30_P31); // P3.0 RX, P3.1 TX

  // Send test message
  UART1_TxString("UART Initialized\r\n");
  UART1_TxString("Firmware Version: ");
  UART1_TxHex(FW_MAJOR);
  UART1_TxChar('.');
  UART1_TxHex(FW_MINOR);
  UART1_TxChar('.');
  UART1_TxHex(FW_PATCH);
  UART1_TxString("\r\n");
#endif
}

// +---------------------------------------------------------------+
// | 100Hz TIMER CONTROL FUNCTIONS                                 |
// +---------------------------------------------------------------+
// Timer0 interrupt service routine - runs 100 times per second
INTERRUPT(Timer0_Routine, EXTI_VectTimer0) {

#ifdef INCLUDE_TEST_POINT 
  // PIN_TP1 = (timer_ticks & 0x01); // TEST POINT toggles @ 100Hz
  PIN_TP1 = 1; // TEST POINT to HIGH (start of ISR)
#endif

  handle_switches(); // Run at 100Hz for proper switch debouncing

  // Pre-enable ADC one tick before sampling (gives 10ms settling time)
  // RVC/VU sampling happens every 5 ticks (0, 5, 10, 15...)
  // Battery sampling happens at tick 100
  if ((timer_ticks % RVC_UPDATE_FREQUENCY_TICKS == (RVC_UPDATE_FREQUENCY_TICKS - 1)) ||
      (timer_ticks == TIMER_FREQUENCY_HZ - 1)) {
    // Turn ON ADC power 10ms before we need it (no delay needed)
    ADC_SetPowerState(HAL_State_ON);
  }

  if (timer_ticks % RVC_UPDATE_FREQUENCY_TICKS == 0) {
    // Run at 20Hz (every 5 ticks = 50ms)
    handle_RVC(false);   // Update RVC attenuation
    if (led_mode == VU_METER_MODE) {
      // If in VU meter mode, update the VU meter display
      handle_VU_meter(); // Sample ADC for VU METER (with fast attack, slow decay)
    }
    handle_leds();       // Update LED for all modes (in VU METER mode, matches VU update rate, saves 80% CPU cycles)
  }

  if (timer_ticks++ >= TIMER_FREQUENCY_HZ) {
    timer_ticks = 0;
    handle_battmon(); // Run at 1Hz - battery monitor only once per second
  }

  // Turn OFF ADC power after sampling is complete
  if ((timer_ticks % RVC_UPDATE_FREQUENCY_TICKS == 1) || (timer_ticks == 1)) {
    ADC_SetPowerState(HAL_State_OFF);
  }

#ifdef INCLUDE_TEST_POINT  
  PIN_TP1 = 0; // TEST POINT to LOW (end of ISR)
#endif
}

// =============================================================
// | MAIN                                                      |
// =============================================================

static uint8_t ReadByte(uint16_t addr) {
  IAP_SetEnabled(HAL_State_ON);

  IAP_CmdRead(addr);
  if (IAP_IsCmdFailed()) {
    IAP_ClearCmdFailFlag();
#ifdef DEBUG
    // UART1_TxString("XX");
#endif    
  IAP_SetEnabled(HAL_State_OFF);
    return 0xFF; // Return 0xFF on failure (erased state)
  }
  uint8_t data = IAP_ReadData();
  IAP_SetEnabled(HAL_State_OFF);
  return data;
}

void main(void) {

  SYS_SetClock();

#ifdef INCLUDE_PREFERENCES
  IAP_SetWaitTime(); // necessary before any IAP commands

  // Pref_Read(&prefs); // read preferences from EEPROM

  // led_mode = prefs.vu_meter_mode_pref; // 2-bit LED mode preference

  // UART1_TxString(", led_mode: ");
  // UART1_TxHex(led_mode);
  // UART1_TxChar('.');
#endif

// Clock divider configuration moved to globals.h
// - Set CLK_DIVIDER_2 or CLK_DIVIDER_4 in globals.h for power saving
// - DEBUG mode automatically uses CLK_DIVIDER_1 (full speed)
// - SYS_SetClock() applies __CONF_CLKDIV from globals.h

#ifdef DEBUG
  init_uart();
#endif

#ifdef INCLUDE_PREFERENCES

  Pref_Init(); // initialize preferences system (required before any Pref_Read/Write)

#ifdef DEBUG
  Pref_Dump(); // dump current preferences to UART
#endif

  Pref_Read(&prefs); // read preferences from EEPROM
  led_mode = prefs.vu_meter_mode_pref; // 2-bit LED mode preference
  rvc_mode = prefs.rvc_curve_pref;     // 1-bit RVC curve preference

  // If power-on mode is VU_METER_MODE, start at max (red) and let it fade naturally
  if (led_mode == VU_METER_MODE) {
    vu_display_val_fixed = (uint16_t)VU_METER_FULL_SCALE << 8; // Start at max (red)
  }

#ifdef DEBUG
  UART1_TxString("  led_mode: ");
  UART1_TxHex(led_mode);
  UART1_TxString("  rvc_mode: ");
  UART1_TxHex(rvc_mode);
  UART1_TxChar('.');
#endif  
#endif

#ifdef INCLUDE_TEST_POINT
  init_test_point(); // initialize test point system (required before any TestPoint_Read/Write) TEST TEST TEST
#endif

  init_leds();
  init_switches();

  init_RVC();      // do RVC first
  init_VU_meter(); // then VU meter
  init_battmon();  // then battery monitor (turns on ADC)

  // Display firmware version on LEDs at power-up (BEFORE Timer0 starts)
  // but ONLY if one of the switches is held down at power-up
  if ((P15 == 0) || (P16 == 0)) {
    display_version_on_leds();

    // wait until both switches are released
    while ((P15 == 0) || (P16 == 0)) {
      SYS_Delay(10);
    }
  }

  // after running at -15dB for a second, now go to RVC volume
  handle_RVC(true); // service once to force init volume as per RVC

  welcome_to_ladybug(); // run fancy LED startup sequence

  // Configure Timer0 for 100Hz interrupt
  TIM_Timer0_Config(HAL_State_OFF, TIM_TimerMode_16BitAuto, TIMER_FREQUENCY_HZ); // 100Hz
  EXTI_Timer0_SetIntState(HAL_State_ON);
  EXTI_Global_SetIntState(HAL_State_ON);
  TIM_Timer0_SetRunState(HAL_State_ON);

  while (1) {
    // Enter IDLE mode to save power
    // CPU stops but peripherals (Timer0, PCA/PWM) continue running
    // Timer0 interrupt will wake the CPU
    PCON |= 0x01; // Set IDL bit to enter IDLE mode
  } // while (1)
}
