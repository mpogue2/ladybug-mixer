// STC8G1K08A specific definitions
// This header provides additional definitions for STC8G series

#ifndef _STC8G_H_
#define _STC8G_H_

#include <8051.h>

// Port 5 and its configuration registers
__sfr __at (0xC8) P5;

// Port configuration registers for STC8G series
// PxM1 and PxM0 control the pin mode:
// M1 M0  Mode
//  0  0  Quasi-bidirectional (default)
//  0  1  Push-pull output
//  1  0  Input-only (high-impedance)
//  1  1  Open-drain output

// Port 3 mode registers
__sfr __at (0xB1) P3M1;
__sfr __at (0xB2) P3M0;

// Port 5 mode registers
__sfr __at (0xC9) P5M1;
__sfr __at (0xCA) P5M0;

// ADC registers
__sfr __at (0xBC) ADC_CONTR;  // ADC control register
__sfr __at (0xBD) ADC_RES;    // ADC result high byte
__sfr __at (0xBE) ADC_RESL;   // ADC result low byte
__sfr __at (0xDE) ADCCFG;     // ADC configuration register
__sfr __at (0xF1) P1ASF;      // Port 1 analog switch function
__sfr __at (0xF5) P5ASF;      // Port 5 analog switch function

// ADC_CONTR bits
#define ADC_POWER   0x80  // ADC power control bit
#define ADC_START   0x40  // ADC start conversion bit
#define ADC_FLAG    0x20  // ADC conversion complete flag

// Pin definitions
__sbit __at (0xB3) P3_3;  // P3.3 for LED (CCP1)
__sbit __at (0xCD) P5_5;  // P5.5 for ADC input

// Timer registers (from 8051.h, but explicit for clarity)
__sfr __at (0x8C) TH0;    // Timer 0 high byte
__sfr __at (0x8A) TL0;    // Timer 0 low byte
__sfr __at (0x89) TMOD;   // Timer mode register
__sfr __at (0x88) TCON;   // Timer control register
__sfr __at (0xA8) IE;     // Interrupt enable register
__sfr __at (0x8E) AUXR;   // Auxiliary register

// Power control register
__sfr __at (0x87) PCON;   // Power control register

// PCON bits
#define IDL     0x01      // Idle mode bit

// IE bits (Interrupt Enable) - bit-addressable at 0xA8
__sbit __at (0xAF) EA;    // IE.7 - Global interrupt enable
__sbit __at (0xA9) ET0;   // IE.1 - Timer 0 interrupt enable

// TCON bits - bit-addressable at 0x88
__sbit __at (0x8D) TF0;   // TCON.5 - Timer 0 overflow flag
__sbit __at (0x8C) TR0;   // TCON.4 - Timer 0 run control

// PCA (Programmable Counter Array) registers for PWM
__sfr __at (0xD8) CCON;    // PCA control register
__sfr __at (0xD9) CMOD;    // PCA mode register
__sfr __at (0xE9) CL;      // PCA counter low byte
__sfr __at (0xF9) CH;      // PCA counter high byte
__sfr __at (0xDA) CCAPM0;  // PCA module 0 mode
__sfr __at (0xDB) CCAPM1;  // PCA module 1 mode
__sfr __at (0xDC) CCAPM2;  // PCA module 2 mode
__sfr __at (0xEA) CCAP0L;  // PCA module 0 capture/compare low
__sfr __at (0xEB) CCAP1L;  // PCA module 1 capture/compare low
__sfr __at (0xEC) CCAP2L;  // PCA module 2 capture/compare low
__sfr __at (0xFA) CCAP0H;  // PCA module 0 capture/compare high
__sfr __at (0xFB) CCAP1H;  // PCA module 1 capture/compare high
__sfr __at (0xFC) CCAP2H;  // PCA module 2 capture/compare high
__sfr __at (0xF2) PCA_PWM0; // PCA PWM mode register 0
__sfr __at (0xF3) PCA_PWM1; // PCA PWM mode register 1
// PCA_PWM1 bit definitions for module 1 (STC8G)
#define EPC1L_BIT 0x01   // EPC1L: low byte MSB bit
#define EPC1H_BIT 0x02   // EPC1H: high byte MSB bit
#define XCCAP1L_MASK 0x0C // Bits [3:2] extended CCAP1 low (for 10-bit)
#define XCCAP1H_MASK 0x30 // Bits [5:4] extended CCAP1 high (for 10-bit)
#define EBS1_MASK    0xC0 // Bits [7:6] EBS1 mode select (00=8-bit)
__sfr __at (0xF4) PCA_PWM2; // PCA PWM mode register 2

// PCA pin switch register (to route PCA output to specific pins)
__sfr __at (0xA2) P_SW1;   // Peripheral switch register 1

// Extended SFR access switch (needed to access PCA_PWMx registers)
__sfr __at (0xBA) P_SW2;   // Peripheral switch register 2 (EAXFR)
#define EAXFR 0x80         // Enable access to extended SFRs when set

// CCON bits - bit-addressable at 0xD8
__sbit __at (0xDE) CF;     // CCON.6 - PCA overflow flag
__sbit __at (0xDF) CR;     // CCON.7 - PCA counter run control

// CMOD bits
#define CIDL    0x80       // CMOD.7 - PCA continues in IDLE mode when 0
#define ECF     0x01       // CMOD.0 - PCA overflow interrupt enable

// CCAPM bits (PWM mode)
#define ECOM    0x40       // Enable comparator
#define PWM_MODE 0x42      // PWM mode (ECOM=1, PWM=1)

// P_SW1 bits for CCP pin routing
#define CCP_S1  0x20       // CCP pin switch bit 1
#define CCP_S0  0x10       // CCP pin switch bit 0
// CCP_S1:CCP_S0 = 00: Default location
// CCP_S1:CCP_S0 = 01: Alternative location 1
// CCP_S1:CCP_S0 = 10: Alternative location 2
// CCP_S1:CCP_S0 = 11: Alternative location 3

// System clock divider
__sfr __at (0x97) CLKDIV;  // System clock divider (0=no divide)

#endif // _STC8G_H_
