// +-----------------------------------------------+
// | BLINKTEST: Firmware for Mixer Board V2.4      |
// |                                               |
// | Copyright (c) 2024, Michael Pogue             |
// | License: GPL V3                               |
// +-----------------------------------------------+

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "fw_hal.h"

// GLOBALS =============================
int LEDCounter = 0;

uint8_t firmwareMajorVersion = 1; // NOTE: Increments infrequently
uint8_t firmwareMinorVersion = 1; // NOTE: Increments every new version

uint8_t jumperADC = 0;
bool    jumperJ1_IN = false; // if IN, then REVERSE DIRECTION on Remote Volume Ctrl
bool    jumperJ2_IN = false; // unassigned right now

uint8_t res = 0;          // attenuation result
uint8_t previousRes = 0;  // previous attenuation result

// SLEEPY: minimize power, but doesn't allow automatic Upload from programmer @ CTL-OPT-U time
// NOTE: Sleepy currently messes up the ADC, so turning it OFF for now
//#define SLEEPY 1

// DEBUG_OUTPUT: enable UART, uses power and cpu cycles, but easier to debug
//  On Mac, use SerialTools set to usbserial-110 (or similar), 115200/8/N/1.
//  NOTE: we have to click Disconnect in SerialTools before we can program the MCU via ISP.
// #define DEBUG_OUTPUT

// ==========================================================
// UTILS ================================
#define nop() __asm__(" nop");

// ==========================================================
// JUMPER READ AT POWER ON TIME =============================
#define ADCCHANNEL_JUMPER 0x01

void initJumperRead() {
    // one-time read of the jumper ADC value, which tells us which
    //   of J1/J2 are in/out.  After read at powerup time, 
    //   we don't need to access ADC1 anymore.

    // NOTE: IF THE STC PROGRAMMER IS CONNECTED, THE ADC READ WILL
    //  ALWAYS BE HIGH, AND BOTH JUMPERS WILL APPEAR TO BE IN.

    // init ADC for JUMPER READ, P3.1/ADC1 -----
    GPIO_P3_SetMode(GPIO_Pin_1, GPIO_Mode_Input_HIP);  // Set ADC1(GPIO P3.1) HIP
    ADC_SetClockPrescaler(0x01);    // ADC Clock = SYSCLK / 2 / (1+1) = SYSCLK / 4
    ADC_SetResultAlignmentLeft();   // Left alignment, high 8-bit in ADC_RES
    ADC_SetPowerState(HAL_State_ON); // Turn on ADC power

    // READ ADC1 value --------
    ADC_SetChannel(ADCCHANNEL_JUMPER);  // Channel: ADC1
    ADC_Start();
    nop();
    nop();

    while (!ADC_SamplingFinished());  // WAIT...

    ADC_ClearInterrupt();
    jumperADC = ADC_RES;

    // J1     J2     out(V)   out (val)
    // out    out    0.0        0
    // out     in    1.7       87
    //  in    out    2.5      127
    //  in     in    3.0      153
    //
    // therefore threshold values to split 
    //   into 4 buckets are:  43, 107, 140
    
    jumperJ1_IN = false;
    jumperJ2_IN = false;
    if (jumperADC > 107) {
        jumperJ1_IN = true;
        if (jumperADC > 140) {
            jumperJ2_IN = true;
        }
    } else {
        // we know that J1 is OUT
        if (jumperADC > 43) {
            jumperJ2_IN = true;
        }
    }
}

// ==========================================================
// BATTERY MONITOR AND LED ==================================
#define PIN_LED P55
#define PIN_BATTMON P54
#define ADCCHANNEL_BATTMON 0x04

#define TURN_LED_ON(x)  { PIN_LED = RESET; }
#define TURN_LED_OFF(x) { PIN_LED = SET;   }

void initBatteryMonitorAndLED() {
    // init LED -----
    GPIO_P5_SetMode(GPIO_Pin_5, GPIO_Mode_InOut_OD); // LED = open drain
    TURN_LED_OFF()

    // init ADC for Battery Monitor -----
    GPIO_P5_SetMode(GPIO_Pin_4, GPIO_Mode_Input_HIP);  // Set ADC4(GPIO P5.4) HIP
    ADC_SetClockPrescaler(0x01);    // ADC Clock = SYSCLK / 2 / (1+1) = SYSCLK / 4
    ADC_SetResultAlignmentLeft();   // Left alignment, high 8-bit in ADC_RES
    ADC_SetPowerState(HAL_State_ON); // Turn on ADC power
}

