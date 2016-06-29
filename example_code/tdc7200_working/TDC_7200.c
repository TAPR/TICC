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

uint8_t TDC7200_reg_local_copy[10] = {0x02, 0x44, 0x07, 0x07, 0xFF, 0xFF, 0xFF, 0xFF, 0x0, 0x0 };

uint8_t upStreamBuf[BUF_LENGTH];
uint8_t downStreamBuf[BUF_LENGTH];
uint8_t RTDBuf[BUF_LENGTH];


volatile uint8_t MEASURE_MODE;
volatile uint16_t tdc_state_flag = TDC_DEFAULT_FLAG;
volatile uint16_t count_per_temp = 0;
volatile uint16_t count_meassure_temp = 0;


void Init_TDC7200(void)
{
	UCB1CTLW0 &= ~UCSWRST;								// Switch on SPI module
	TDC7200_ENABLE_PxOUT |= TDC7200_ENABLE_PIN;			// Enable TDC7200
	delay_uS(TDC7200_WAKEUP_PERIOD);				    // Reset TDC7200


	TDC7200_reg_init();									// init registers of TDC7200

	UCB1CTLW0 |= UCSWRST;								// Switch oFF SPI module
	TDC7200_CS_EN;
	TDC7200_ENABLE_PxOUT &= ~TDC7200_ENABLE_PIN;		// Enable TDC7200
}

//******************************************************************************
void TDC7200_reg_init(void)
{
	TDC7200_SPIAutoIncWriteReg(TDC7200_CONFIG1_REG, TDC7200_reg_local_copy, TDC7200_TOTAL_NUM_CONFIG_REG);
}

void tdc7200_reset(void)
{
	TDC7200_ENABLE_PxOUT &= ~TDC7200_ENABLE_PIN;
	tdc_disable_clock();
	// wait for 1sec for the cap to discharge
	delay_ACLK(24576);
	TDC7200_ENABLE_PxOUT |= TDC7200_ENABLE_PIN;
	// give at least 500us
	delay_uS(500);
	tdc_enable_clock();
	
	// Unset "Start New Measurement" bit to avoid measurement being triggered
	TDC7200_reg_local_copy[TDC7200_CONFIG1_REG] &= ~0x01;
	TDC7200_SPIAutoIncWriteReg(TDC7200_CONFIG1_REG, TDC7200_reg_local_copy, TDC7200_TOTAL_NUM_CONFIG_REG);
}


void tdc_power_cycle_on(void)
{
	

	TDC7200_ENABLE_PxOUT |= TDC7200_ENABLE_PIN;					// Enable device
	delay_uS(TDC7200_WAKEUP_PERIOD);							// wait for TDC7200 wakeup delay

	// Unset "Start New Measurement" bit to avoid measurement being triggered
	TDC7200_reg_local_copy[TDC7200_CONFIG1_REG] &= ~0x01;
	
	// init TDC7200 with local saved data
	TDC7200_SPIAutoIncWriteReg(TDC7200_CONFIG1_REG, TDC7200_reg_local_copy, TDC7200_TOTAL_NUM_CONFIG_REG);
}

void tdc_power_cycle_off(void)
{
	// Backup register values before disable
	TDC7200_SPIAutoIncReadReg(TDC7200_CONFIG1_REG, TDC7200_reg_local_copy, TDC7200_TOTAL_NUM_CONFIG_REG);

	// Disable device
	TDC7200_ENABLE_PxOUT &= ~TDC7200_ENABLE_PIN;    
	
}

void tdc_trigger_common(uint8_t *buf)
{
	uint8_t byte_data;
	
	byte_data = TDC7200_reg_local_copy[0];

	// set start measurement bit & use default mode or set by user
	byte_data |= 0x01;
	TDC7200_SPIByteWriteReg(TDC7200_CONFIG1_REG, byte_data);
		
	start_ms_timeout(500);
	
	//wait for INTB pin to go low
	TDC7200_INTB_PxIFG &= ~TDC7200_INTB_PIN;
	TDC7200_INTB_PxIE |= TDC7200_INTB_PIN;
	LPM3;
	TDC7200_INTB_PxIE &= ~TDC7200_INTB_PIN;
	
	stop_ms_timeout();

	if (timeout)
	{
		__no_operation();
	}
	
	// Read all the result registers
	TDC7200_SPIAutoIncReadReg(TDC7200_TIME1_REG, buf, TDC7200_TOTAL_NUM_RESULT_REG);
	buf[39] = 0x00;

}

//******************************************************************************
// N8UR NOTE: This starts to get specific about the flow rate example.
// Much we don't care about here!
void tdc_trigger_measure(void)
{
	uint8_t temp1, temp2;
	
	if (tdc_state_flag & (TDC_CONTINUOUS_TRIGGER_STATE + TDC_TOF_GRAPTH_STATE + TDC_SINGLE_SHOT_MEASURE_STATE))
	{
		// Perform power cycle if enabled
		if (tdc_state_flag & TDC_POWER_CYCLE_FLAG)
		{
			tdc_power_cycle_on();
		}
		
		// Clear error flags and reset state machine
		TDC7200_SPIByteWriteReg(TDC7200_INTRPT_STATUS_REG, 0x1F);      

		// Measure for up stream

		tdc_trigger_common(upStreamBuf);


		if (tdc_state_flag & (TDC_TOF_GRAPTH_STATE + TDC_SINGLE_SHOT_MEASURE_STATE))
		{      
			tdc_state_flag |= TDC_UP_STREAM_BUFFER_READY;
		}
		
		
				// config TDC7200 for 5 stops
				// clear last 6 bits: both num stops and avgng cycles
				temp2 = TDC7200_SPIByteReadReg(TDC7200_CONFIG2_REG);
				TDC7200_SPIByteWriteReg(TDC7200_CONFIG2_REG, (temp2 & 0xC0) | 0x04);
				
				tdc_trigger_common(RTDBuf);
				RTDBuf[39] = 0xA5;

				// Restore register
				TDC7200_SPIByteWriteReg(TDC7200_CONFIG2_REG, temp2);
				
				count_meassure_temp = count_per_temp;
				tdc_state_flag |= TDC_RTD_BUFFER_READY;
			}
		}
		
		if (tdc_state_flag & TDC_POWER_CYCLE_FLAG)
		{
			tdc_power_cycle_off();
		}
	}
}
