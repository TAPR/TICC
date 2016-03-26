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

#include "global.h"

#ifndef HOST_INTERFACE_H
#define HOST_INTERFACE_H

#define Firmware_VersionA 0
#define Firmware_VersionB 1									// Major Rev: A + B
#define Firmware_VersionC 0
#define Firmware_VersionD 27								// Minor Rev: C + D

#define MAX_STR_LENGTH 32

extern volatile uint8_t receiving_cmd;
extern uint8_t cmdResponseString[MAX_STR_LENGTH];

uint8_t handleHostCommand(void);
void reset_uart(void);
void reset_interval_timer(void);

//******************************************************************************
//Command and Packet definition
// 

#define Command_LoopPacket 0							// Loop back entire packet
														// PacketBuffer[0],[1] Command

#define Command_ReInit 1								// Controls the firmware settings
														// PacketBuffer[0],[1] Command
														// PacketBuffer[1] Look for external trigger or not in continuous sampling 
														// PacketBuffer[2]     													                                            	                                                	

#define Command_TDC1000_SPI_Byte_Write 2				// Writes data to SPI port
														// PacketBuffer[0],[1] Command
														// PacketBuffer[1],[2] SPI Address Byte											    
														// PacketBuffer[3],[4] SPI Write Data Byte

#define Command_TDC1000_SPI_Byte_Read 3					// Read byte data from SPI port 
														// PacketBuffer[0],[1] Command
														// PacketBuffer[1],[2] SPI Address Byte
														// PacketBuffer[8] Response: SPI Data Byte Read from device

#define Command_Start_Continuous_Trigger 4				// start continuous trigger (no c2000 measurement data response)
														// PacketBuffer[0],[1] Command
														//
														// PacketBuffer[8] Response:

#define Command_Start_TOF_One_Shot 5					//Start TOF command
														// PacketBuffer[0],[1] Command
														// PacketBuffer[2],[3] Expected num of stop pulses
										
														// PacketBuffer[8] Response: start-stop1 data low byte 
														// PacketBuffer[9] Response: start-stop1 data high byte
														// PacketBuffer[10] Response: start-stop2 data low byte (only if numstop >= 2)
														// PacketBuffer[11] Response: start-stop2 data high byte
														// PacketBuffer[12] Response: start-stop3 data low byte (only if numch == 3)
														// PacketBuffer[13] Response: start-stop3 data high byte


#define Command_Start_TOF_Graph 6						// Start tof graph
														// PacketBuffer[0],[1] Command
														//

														// PacketBuffer[8] Response:


#define Command_End_TOF_Graph 7							// stop tof graph
														// PacketBuffer[0],[1] Command
														//
														// PacketBuffer[8] Response:

#define Command_Stop_Continuous_Trigger 8				// stop continuous trigger
														// PacketBuffer[0],[1] Command
														//
														// PacketBuffer[8] Response: 

#define Command_Firmware_Version_Read 9					// Read firmware version 
														// PacketBuffer[0],[1] Command
														// PacketBuffer[8] Response: Firmware VersionA 
														// PacketBuffer[9] Response: Firmware VersionB
														// PacketBuffer[10] Response: Firmware VersionC 
														// PacketBuffer[11] Response: Firmware VersionD

#define Command_LED_Toggle 0x0A							// Toggle LED
														// PacketBuffer[0],[1] Command

#define Command_MSP430SPI_Config_Read 0x0B				// Read MSP430 SPI config info 
														// PacketBuffer[0],[1] Command                            
														// PacketBuffer[8] Response: SPI Clock Config   
														// PacketBuffer[9] Response: SPI clock divider (low byte)
														// PacketBuffer[10] Response: SPI clock divider (high byte)

#define Command_MSP430SPI_Config_Write 0x0C				// Write MSP430 SPI config info 
														// PacketBuffer[0],[1] Command
														// PacketBuffer[2],[3] SPI Clock Config
														// PacketBuffer[4],[5] SPI Clock divider low byte
														// PacketBuffer[6],[7] SPI Clock divider high byte