void showFirmwareVersion() {
    // Flash the LED "firmwareMinorVersion" number of times
    //  to indicate which version we are running...
    // Right now, we're NOT going to flash for firmwareMajorVersion (that's
    //   for future expansion)
    SYS_Delay(400); // wait 1/2 sec (in total) for everything to settle
    for (uint8_t i = 0; i < firmwareMinorVersion; i++) {
        // blink
        SYS_Delay(100);
        TURN_LED_ON()
        SYS_Delay(100);
        TURN_LED_OFF()
    }
    SYS_Delay(500); // wait 1/2 sec (in total) for everything to settle
}

inline void serviceBatteryMonitorAndLED() {
    // READ ADC4 value --------
    ADC_SetChannel(ADCCHANNEL_BATTMON);  // Channel: ADC4
    ADC_Start();
    nop();
    nop();

    while (!ADC_SamplingFinished());  // WAIT...

    ADC_ClearInterrupt();
    uint8_t result = ADC_RES;

    // battery voltage = 0 - 12vdc, divider = 20Kohm/67Kohm = 0.3
    //   so, battery voltage 12v maps to (12 * 0.3)/5 * 255 => 184
    //   so, battery voltage  9v maps to ( 9 * 0.3)/5 * 255 => 138

    //   so, battery voltage  7v maps to ( 7 * 0.3)/5 * 255 => 107
    //   so, battery voltage  6v maps to ( 6 * 0.3)/5 * 255 =>  91
    
    //   so, battery voltage  4.6v maps to ( 4.6 * 0.3)/5 * 255 => 70 ( I get 70!) <-- USB Stick case

    // Here's our design choice:
    // 107 ==> 7.0V = HIGH WATER MARK = LED ON (below this, light starts flashing)
    //  91 ==> 6.0V = LOW WATER MARK = LED OFF 90%/ON 10%

    // actually measured on the Mixer V2.2 board: flashing starts at 7.2V (which is fine,
    //   there's always a little uncertainty in the ADC measurement)
    //
    // Measurements predict that once flashing begins, 
    //   the AMZN Alkaline will die in about 40 hours
    //   the Tencent Lithium will die in about 18 hours
    // As battery exhaustion gets closer, the LED will be on less and less.
    //
    // NOTE: the rechargeable USB lithium batteries will die without any warning!

    // // DEBUG DEBUG DEBUG JUMPER ************
    // if (!jumperJ1_IN) {
    //     if (!jumperJ2_IN) {
    //         result = 0; // both out
    //     } else {
    //         result = 110; // just J2 in
    //     }
    // } else {
    //     if (!jumperJ2_IN) {
    //         result = 120;  // just J1 in
    //     } else {
    //         result = 130; // both in
    //     }
    // }
    // // DEBUG DEBUG DEBUG JUMPER ************

    // LED ----------------------
    // counter goes from slot 0 - 9 over 2 seconds (200ms per slot)
    int lastSlot; // slot (0-9) which is last ON slot, others are OFF
    if (result < 91) {
        lastSlot = 0;  // this is as low as we can go, LED is never fully OFF
                       // but, LED dims and MCU stops working at Battery = 3.0VDC
    } else if (result > 108) {
        lastSlot = 10; // always ON
    } else {
        // result = 91 to 108 maps to => 0 - 17 maps to => lastSlot = 1 to 9
        // 0/1 = 1
        // 2/3 = 2
        // 4/5 = 3
        // 6/7 = 4
        // 8/9 = 5
        // 10/11 = 6
        // 12/13 = 7
        // 14/15 = 8 // flashing starts here at 91+15 = 106 => 7.0 volts
        // 16/17 = 9 // still fully ON
        lastSlot = ((result - 91) >> 1) + 1; // fast integer division by 2
    }

    // example: if lastSlot is 8, turn OFF if we're in slot 9.
    if (LEDCounter > lastSlot) {
        TURN_LED_OFF()
    } else {
        TURN_LED_ON()
    }

    // LEDCounter = 0 - 9
    if (++LEDCounter >= 10) {
        LEDCounter = 0;
    }

// #ifdef DEBUG_OUTPUT
//     // PRINT IT -----
//     // UART1_TxString("ADC=0x");
//     // UART1_TxHex(result & 0xFF);
//     UART1_TxHex(jumperADC & 0xFF); // print out the jumper value
//     // UART1_TxString("\r\n");
//     UART1_TxString(",");
// #endif
}

// ==========================================================
// REMOTE VOLUME CONTROL ===========================
// ADC3 on P3.3
// VOL_CLK on P3.0
// VOL_DATA on P3.1
// VOL_LOAD on P3.2

#define PIN_ATTEN_CLK  P30
#define PIN_ATTEN_DATA P31
#define PIN_ATTEN_LOAD P32

