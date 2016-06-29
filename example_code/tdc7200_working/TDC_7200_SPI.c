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

void TDC_SPISetup(void)
{

	TDC7200_CSn_PxDIR |= TDC7200_CSn_PIN;
	TDC1000_CSn_PxDIR |= TDC1000_CSn_PIN;

	
	UCB1_PxSEL0 |= UCB1_SIMO_PIN + UCB1_SOMI_PIN + UCB1_UCLK_PIN;
	UCB1_PxSEL1 &= ~(UCB1_SIMO_PIN + UCB1_SOMI_PIN + UCB1_UCLK_PIN);
	
	UCB1CTLW0 |= UCSWRST;
	
	// 3-pin, 8-bit SPI master
	// Clock polarity high, MSB
	// Clock Source: SMCLK
	UCB1CTLW0 |= UCMST + UCSYNC + UCCKPL + UCMSB + UCSSEL_2;
	
	UCB1BRW = 0;

}

void TDC1000_SPIByteWriteReg(uint8_t addr, uint8_t value)
{
	addr |= TDC1000_WRITE_BIT;			// for write make sure bit6 is 1 
	
	TDC1000_CS_EN;
	
	UCB1IFG		&= ~UCRXIFG;
	UCB1IE		|= UCRXIE;
	UCB1TXBUF	= addr;
	LPM0;
	
	UCB1IFG		&= ~UCRXIFG;
	UCB1TXBUF	= value;
	LPM0;
	UCB1IE		&= ~UCRXIE;

	TDC1000_CS_DIS;
}

uint8_t TDC1000_SPIByteReadReg(uint8_t addr)
{
	uint8_t value;
	
	TDC1000_CS_EN;

	addr &= ~TDC1000_WRITE_BIT;
	
	value		= UCB1RXBUF;
	UCB1IE		|= UCRXIE;
	UCB1TXBUF	= addr;
	LPM0;
	value		= UCB1RXBUF;

	UCB1TXBUF	= 0;
	LPM0;
	value		= UCB1RXBUF;

	UCB1IE		&= ~UCRXIE;
	TDC1000_CS_DIS;
	
	return value;
}

void TDC7200_SPIByteWriteReg(uint8_t addr, uint8_t value)
{
	addr &= ~TDC7200_AUTOINC_ON_BIT;
	addr |= TDC7200_WRITE_BIT;
	
	TDC7200_CS_EN;
	
	UCB1IE	&= ~ (UCTXIE + UCRXIE);


	UCB1TXBUF	= addr;
	while(!(UCB1IFG & UCRXIFG));
	UCB1TXBUF	= value;
	while(!(UCB1IFG & UCRXIFG));

	TDC7200_CS_DIS;
}


uint8_t TDC7200_SPIByteReadReg(uint8_t addr)
{
	uint8_t value;
	
	addr &= ~(TDC7200_AUTOINC_ON_BIT + TDC7200_WRITE_BIT);
	
	TDC7200_CS_EN;
	
	UCB1IFG		&= ~UCRXIFG;
	UCB1IE		|= UCRXIE;
	UCB1TXBUF	= addr;
	LPM0;
	UCB1IFG		&= ~UCRXIFG;

	UCB1TXBUF	= 0;
	LPM0;
	value		= UCB1RXBUF;
	
	UCB1IE		&= ~UCRXIE;
	TDC7200_CS_DIS;

	return value;

}

void TDC7200_SPIAutoIncReadReg(uint8_t addr, uint8_t *buffer, uint8_t size)
{
	addr &= ~TDC7200_WRITE_BIT;
	addr |= TDC7200_AUTOINC_ON_BIT;
	
	TDC7200_CS_EN;
	
	UCB1IFG		&= ~UCRXIFG;
	UCB1IE		&= ~UCRXIE;
	UCB1IE		&= ~UCTXIE;

	UCB1TXBUF	= addr;

	while(!(UCB1IFG & UCRXIFG));

	do
	{
		UCB1TXBUF	= 0;
	while(!(UCB1IFG & UCTXIFG));
		*buffer++	= UCB1RXBUF;
	}
	while(--size);
	
	UCB1IE		&= ~UCRXIE;
	TDC7200_CS_DIS;
}

void TDC7200_SPIAutoIncWriteReg(uint8_t addr, uint8_t *buffer, uint8_t size)
{
	addr |= TDC7200_WRITE_BIT + TDC7200_AUTOINC_ON_BIT;
	
	TDC7200_CS_EN;
	
	UCB1IFG		&= ~UCRXIFG;
	UCB1IE		|= UCRXIE;
	UCB1TXBUF	= addr;
	LPM0;
	
	do
	{
		UCB1IFG		&= ~UCRXIFG;
		UCB1TXBUF	= *buffer++;
		LPM0;
	}
	while(--size);

	UCB1IE		&= ~UCRXIE;
	TDC7200_CS_DIS;
}

// Read 3 bytes long data
uint32_t TDC7200_SPILongReadReg(uint8_t addr)
{
	uint32_t	result = 0;
	uint8_t		size = 3;
	
	TDC7200_CS_EN;
	
	UCB1IFG		&= ~UCRXIFG;
	
	UCB1IE		|= UCRXIE;
	UCB1TXBUF	= addr;
	LPM0;
	UCB1IFG		&= ~UCRXIFG;
	
	do
	{
		UCB1TXBUF	= 0;
		LPM0;
		result		<<= 8;
		result		+= UCB1RXBUF;
	}
	while(--size);
	
	UCB1IE		&= ~UCRXIE;
	TDC7200_CS_DIS;
	
	return result;
}


#pragma vector=USCI_B1_VECTOR
__interrupt void USCI_B1_ISR(void)
{
	switch (__even_in_range(UCB1IV, USCI_SPI_UCTXIFG))
	{
		case USCI_SPI_UCRXIFG:		//Vector 2 - RXIFG
			LPM0_EXIT;
			break;
		case USCI_SPI_UCTXIFG:		//Vector 2 - TXIFG
			LPM0_EXIT;
			break;
		
		default: break;
	}
}
