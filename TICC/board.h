#ifndef BOARD_H
#define BOARD_H

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2019
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include "config.h"

// I/0 assignments
#if (BOARD_REVISION == 'C')  // rev C board had different mapping for these pins
const int OUT1 =        2;   // spare output
const int OUT2 =        3;   // spare output
const int STOP_0 =      12;
const int STOP_1 =      13;
#elif (BOARD_REVISION == 'D')
const int STOP_0 =      2;   // PINE,4
const int STOP_1 =      3;   // PINE,5
const int OUT1 =        12;  // spare output
const int OUT2 =        13;  // spare output
#endif

const int ENABLE_0 = 	  4;
const int ENABLE_1 =    5;
const int CSB_0  =      6;
const int CSB_1 =       7;
const int INP1 =        8;   // spare input
const int INP2 =        9;   // spare input
const int INTB_0 =  	  10;  // PINB,4
const int INTB_1 =      11;  // PINB,5
const int D16 =         16;  // spare unassigned
const int D17 =         17;  // spare unassigned
const int COARSEint =   18;  // hardware interrupt for COARSE clock
const int SLAVE_SYNC =  A8;  // use to sync multiple boards
const int AN9 =         A9;  // spare unassigned
const int AN10 =        A10; // spare unassigned
const int EXT_LED_CLK = A11; // external LED shows 100kHz clock status -- PORTK,3
const int EXT_LED_0 =   A12; // external LED tandem with LED_0 -- PORTK,4
const int EXT_LED_1 =   A13; // external LED tandem with LED_1 -- PORTK,5
const int LED_0 =       A14; // onboard LED -- PORTK,6
const int LED_1 =       A15; // onboard LED -- PORTK,7

// These are macros to turn LEDs on and off really fast.
// We trade flexibility for speed.

#define CLR_LED_0 (PORTK&=(~(1<<6)))
#define SET_LED_0 (PORTK|=(1<<6))
#define CLR_LED_1 (PORTK&=(~(1<<7)))
#define SET_LED_1 (PORTK|=(1<<7))
#define CLR_EXT_LED_0 (PORTK&=(~(1<<4)))
#define SET_EXT_LED_0 (PORTK|=(1<<4))
#define CLR_EXT_LED_1 (PORTK&=(~(1<<5)))
#define SET_EXT_LED_1 (PORTK|=(1<<5))
#define CLR_EXT_LED_CLK (PORTK&=(~(1<<3)))
#define SET_EXT_LED_CLK (PORTK|=(1<<3))

#endif	/* BOARD_H */
