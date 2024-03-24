// +-----------------------------------+
// | Copyright (c) 2024, Michael Pogue |
// | License: GPL V3                   |
// +-----------------------------------+

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "fw_hal.h"

// GLOBALS =============================
int LEDCounter = 0;

// SLEEPY: minimize power, but doesn't allow automatic Upload from programmer @ CTL-OPT-U time
#define SLEEPY

// DEBUG_OUTPUT: enable UART, uses power and cpu cycles, but easier to debug
// #define DEBUG_OUTPUT

// UTILS ================================
#define nop() __asm__(" nop");

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
    // 130 ==> 8.5V = HIGH WATER MARK
    // 100 ==> 6.5V = LOW WATER MARK

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

#ifdef DEBUG_OUTPUT
    // PRINT IT -----
    // UART1_TxString("ADC=0x");
    UART1_TxHex(result & 0xFF);
    // UART1_TxString("\r\n");
    UART1_TxString(",");
#endif
}

// UART ==========================
#ifdef DEBUG_OUTPUT
void initUART() {
    // UART1_Config8bitUart(UART1_BaudSource_Timer1, HAL_State_ON, 115200); // 115200/8/N/1
    // UART1_Config8bitUart(UART1_BaudSource_Timer1, HAL_State_ON, 115200); // 115200/8/N/1
    UART1_Config8bitUart(UART1_BaudSource_Timer1, HAL_State_ON, 160911); // =115200/8/N/1  FIX FIX FIX: slow MCU clock
}
#endif

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
    // WKTCL = 0x63;
    // WKTCH = 0x80;   // wake up every 50ms (20X/sec) 
    // WKTCL = 0x01;
    // WKTCH = 0x80;   // wake up every 1ms (1000X/sec), good for 1:16 PWM on LED
    WKTCL = 0x8F;
    WKTCH = 0x81;   // wake up every 200ms (5X/sec), spreadsheet gives 0x018F, the 0x80 is WKT ENABLE
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

    initBatteryMonitorAndLED();
    initSleepControl();

#ifdef DEBUG_OUTPUT
    initUART();
#endif

    uint8_t currentADC = 0;

    // MAIN LOOP -----------------
    while(1)
    {
        // WAKE UP!
        serviceBatteryMonitorAndLED();

        // BACK TO SLEEP...
        sleepUntilTimeToWakeup();  // 5X/sec
    }
}