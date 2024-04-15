// +-----------------------------------------------+
// | BLINKTEST: Firmware for Mixer Board V2.2      |
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

uint8_t jumperADC = 0;
bool    jumperJ1_IN = false;
bool    jumperJ2_IN = false;

uint8_t res = 0;          // attenuation result
uint8_t previousRes = 0;  // previous attenuation result

// SLEEPY: minimize power, but doesn't allow automatic Upload from programmer @ CTL-OPT-U time
#define SLEEPY

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
    //   so, battery voltage  4.6v maps to ( 4.6 * 0.3)/5 * 255 => 70 ( I get 70!) <-- USB Stick case

    // 130 ==> 8.5V = HIGH WATER MARK = LED ON
    // 100 ==> 6.5V = LOW WATER MARK = LED OFF 90%/ON10%

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
    if (result < 100) {
        lastSlot = 0;  // this is as low as we can go, LED is never fully OFF
                       // but, LED dims and MCU stops working at Battery = 3.0VDC
    } else if (result >= 130) {
        lastSlot = 10; // always ON
    } else {
        // 100 - 129 => 0 - 29 => 0 - 9
        lastSlot = (result - 100)/3; // integer division
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

    // At this point, we know what the remote volume control is set to.
    // "RVCval" at this point will be:
    //    full CCW = 0xD3 (increase vol for RH people)
    //    full CW  = 0x00 (mute for RH people)
    //
    // Most right-handed users will push the pot CCW to increase volume.
    // Left-handed users might push the pot CW to increase volume.
    // We could use one of the jumpers to set the direction of the pot.
    // For example, if jumper was IN, full CCW = max volume, and
    //   if jumper is OUT, full CCW = min volume.
    // 
    // We could use the other jumper to decide whether the min volume is ZERO (e.g.
    //   in place of the MUTE function that some Hilton cables have), or the min
    //   volume should be say 20% of the max volume (which I have heard some
    //   callers prefer).
    //
    // TODO: Mapping between the current value of "result" and what we should send
    //   to the LM1971 attenuator should be done here.

// #ifdef DEBUG_OUTPUT
//     // PRINT IT -----
//     UART1_TxHex(result & 0xFF); // print out the remote volume control value
//     UART1_TxString(",");
// #endif

    // range is 0x00 - 0xD3, which is 0 - 211
    // let's assume that it's 0 - 0xD0, which is 0 - 208
    // guard bands at top (MAX VOL) and bottom (MUTE) of range, of (208 - 180)/2 = 14
    // 
    if (RVCval < 15) {
        res = 64; // MUTE (96dB attenuation); NOTE: do not use 255 here, because we must step the attenuation
    } else if (RVCval >= 0xD0) {
        // max ADC is 0xD3 = 211
        // guard band is 0xD0 = 208
        res = 0;   // MAX VOL (0dB attenuation)
    } else {
        // RVCval range now 15 to 207 = 192 steps = ~64*3
        // (RVCval - 15) = 0 to 192
        // (RVCval - 15)/3 = 0 to 64
        // 64 - (RVCval - 15)/3 = 64 to 0
        res = 64 - (RVCval - 15)/3; // 64dB to 0dB
        // NOTE: this divide should be fast, because STC8G1K08A has MCU16 HW divider
    }

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
    P3M1 &= ~0xFF; // all pins on P3 go to sleep right now, since no REMOTE VOL yet
    P3M0 &= ~0xFF;
    P3 |= 0xFF;

    P5M1 &= ~0xCF; // all pins on P5 go to sleep except LED:P5.5, BATTMON: P5.4
    P5M0 &= ~0xCF;
    //P5 |= 0xDF;

    enterPowerDownMode();
#else        
    SYS_Delay(200); // 5X/sec
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