#define Command_TDC7200_SPI_Byte_Write 0x12				// Writes data to SPI port
														// PacketBuffer[0],[1] Command
														// PacketBuffer[1],[2] SPI Address Byte											    
														// PacketBuffer[3],[4] SPI Write Data Byte


#define Command_TDC7200_SPI_Byte_Read 0x13				// Read byte data from SPI port 
														// PacketBuffer[0],[1] Command
														// PacketBuffer[1],[2] SPI Address Byte
														// PacketBuffer[8] Response: SPI Data Byte Read from device

#define Command_TDC7200_SPI_Word_Read 0x14				// Read word data from SPI port 
														// PacketBuffer[0],[1] Command
														// PacketBuffer[1],[2] SPI Address Byte
														// PacketBuffer[8] Response: SPI Data word (low byte) 
														// PacketBuffer[9] Response: SPI Data word (mid byte)
														// PacketBuffer[9] Response: SPI Data word (high byte)

#define Command_TDC7200_Status_Read 0x15				// Controls the DUT Enable/Disable status
														// PacketBuffer[0],[1] Command    													                                            	                                                	
														// PacketBuffer[8] Response: Status

#define Command_TDC7200_Status_Write 0x16				// Controls the DUT Enable/Disable status
														// PacketBuffer[0],[1] Command    													                                            	                                                	
														// PacketBuffer[2],[3] Status
														// PacketBuffer [8] Response: Error Code

#define Command_Set_ExtOSC_Wakeup_Delay 0x17			// PacketBuffer[0],[1] Command
														// PacketBuffer[2],[3] wakeup delay low byte
														// PacketBuffer[4],[5] wakeup delay high byte

#define Command_Set_Timer_Trigger_Freq 0x18				// PacketBuffer[0],[1] Command
														// PacketBuffer[2],[3] trigger freq low byte
														// PacketBuffer[4],[5] trigger freq high byte

#define Command_Set_Xclk_Period 0x19					// PacketBuffer[0],[1] Command
														// PacketBuffer[2],[3] xclk period byte

#define Command_Read_Xclk_Period 0x1A					// PacketBuffer[0],[1] Command
														// PacketBuffer[8] xclk period byte

#define Command_Read_Timer_Trigger_Freq 0x1B			// PacketBuffer[0],[1] Command
														// PacketBuffer[8],[9] Timer Trigger Freq byte

#define Command_Read_ExtOSC_Wakeup_Period 0x1C			// PacketBuffer[0],[1] Command
														// PacketBuffer[9],[10] wakeup period byte

#define Command_Enter_MSP430_BSL 0x1D					// PacketBuffer[0],[1] Command

#define Command_Reset_TDC7200 0x1E						// PacketBuffer[0],[1] Command

#define Command_Reset_TDC1000 0x1F						// PacketBuffer[0],[1] Command

#define Command_TDC7200_Set_Double_Resolution 0x20		// PacketBuffer[0],[1] Command

#define Command_TDC7200_Clear_Double_Resolution 0x21	// PacketBuffer[0],[1] Command

#define Command_TDCxxxx_Disable_Power_Cycle 0x22		// PacketBuffer[0],[1] Command

#define Command_TDCxxx_Enable_Power_Cycle 0x23			// PacketBuffer[0],[1] Command

#define Command_Enable_CPU_Clock 0x24					// PacketBuffer[0],[1] Command

#define Command_Disable_CPU_Clock 0x25					// PacketBuffer[0],[1] Command

#define Command_Enable_HV_DRIVER_EN1 0x26				// PacketBuffer[0],[1] Command
														// PacketBuffer[2],[3] Enable Period in us

#define Command_Disable_HV_DRIVER_EN1 0x27				// PacketBuffer[0],[1] Command

#define Command_Enable_HV_DRIVER_EN2 0x28				// PacketBuffer[0],[1] Command
														// PacketBuffer[2],[3] Enable Period in us

#define Command_Disable_HV_DRIVER_EN2 0x29
#endif
