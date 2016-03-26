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

volatile uint16_t uart_cnt = 0 ;

void UART_Init(void)
{
	// eUSCI UART (9600 baud @8MHz)
///	P4SEL0		|= BIT2 + BIT3;            		/// to be set when active
	UCA0CTL1	|= UCSWRST;						// Put eUSCI in reset
	UCA0CTL1	|= UCSSEL__SMCLK;				// CLK = SMCLK
//	UCA0CTL1	|= UCSSEL__ACLK;				// CLK = ACLK
	UCA0BRW		= 52;							// UCBRx = 52
//	UCA0BRW		= 3;							// UCBRx = 3 for ACLK
	UCA0MCTLW	= 0x4900 + UCBRF_1 + UCOS16;	// UCBS = 0x49, UCBRF = 0, UCOS16 = 1
//	UCA0MCTLW	= 0x9200 ;						// UCBS = 0x92, UCBRF = 0, UCOS16 = 0

///	UCA0CTL1	&= ~UCSWRST;					// Initialize eUSCI
	UCA0IE		|= UCRXIE;						// Enable USCI_A0 RX interrupt
	UCA0IFG		&= ~UCRXIFG;
	
	// DMA Settings
	DMACTL0 = DMA1TSEL_15;						// Trigger source UCA0TXIFG
	
	// Single Transfer, DST: increment, SRC: static
	// Byte 2 Byte, Edge sensitive
	DMA1CTL = DMADT_0 + DMADSTINCR_0 + DMASRCINCR_3 + DMADSTBYTE + DMASRCBYTE;
	
	// Destination address  	
	__data20_write_long((unsigned long) &DMA1DA,(unsigned long) &UCA0TXBUF);

}

void UART_Send_Bytes(uint8_t *address, uint8_t size)
{
	// Source address
	__data20_write_long((unsigned long) &DMA1SA,(unsigned long) address);
	
	// Number of data to store
	DMA1SZ	= size;
	DMA1CTL	|= DMAEN + DMAIE;					// Enable DMA.
	
	// make a fake edge trigger to activate DMA
	UCA0IFG &= ~UCTXIFG;
	UCA0IFG |= UCTXIFG;
	
	LPM0;
	DMA1CTL	&= ~(DMAEN + DMAIE);				// Disable DMA.
}

#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
	switch(__even_in_range(UCA0IV,USCI_UART_UCTXCPTIFG))
	{
	case USCI_NONE: break;
	case USCI_UART_UCRXIFG:

		if (!(action_items&HANDLE_COMMAND))
		{
		cmdResponseString[uart_cnt] = UCA0RXBUF;
		
		if (++uart_cnt < MAX_STR_LENGTH)
		{
			receiving_cmd |= BIT0;

			TA2CCR0	= 100;
			TA2CTL	= MC__UP + TASSEL__ACLK + TAIE + TACLR;
		}
		else
		{
			action_items |= HANDLE_COMMAND;
			TA2CTL = 0;
			uart_cnt = 0;

		}
		}

		else
		{__no_operation();}

		break;
		
	case USCI_UART_UCTXIFG: break;
	case USCI_UART_UCSTTIFG: break;
	case USCI_UART_UCTXCPTIFG: break;
	}
}

#pragma vector=TIMER2_A1_VECTOR
__interrupt void ISR_TIMER2_A1(void)
{
	TA2CTL = 0;
	uart_cnt = 0;
}

//========================= Interrupt Service Routine ==========================
#pragma vector=DMA_VECTOR
__interrupt void DMA_ISR(void)
{
	switch(__even_in_range(DMAIV, 0x10))
	{
		case 0x00: break;					// No interrupt
		case 0x02: break;
		case 0x04:							// DMA1IFG (for UART Operation)
			DMA1CTL &= ~(DMAIFG|DMAIE);		// Clear flag and stop interrupt
			LPM0_EXIT;
			break;
		case 0x06: break;					// DMA2IFG
		case 0x08: break;					// DMA3IFG
		case 0x0A: break;					// DMA4IFG
		case 0x0C: break;					// DMA5IFG
		case 0x0E: break;					// DMA6IFG
		case 0x10: break;					// DMA7IFG
	}
}