// --------------------------------------------------------
// 0 = 0dB attenuation, >=63 = MUTE
void setAttenuation(uint8_t attenInDB) {
    // Expanding all loops so that the entire cycle takes the minimal time
    //   to set the attenuation.  This completely eliminates popping!
    // This also allows us to return from servicing ASAP, to reduce power consumption.
    PIN_ATTEN_CLK = 0;
    PIN_ATTEN_DATA = 0;
    PIN_ATTEN_LOAD = 0; // SPEC: CLOCK must go down before LOAD goes down (2 cycles = 84ns)
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
    nop(); // these might not be strictly necessary, but it makes them more visible on Saleae
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
    nop(); // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

    // DATA -----------------------------
    PIN_ATTEN_CLK = 0; // CLK low
    PIN_ATTEN_DATA = (attenInDB & 0x80) >> 7;
    nop();
    nop();
    nop(); // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
    PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
    nop(); // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

    PIN_ATTEN_CLK = 0; // CLK low
    PIN_ATTEN_DATA = (attenInDB & 0x40) >> 6;
    nop();
    nop();
    nop(); // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
    PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
    nop(); // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

    PIN_ATTEN_CLK = 0; // CLK low
    PIN_ATTEN_DATA = (attenInDB & 0x20) >> 5;
    nop();
    nop();
    nop(); // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
    PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
    nop(); // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

    PIN_ATTEN_CLK = 0; // CLK low
    PIN_ATTEN_DATA = (attenInDB & 0x10) >> 4;
    nop();
    nop();
    nop(); // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
    PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
    nop(); // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

    PIN_ATTEN_CLK = 0; // CLK low
    PIN_ATTEN_DATA = (attenInDB & 0x08) >> 3;
    nop();
    nop();
    nop(); // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
    PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
    nop(); // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

    PIN_ATTEN_CLK = 0; // CLK low
    PIN_ATTEN_DATA = (attenInDB & 0x04) >> 2;
    nop();
    nop();
    nop(); // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
    PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
    nop(); // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

    PIN_ATTEN_CLK = 0; // CLK low
    PIN_ATTEN_DATA = (attenInDB & 0x02) >> 1;
    nop();
    nop();
    nop(); // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
    PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
    nop(); // SPEC: DATA HOLD TIME > 50ns (2 cycles = 84ns)

    PIN_ATTEN_CLK = 0; // CLK low
    PIN_ATTEN_DATA = (attenInDB & 0x01);
    nop();
    nop();
    nop(); // SPEC: DATA SETUP TIME > 100ns ( 3 cycles = 120ns)
    PIN_ATTEN_CLK = 1; // CLK high <-- data on positive edge
    nop(); // SPEC: DATA HOLD TIME > 50ns (3 cycles = 120ns)
    nop();

    // finish up ------
    PIN_ATTEN_LOAD = 1; // SPEC: CLOCK TO LOAD HIGH > 50ns (2 cycles = 84ns)
    PIN_ATTEN_DATA = 1; // DATA high
}

// --------------------------------------------------------
void initRemoteVolumeControl() {
    GPIO_P3_SetMode(GPIO_Pin_3, GPIO_Mode_Input_HIP);  // Set ADC3(GPIO P3.3) High-impedance InPut
    GPIO_SetPullUp(GPIO_Port_3, GPIO_Pin_3, HAL_State_OFF);    // ensure NO PULLUP RESISTOR
    GPIO_P3_SetMode(GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2, GPIO_Mode_Output_PP); // VOL_* are Push-Pull outputs

    PIN_ATTEN_LOAD = 1; // LOAD
    PIN_ATTEN_CLK  = 1; // CLK
    PIN_ATTEN_DATA = 1; // DATA

    // TODO: These should already be set elsewhere, so comment out
    ADC_SetClockPrescaler(0x01);    // ADC Clock = SYSCLK / 2 / (1+1) = SYSCLK / 4
    ADC_SetResultAlignmentLeft();   // Left alignment, high 8-bit in ADC_RES
    ADC_SetPowerState(HAL_State_ON); // Turn on ADC power

    setAttenuation(0); // initially ZERO attenuation
}

