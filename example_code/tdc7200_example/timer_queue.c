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

#define TA0_DELAY_MODE		0
#define TA0_TIMEOUT_MODE	1

// Timer A1		interval timer 100mS
// Timer A0		for uS/mS delay or mS timeout

volatile uint16_t	timer_trigger_freq = 1;
volatile uint8_t	timeout = 1;

static uint8_t		ta0mode, interval_timer_count;

void start_ms_timeout(uint16_t ms)
{
	ta0mode			= TA0_TIMEOUT_MODE;
	timeout			= 0;
	
	TA0CCR0		= 33*ms;
	TA0CCTL0	= CCIE;
	TA0CTL		= MC__UP + ID__1 + TASSEL__ACLK + TACLR;
}

void stop_ms_timeout(void)
{
	TA0CTL			= 0;
	TA0CCTL0		= 0;
}

void delay_ACLK(uint16_t aclk)
{
	ta0mode			= TA0_DELAY_MODE;

	// Timer setting based on 32.768kHz ACLK
	TA0CCR0	 = aclk;
	TA0CCTL0 = CCIE;
	TA0CTL	 = MC__UP + ID__1 + TASSEL__ACLK + TACLR;
	
	LPM3;
}

void delay_uS(uint16_t us)
{
	ta0mode			= TA0_DELAY_MODE;
	
	// Timer setting based on 8MHz SMCLK
	TA0CCR0			= us;
	TA0CCTL0 = CCIE;
	TA0CTL	= MC__UP + ID__8 + TASSEL__SMCLK + TACLR;
	
	LPM0;
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void ISR_TIMER0_A0(void)
{
	if (ta0mode == TA0_DELAY_MODE)
	{
			TA0CTL = 0;
			TA0CCTL0 = 0;
			LPM3_EXIT;

	}
	else if (ta0mode == TA0_TIMEOUT_MODE)
	{
			TA0CTL = 0;
			TA0CCTL0 = 0;
			timeout = 1;
			LPM3_EXIT;
	}
}

void pause_interval_timer(void)
{
	TA1CCTL0 &= ~CCIE;
	TA1CTL &= ~MC__UP;
}

void resume_interval_timer(void)
{
	TA1CCTL0 |= CCIE;
	TA1CTL |= MC__UP;
}

void interval_timer_init(void)
{
	interval_timer_count = timer_trigger_freq;
	TA1CCR0 = 3276;
	TA1CCTL0 = CCIE;
	TA1CTL	= MC__UP + TASSEL__ACLK + TACLR;
}


#pragma vector=TIMER1_A0_VECTOR
__interrupt void ISR_TIMER1_A0(void)
{
	if (!interval_timer_count--)
	{
		action_items |= HANDLE_NEXT_TRIGGER;
		interval_timer_count = timer_trigger_freq; 
	}
	if (action_items&HANDLE_NEXT_TRIGGER)
	{
		LPM3_EXIT;
	}

	if (action_items&HANDLE_COMMAND)
	{
		LPM3_EXIT;
	}

}
