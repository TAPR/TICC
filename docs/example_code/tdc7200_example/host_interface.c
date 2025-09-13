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

volatile uint8_t receiving_cmd = 0;
uint8_t cmdResponseString[MAX_STR_LENGTH];

uint8_t char2nibble(uint8_t db)
{
	if ((db >= '0') && (db <= '9'))
	return (db-'0');
	else if ((db >= 'A') && (db <= 'F'))
	return (db-'A'+0xa);
	else if ((db >= 'a') && (db <= 'f'))
	return (db-'a'+0xa);
	else
	return (db);
}

// Parse Command, then execute
uint8_t handleHostCommand(void)
{
	uint8_t		*pBuf = cmdResponseString;
	uint8_t		address;
	uint8_t		host_response = 0;
	uint16_t	word_data;
	uint8_t		nxt_byte, byte_data;
	uint32_t	long_data;
	uint8_t		double_byte[2];
	
	receiving_cmd |= BIT0;

	nxt_byte = char2nibble(pBuf[0]);
	nxt_byte = (nxt_byte << 4) + char2nibble(pBuf[1]);
	switch(nxt_byte)
	{
	case Command_LoopPacket:  
		{
			host_response = 1;
			break;
		}

	case Command_TDC1000_SPI_Byte_Write:
		{
			address		= (char2nibble(pBuf[2]) << 4) + char2nibble(pBuf[3]);
			byte_data	= (char2nibble(pBuf[4]) << 4) + char2nibble(pBuf[5]);

			TDC1000_SPIByteWriteReg(address, byte_data);
			host_response = 1;
			break;
		}
	case Command_TDC1000_SPI_Byte_Read:
		{
			address		= (char2nibble(pBuf[2]) << 4) + char2nibble(pBuf[3]);

			pBuf[8]		= TDC1000_SPIByteReadReg(address);
			host_response = 1;
			break;
		}
	case Command_Start_TOF_One_Shot:
		{
			tdc_state_flag |= TDC_SINGLE_SHOT_MEASURE_STATE;
			host_response = 1;
			receiving_cmd |= BIT2;
			break;
		}
	case Command_Start_TOF_Graph:
		{
			receiving_cmd &= ~BIT1;
			// low byte first
			word_data = char2nibble(pBuf[4]);
			word_data = (word_data <<4) + char2nibble(pBuf[5]);
			// high byte next
			word_data = (word_data <<4) + char2nibble(pBuf[2]);
			word_data = (word_data <<4) + char2nibble(pBuf[3]);
			if (word_data != 0)
			{
				tdc_state_flag |= TDC_INTERLEAVED_TEMP_MEASURE;
				count_per_temp = word_data;
			}
			else
			{
				tdc_state_flag &= ~TDC_INTERLEAVED_TEMP_MEASURE;
				count_per_temp = 0;
			}
			count_meassure_temp = count_per_temp;
			
			tdc_state_flag |= TDC_TOF_GRAPTH_STATE;


			reset_uart();
			reset_interval_timer();


			host_response = 1;
			break;
		}
	case Command_End_TOF_Graph:
		{
			receiving_cmd |= BIT1;
			tdc_state_flag &= ~(TDC_TOF_GRAPTH_STATE + TDC_INTERLEAVED_TEMP_MEASURE);
			TDC1000_LINK_LED_PxOUT |= TDC1000_LINK_LED_PIN;                        // Turn on LED
			TDC1000_MEAS_LED_PxOUT &= ~TDC1000_MEAS_LED_PIN;
			host_response = 1;
			break;
		}
	case Command_Start_Continuous_Trigger:
		{
			tdc_state_flag |= TDC_CONTINUOUS_TRIGGER_STATE;
			host_response = 1;
			break;
		}
	case Command_Stop_Continuous_Trigger:
		{
			tdc_state_flag &= ~TDC_CONTINUOUS_TRIGGER_STATE;
			TDC1000_LINK_LED_PxOUT |= TDC1000_LINK_LED_PIN;                        // Turn on LED
			TDC1000_MEAS_LED_PxOUT &= ~TDC1000_MEAS_LED_PIN;
			host_response = 1;
			break;
		}
	case Command_Firmware_Version_Read:
		{
			pBuf[8] = Firmware_VersionA;
			pBuf[9] = Firmware_VersionB;
			pBuf[10] = Firmware_VersionC;
			pBuf[11] = Firmware_VersionD;
			host_response = 1;
			break;
		}
	case Command_LED_Toggle:
		{
			// toggle LED
			TDC1000_LINK_LED_PxOUT ^= TDC1000_LINK_LED_PIN;
			host_response = 1;
			break;
		}
	case Command_MSP430SPI_Config_Read:
		{
			pBuf[8] = UCB1CTL0 & 0xC0;
			pBuf[9] = UCB1BR0;
			pBuf[10] = UCB1BR1;
			host_response = 1;
			break;
		}
	case Command_MSP430SPI_Config_Write:
		{
			byte_data	= (char2nibble(pBuf[2]) << 4) + char2nibble(pBuf[3]);
			byte_data	&= 0xC0;												// make sure only top 2 bits are valid
			
			// low byte first
			word_data	= (char2nibble(pBuf[6]) << 4) + char2nibble(pBuf[7]);
			// high byte next
			word_data = (word_data <<4) + char2nibble(pBuf[4]);
			word_data = (word_data <<4) + char2nibble(pBuf[5]);
			
			if (word_data) word_data--;
			
			UCB1CTL1	|= UCSWRST;												// **Disable USCI state machine**
			UCB1CTL0	&= 0x3F;												// update
			UCB1CTL0	|= byte_data;
			UCB1BRW		= word_data;											// UCLK divider;
			UCB1CTL1	&= ~UCSWRST;											// **Initialize USCI state machine**
			
			host_response = 1;
			break;
		}
	case Command_TDC7200_SPI_Byte_Write:
		{                              
			address		= (char2nibble(pBuf[2]) << 4) + char2nibble(pBuf[3]);
			byte_data	= (char2nibble(pBuf[4]) <<4) + char2nibble(pBuf[5]);
			
			TDC7200_SPIByteWriteReg(address, byte_data);
			host_response = 1;
			break;
		}
	case Command_TDC7200_SPI_Byte_Read:
		{
			address		= (char2nibble(pBuf[2]) << 4) + char2nibble(pBuf[3]);
			
			if ((tdc_state_flag & TDC_POWER_CYCLE_FLAG)
				&& (address <= TDC7200_CLOCK_COUNTER_STOP_MASKL_REG)
				)
			{
				pBuf[8]= TDC7200_reg_local_copy[address];
			}
			else
			{
				pBuf[8] = TDC7200_SPIByteReadReg(address);
			}
			host_response = 1;
			break;
		}
	case Command_TDC7200_SPI_Word_Read:
		{
			address		= (char2nibble(pBuf[2]) << 4) + char2nibble(pBuf[3]);

			long_data = TDC7200_SPILongReadReg(address);
			pBuf[8] = long_data;
			pBuf[9] = long_data >> 8;
			pBuf[10] = long_data >> 16;      
			host_response = 1;
			break;
		}
	case Command_TDC7200_Status_Write:
		{
			byte_data = (char2nibble(pBuf[2]) <<4) + char2nibble(pBuf[3]);
			if (byte_data)
			{
				tdc_enable_clock();
				TDC7200_ENABLE_PxOUT		|= TDC7200_ENABLE_PIN;					// Enable device
				tdc_state_flag |= TDC_TDC7200_ENABLED;
			}
			else
			{
				tdc_disable_clock();
				TDC7200_ENABLE_PxOUT		&= ~TDC7200_ENABLE_PIN;					// Disable device
				tdc_state_flag &= ~TDC_TDC7200_ENABLED;
			}
		}
	case Command_TDC7200_Status_Read:
		{
			pBuf[8] = (tdc_state_flag & TDC_TDC7200_ENABLED) ? 1 : 0;
			host_response = 1;
			break;
		}

	case Command_Set_Timer_Trigger_Freq:
		{                              
			// low byte first
			word_data = char2nibble(pBuf[4]);
			word_data = (word_data <<4) + char2nibble(pBuf[5]);
			// high byte next
			word_data = (word_data <<4) + char2nibble(pBuf[2]);
			word_data = (word_data <<4) + char2nibble(pBuf[3]); 

			timer_trigger_freq = word_data;
			
			host_response = 1;
			break;
		}

	case Command_Read_Timer_Trigger_Freq:
		{
			pBuf[8] = timer_trigger_freq;
			pBuf[9] = timer_trigger_freq >> 8;
			host_response = 1;
			break;
		}

	case Command_Reset_TDC7200:
		{
			tdc7200_reset();
			host_response = 1;
			break;
		}
	case Command_Reset_TDC1000:
		{
			tdc1000_reset();
			host_response = 1;
			break;
		}
	case Command_TDC7200_Set_Double_Resolution:
		{
			address = 0x0b;
			
			double_byte[0] = 0;
			double_byte[1] = 0x01;
			
			TDC7200_SPIAutoIncWriteReg(address, double_byte, 2); 
			
			tdc_state_flag |= TDC_DOUBLE_RESOLUTION_FLAG;
			
			host_response = 1;
			break;
		}
	case Command_TDC7200_Clear_Double_Resolution:
		{
			uint8_t reg_address;
			reg_address = 0x0b;
			
			double_byte[0] = 0;
			double_byte[1] = 0;
			
			TDC7200_SPIAutoIncWriteReg(reg_address, double_byte, 2);

			tdc_state_flag &= ~TDC_DOUBLE_RESOLUTION_FLAG;      
			
			host_response = 1;
			break;
		}
	case Command_TDCxxxx_Disable_Power_Cycle:
		{
			tdc_state_flag &= ~TDC_POWER_CYCLE_FLAG;
			tdc_power_cycle_on();

			host_response = 1;      
			break;
		}
	case Command_TDCxxx_Enable_Power_Cycle:
		{
			tdc_state_flag |= TDC_POWER_CYCLE_FLAG;
			tdc_power_cycle_off();
			
			host_response = 1;
			break;
		}
		
		// Not implemented. Dummy cases.
	case Command_ReInit: 
	case Command_Enter_MSP430_BSL:
	case Command_Read_Xclk_Period:
	case Command_Set_Xclk_Period:
	case Command_Read_ExtOSC_Wakeup_Period:
	case Command_Set_ExtOSC_Wakeup_Delay:
	case Command_Enable_CPU_Clock:												// Enable SMCLK Output
	case Command_Disable_CPU_Clock:												// Disable SMCLK Output
	case Command_Enable_HV_DRIVER_EN1:
	case Command_Disable_HV_DRIVER_EN1:
	case Command_Enable_HV_DRIVER_EN2:
	case Command_Disable_HV_DRIVER_EN2:
		{     
			host_response = 1;
			break;
		}    
	default:
		{
			host_response = 1;
			break;
		}    
	}
	return (host_response);
}


void reset_uart(void)
{
	unsigned int i;

	UART_Init();
	P4SEL0		|= BIT2 + BIT3;								// Enable UART
	UCA0CTL1	&= ~UCSWRST;								// when measuring system current consumption,
	UCA0IFG		&= ~UCRXIFG;								// un-define "GUI" and remove the connection of UART
	UCA0IE		|= UCRXIE;

	uart_cnt = 0;

	for (i = 0; i < MAX_STR_LENGTH; i++)
				{
					cmdResponseString[i] = 0x00;
				}
}

void reset_interval_timer(void)
{
	pause_interval_timer();
	interval_timer_init();
	action_items &= ~HANDLE_NEXT_TRIGGER;
}
