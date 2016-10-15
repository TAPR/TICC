#ifndef BOARD_H
#define BOARD_H

// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.79 14 October 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

// hardware connections to TDC2700. Defines Arduino IDE pin number.
// changed for Rev. C board
const int ENABLE_A =		4;
const int INTB_A =		10;
const int CSB_A	=  		6;
const int STOP_A =		12;

const int ENABLE_B =		5;
const int INTB_B =		11;
const int CSB_B	=	  	7;
const int STOP_B =		13;

// interrupt assignments
const int COARSEint = 18;   // Interrupt for COARSE clock
const int STOPAint =  19;   // Interrupt for STOPA
const int STOPBint =  20;   // Interrupt for STOP

#endif	/* BOARD_H */
