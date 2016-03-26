/* --COPYRIGHT--,BSD
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/

#include "all_header.h"

void init_board(void)
{

	__disable_interrupt();								// Disable global interrupts
	Init_Ports();										//Init ports (do first ports because clocks do change ports)
	Init_Clock();										//Init clocks

	UART_Init();
	LCD_init();
	LCDCCTL0 &= ~LCDON;	                				// turn off LCD

	__enable_interrupt();								// enable global interrupts


}

//******************************************************************************
void Init_Clock(void)
{
	// Enable XT2 XIN/XOUT Pins
#ifdef REVISION_2
	PJSEL0 |= BIT4 + BIT5;
#else
	PJSEL0 |= BIT4 + BIT5 + BIT6 + BIT7;				// Select XIN, XOUT on PJ.6 and PJ.7
#endif
	CSCTL0	= 0XA500;									// Write in the password for system clock setting
	CSCTL1	= DCOFSEL_3 + DCORSEL;						// DCO = 8MHz
	CSCTL2	= SELA__LFXTCLK
			+ SELS__DCOCLK
			+ SELM__DCOCLK;

	CSCTL3	= DIVA__1
			+ DIVS__1
			+ DIVM__1;									// ACLK div by 1, SMCLK div by 1, MCLK div by 1
	// CSCTL4	= HFXTDRIVE_3 + HFFREQ_2
	CSCTL4	= HFXTOFF
			+ LFXTDRIVE_3
			+ VLOOFF;

	do
	{
		CSCTL5 &= ~(LFXTOFFG + HFXTOFFG);				// Clear XT1 fault flag
		SFRIFG1 &= ~OFIFG;
	}while (CSCTL5 & LFXTOFFG);			       			// Test oscillator fault flag

	CSCTL4 = LFXTDRIVE_0 + HFXTOFF + VLOOFF;

}

void Init_Power(void)
{
}

void Init_Ports(void)
{
	PADIR	= 0xffff;
	PAOUT	= 0x0000;
	PBDIR	= 0xffff;
	PBOUT	= 0x0000;
	PCDIR	= 0xffff;
	PCOUT	= 0x0000;
	PDDIR	= 0xffff;
	PDOUT	= 0x0000;
	PEDIR	= 0xffff;
	PEOUT	= 0x0000;
	PJDIR	= 0xffff;
	PJOUT	= 0x0000;

	P1OUT  |= BIT2;

	// DCDC Voltage select
	VSEL_PxDIR	|= VSEL4 + VSEL3 + VSEL2 + VSEL1;
	set_dcdc_voltage(DCDC_3_3V);
	
	// LOAD_ENABLE
	LOAD_DISABLE;
	LOAD_ENABLE_PxDIR |= LOAD_ENABLE_PIN;

	// VDD_ENABLE
	VDD_DISABLE;
	VDD_ENABLE_PxDIR |= VDD_ENABLE_PIN;

	// Link LED
	TDC1000_LINK_LED_PxOUT	|= TDC1000_LINK_LED_PIN;		// Set LINK_LED OFF
	TDC1000_LINK_LED_PxDIR	|= TDC1000_LINK_LED_PIN;		// Set pin direction is output

	// Measurement LED
	TDC1000_MEAS_LED_PxOUT	|= TDC1000_MEAS_LED_PIN;		// Set MEAS_LED OFF
	TDC1000_MEAS_LED_PxDIR	|= TDC1000_MEAS_LED_PIN;		// Set pin direction is output

	// External OSC Pin
	TDC1000_OSCENABLE_PxOUT	&= ~TDC1000_OSCENABLE_PIN;		// Disable external OSC by default
	TDC1000_OSCENABLE_PxDIR	|= TDC1000_OSCENABLE_PIN;		// Set pin direction is output

	// TDC1000 Reset Pin
	TDC1000_RESET_PxDIR		|= TDC1000_RESET_PIN;			// Reset Pin

	// TDC1000 Enable Pin
	TDC1000_ENABLE_PxOUT		&= ~TDC1000_ENABLE_PIN;		// Disable TDC1000 by default
	TDC1000_ENABLE_PxDIR		|= TDC1000_ENABLE_PIN;		// Set pin direction is output

	// TDC7200 Enable Pin
	TDC7200_ENABLE_PxOUT		&= ~TDC7200_ENABLE_PIN;		// Disable TDC7200 by default
	TDC7200_ENABLE_PxDIR		|= TDC7200_ENABLE_PIN;		// Set pin direction output

	// TDC7200 INT Pin
	TDC7200_INTB_PxDIR		&= ~TDC7200_INTB_PIN;			// Set up port pin for INTB
	TDC7200_INTB_PxOUT		|= TDC7200_INTB_PIN;			// INTB Input Pull up enable
	TDC7200_INTB_PxIES		|= TDC7200_INTB_PIN;			// Interrupt Edge Select
	TDC7200_INTB_PxIFG		&= ~TDC7200_INTB_PIN;			// Clear Interrupt Flag

	PM5CTL0 &= ~LOCKLPM5;;        	// In initialization, the I/Os are configured before unlocking the I/O ports

	PMMCTL0_H = 0xA5;
//	PMMCTL0_L |= PMMREGOFF;
	PMMCTL0_L &= ~SVSHE;
	PMMCTL0_H = 0xEE;

}


// Set DCDC regulator output voltage
void set_dcdc_voltage(uint8_t voltage)
{
	unsigned char dummy;

	dummy = VSEL_PxOUT;
    dummy &= 0xF0;
	dummy |= voltage;
	VSEL_PxOUT = dummy;

}

// Set charge mode for RF circuit
void charge_mode(uint8_t charge_mode)
{
	CHARGE_PxOUT &= ~(FAST_CHARGE_PIN + SLOW_CHARGE_PIN);
	CHARGE_PxOUT = charge_mode;
}

// Set impedance matching channel select
void im_ch_sel(uint8_t channel)
{
	IM_CH_SEL_PxOUT &= ~(IM_CH1_SEL_PIN + IM_CH2_SEL_PIN);
	IM_CH_SEL_PxOUT |= channel;
}


