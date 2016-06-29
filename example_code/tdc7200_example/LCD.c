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

const unsigned char lcd_segment_lower[11] = {
	0xFC,		// 0
	0x60,		// 1
	0xDB,		// 2
	0xF3,		// 3
	0x67,		// 4
	0xB7,		// 5
	0xBF,		// 6
	0xE0,		// 7
	0xFF,		// 8
	0xF7,		// 9
	0x03,       // -
};

volatile unsigned char *lcd_mem_lower[6] = {
	&LCDM11,
	&LCDM9,
	&LCDM7,
	&LCDM5,
	&LCDM8,
	&LCDM2,
};

void LCD_init(void)
{
	// LCD_C
	// Enable COMx Pin
	P6SELC		|= BIT3 + BIT4 + BIT5 + BIT6;
	
	LCDCPCTL0	= 0xF30C;
	LCDCPCTL1	= 0x0033;

#ifdef LCD_USE_CHARGE_PUMP
	LCDCVCTL	= VLCD_1 + LCDCPEN;									// Use charge pump
	LCDCCPCTL	= LCDCPCLKSYNC;										// Synchronize charge pump with internal clock
#else
	P6SELC		= BIT0 + BIT1 + BIT2;								// select external resistor
	LCDCVCTL	= LCDREXT + R03EXT +LCDEXTBIAS;						// Use resistor
#endif
	LCDCMEMCTL	= LCDCLRM;											// Clear LCD memory
	LCDCCTL0	= LCDDIV_3 + LCDPRE_5 + LCD4MUX + LCDLP  + LCDON;	// 4 MUX, Low power waveform, use ACLK, turn on LCD
}


// Display number on the LCD screen
// Input:
//		num:		number to be displayed on the LCD
void lcd_display_num(int num)
{
	static	const int		divider[5]= {1,10,100,1000,10000};
	unsigned char			digit, temp = 0;
	unsigned char			i = 5;
	unsigned char           sign;

	// Only 4 digits are needed for unsigned integer
	//	divider = 1000;
	sign = 0;

	if (num < 0)
	{
		num *= -1;
		sign = 1;

	}
	
	// Start from most significant digit
	while (i--)
	{
		digit = 0;
		while (num >= divider[i])
		{
			num -= divider[i];
			digit++;
		}

		if (digit || temp || i == 0)
		{
			*lcd_mem_lower[i] = lcd_segment_lower[digit];
			temp	= 1;
		}
		else
		{
			*lcd_mem_lower[i] = 0;
		}
	//		divider	/= 10;
	}

	if (sign == 1)
	{*lcd_mem_lower[5] = lcd_segment_lower[10];}
	else
	{*lcd_mem_lower[5] = 0x00;}
}