// --------------------------------------------------------
void serviceRemoteVolumeControl() {
#define ATTEN_STEP_DELAY_US 50
    
    ADC_SetChannel(0x03);           // RVC channel = ADC3
    ADC_Start();
    nop();
    nop();

    while (!ADC_SamplingFinished());  // WAIT...

    ADC_ClearInterrupt();
    uint8_t RVCval = ADC_RES;

#ifdef DEBUG_OUTPUT
    // PRINT IT -----
    UART1_TxHex(RVCval); // print out the remote volume control value
    UART1_TxString(",");
#endif    

    // At this point, we know what the remote volume control is set to.
    // "RVCval" at this point will be:
    //
    //    open (no RVOL) = 0xFF
    //    full CCW = 0xD3
    //    full CW  = 0x00
    //
    // Most right-handed users will turn the pot CW to increase volume.
    // Some users (perhaps left-handed) might prefer to push the pot CCW to increase volume.
    //
    // Jumper J1 (read at power up time) selects between the two:
    //
    //             J1 IN          J1 OUT
    //             left-handed    right-handed
    //           +--------------+--------------+
    //   Pot CW  | VOL DOWN     | VOL UP       |
    //   Pot CCW | VOL UP       | VOL DOWN     |
    //           +--------------+--------------+
    //
    // We could use the other jumper to decide whether the min volume is ZERO (e.g.
    //   in place of the MUTE function that some Hilton cables have), or the min
    //   volume should be say 20% of the max volume (which I have heard some
    //   callers prefer).  For the moment, J2 is currently UNASSIGNED.
    //

    if (RVCval > 0xE0) {
        // no Remote Volume Control at all, so result attenuation (res) is 0dB
        res = 0;
    } else {
        // we have a Remote Volume Control plugged in

        // RVCval range = {0, 133 or so (depends on tolerance of pot)}
        uint16_t top = (RVCval << 8) - RVCval; // (RVCval * 255)
        uint16_t bot = (255 - RVCval);         // (255 - RVCval)
        uint16_t linearPotVal = top/bot;  // range: {0, 256}
            // NOTE: this divide should be fast, because STC8G1K08A has MCU16 HW divider

        if (linearPotVal > 255) {
            linearPotVal = 255; // cap this, just in case
        }

        if (jumperJ1_IN) {
            // ***** J1 IN = LEFT-HANDED or "REVERSED" VOL CTRL DIRECTION
        } else {
            // ***** J1 OUT = RIGHT-HANDED or "NORMAL" VOL CTRL DIRECTION
            linearPotVal = 255 - linearPotVal; // 255 - 0
        }

// NO SLEEPY RANGE: 0 - 133

    uint16_t q1 = linearPotVal/3;
    if (q1 >= 80) {
        // don't allow attenuations of less than 0
        q1 = 80;
    }

    res = 80 - q1;
    if (res > 64) {
        // don't allow attenuations of more than 64dB
        res = 64;
    }

    // RESULTING ATTENUATION = res, range: 0 - 64

// // RANGE: 0 - 255
// #define LOWVAL 4
// #define DIVIDER 4
// // #define HIGHVAL (LOWVAL + DIVIDER * 64)
// #define HIGHVAL 251

//         if (linearPotVal < LOWVAL) {
//             // lowest ~6% on the pot
//             res = 64; // MUTE (96dB attenuation); NOTE: do not use 255 here, because we must step the attenuation
//         } else if (linearPotVal > HIGHVAL) {
//             // highest ~5% on the pot
//             res = 0;   // MAX VOL (0dB attenuation)
//         } else {
//             // linearPotVal range now 61 to 959 = 898 steps = ~64*14
//             // (linearPotVal - 61) = 0 to 898
//             // (linearPotVal - 61)/14 = 0 to 64
//             // 64 - (linearPotVal - 61)/14 = 64 to 0
//             res = 64 - (linearPotVal - LOWVAL)/DIVIDER; // 64dB to 0dB
//             // NOTE: this divide should be fast, because STC8G1K08A has MCU16 HW divider
//         }

        // // OLD CODE -------
        // if (RVCval < 15) {
        //     res = 64; // MUTE (96dB attenuation); NOTE: do not use 255 here, because we must step the attenuation
        // } else if (RVCval >= 0xD0) {
        //     // max ADC is 0xD3 = 211
        //     // guard band is 0xD0 = 208
        //     res = 0;   // MAX VOL (0dB attenuation)
        // } else {
        //     // RVCval range now 15 to 207 = 192 steps = ~64*3
        //     // (RVCval - 15) = 0 to 192
        //     // (RVCval - 15)/3 = 0 to 64
        //     // 64 - (RVCval - 15)/3 = 64 to 0
        //     res = 64 - (RVCval - 15)/3; // 64dB to 0dB
        //     // NOTE: this divide should be fast, because STC8G1K08A has MCU16 HW divider
        // }
    }

// #ifdef DEBUG_OUTPUT
//     // PRINT DETECTED JUMPER VALUES -----
//     UART1_TxString("\nJUMPER ADC: ");
//     UART1_TxHex(jumperADC & 0xFF);
//     UART1_TxString(", JUMPER J1: ");
//     if (jumperJ1_IN) {
//         UART1_TxString("IN");
//     } else {
//         UART1_TxString("OUT");
//     }
//     UART1_TxString(", JUMPER J2: ");
//     if (jumperJ2_IN) {
//         UART1_TxString("IN");
//     } else {
//         UART1_TxString("OUT");
//     }
//     UART1_TxString("\n");
// #endif

#ifdef DEBUG_OUTPUT
    // PRINT IT -----
    UART1_TxHex(res & 0xFF); // print out the attenuation in dB
    UART1_TxString("\n");
#endif

    // step it up or down in increments of 1 dB to avoid zipper effect
    if (previousRes < res) {
        // step UP to get to correct level of attenuation
        for (uint8_t a = previousRes+1; a < res; a++) {
            setAttenuation(a);
#if ATTEN_STEP_DELAY_US > 0
            SYS_DelayUs(ATTEN_STEP_DELAY_US);
#endif            
        }
        setAttenuation(res);
    } else if (res < previousRes) {
        // step DOWN to get to correct level of attenuation
        for (uint8_t a = previousRes-1; a > res; a--) {
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
    previousRes = res; // remember where we are now
}

// ==========================================================
// UART ==========================
#ifdef DEBUG_OUTPUT
void initUART() {
    // UART1_Config8bitUart(UART1_BaudSource_Timer1, HAL_State_ON, 115200); // 115200/8/N/1
    // UART1_Config8bitUart(UART1_BaudSource_Timer1, HAL_State_ON, 115200); // 115200/8/N/1
    UART1_Config8bitUart(UART1_BaudSource_Timer1, HAL_State_ON, 160911); // =115200/8/N/1  FIX FIX FIX: slow MCU clock
}
#endif

// ==========================================================
// POWER CONTROL ==================
#define M_PD 0x2
void enterPowerDownMode() {
	PCON |= M_PD;
	nop();
	nop();
}

void initSleepControl() {
#ifdef SLEEPY
    // ENABLE WATCHDOG WAKEUP -----
    // see section 6.6.1 of the STC8G-EN.pdf manual for a table of these

    // WKTCL = 0x01;
    // WKTCH = 0x80;   // wake up every 1ms (1000X/sec), good for 1:16 PWM on LED

    WKTCL = 0x63;
    WKTCH = 0x80;   // wake up every 50ms (20X/sec) 

    // WKTCL = 0x8F;
    // WKTCH = 0x81;   // wake up every 200ms (5X/sec), spreadsheet gives 0x018F, the 0x80 is WKT ENABLE
#endif
}

inline void sleepUntilTimeToWakeup() {
#ifdef SLEEPY
    // GET READY TO SLEEP AGAIN -----
    P3M1 &= ~0x7F; // all pins on P3 go to sleep right now, except ADC:P3.3
    P3M0 &= ~0x7F;
    P3 |= 0xFF;

    P5M1 &= ~0xCF; // all pins on P5 go to sleep except LED:P5.5, BATTMON: P5.4
    P5M0 &= ~0xCF;
    //P5 |= 0xDF;

    enterPowerDownMode();
#else        
    SYS_Delay(50); // 20X/sec
#endif        
}

// ===============================================================================
// MAIN ==========================================================================
// ===============================================================================
void main()
{
    SYS_SetClock();  // Set system clock. Removed because system clock is already set by STC-ISP X86

    initJumperRead();  // read the jumper value J1/J2 (analog value)
    initRemoteVolumeControl();    
    initBatteryMonitorAndLED();
    initSleepControl();

    // before we get going, service these once
    serviceRemoteVolumeControl();  // service once to set volume
    serviceBatteryMonitorAndLED(); // service once to set LED

    // this takes a while...
    showFirmwareVersion();

#ifdef DEBUG_OUTPUT
    initUART();
#endif

    uint8_t batteryCounter = 0;

    // MAIN LOOP -----------------
    while(1)
    {
        // WAKE UP!
        if (batteryCounter++ & 0x03 == 0x03) {
            // Running the wakeups at 20X/sec now, so let's call the battery 
            //   monitor only every 4th time, so that it gets called only every 200ms.
            // There are 10 slots, so the cycle time for battery animations
            //   is back down to every 2 seconds.
            serviceBatteryMonitorAndLED();
        }

        serviceRemoteVolumeControl();  // service this one 20X/sec, so no lag

        // BACK TO SLEEP...
        sleepUntilTimeToWakeup();  // 20X/sec
    }
}