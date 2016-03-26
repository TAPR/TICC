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

#ifndef TDC_7200_SPI_H
#define TDC_7200_SPI_H

#define TDC7200_CS_EN	TDC7200_CSn_PxOUT &= ~TDC7200_CSn_PIN
#define TDC7200_CS_DIS	TDC7200_CSn_PxOUT |= TDC7200_CSn_PIN

/************************************************************
* TDC7200 REGISTER SET ADDRESSES
************************************************************/

#define TDC7200_CONFIG1_REG						0x00
#define TDC7200_CONFIG2_REG						0x01
#define TDC7200_INTRPT_STATUS_REG				0x02
#define TDC7200_INTRPT_MASK_REG					0x03
#define TDC7200_COARSE_COUNTER_OVH_REG			0x04
#define TDC7200_COARSE_COUNTER_OVL_REG			0x05
#define TDC7200_CLOCK_COUNTER_OVH_REG			0x06
#define TDC7200_CLOCK_COUNTER_OVL_REG			0x07
#define TDC7200_CLOCK_COUNTER_STOP_MASKH_REG	0x08
#define TDC7200_CLOCK_COUNTER_STOP_MASKL_REG	0x09

#define TDC7200_TIME1_REG						0x10
#define TDC7200_CLOCK_COUNT1_REG				0x11
#define TDC7200_TIME2_REG						0x12
#define TDC7200_CLOCK_COUNT2_REG				0x13
#define TDC7200_TIME3_REG						0x14
#define TDC7200_CLOCK_COUNT3_REG				0x15
#define TDC7200_TIME4_REG						0x16
#define TDC7200_CLOCK_COUNT4_REG				0x17
#define TDC7200_TIME5_REG						0x18
#define TDC7200_CLOCK_COUNT5_REG				0x19
#define TDC7200_TIME6_REG						0x1A
#define TDC7200_CALIBRATION1_REG				0x1B
#define TDC7200_CALIBRATION2_REG				0x1C

// Useful definitions
#define TDC7200_WRITE_BIT						0x40
#define TDC7200_AUTOINC_ON_BIT					0x80

#define TDC7200_TOTAL_NUM_CONFIG_REG			10
#define TDC7200_TOTAL_NUM_RESULT_REG			39

/*******************************/
/*** TDC7200 Config-1 (0x00) ***/
/*******************************/

#define TDC7200_START_NEW_MEASUREMENT			0x01
#define TDC7200_MEASUREMENT_MODE0				0x02
#define TDC7200_MEASUREMENT_MODE1				0x04
#define TDC7200_START_EDGE_POLARITY				0x08
#define TDC7200_STOP_EDGE_POLARITY				0x10
#define TDC7200_TRIGGER_EDGE_POLARITY			0x20
#define TDC7200_PARITY_EN						0x40
#define TDC7200_FORCE_CALIBRATION				0x80

#define TDC7200_MEASUREMENT_MODE__1				0x00
#define TDC7200_MEASUREMENT_MODE__2				TDC7200_MEASUREMENT_MODE0

#define TDC7200_START_EDGE_POLARITY__RISING		0x00
#define TDC7200_START_EDGE_POLARITY__FALLING	TDC7200_START_EDGE_POLARITY

#define TDC7200_STOP_EDGE_POLARITY__RISING		0x00
#define TDC7200_STOP_EDGE_POLARITY__FALLING		TDC7200_STOP_EDGE_POLARITY

#define TDC7200_TRIGGER_EDGE_POLARITY__RISING	0x00
#define TDC7200_TRIGGER_EDGE_POLARITY__FALLING	TDC7200_TRIGGER_EDGE_POLARITY

/*******************************/
/*** TDC7200 Config-2 (0x01) ***/
/*******************************/

#define TDC7200_NUM_OF_STOPS0					0x01
#define TDC7200_NUM_OF_STOPS1					0x02
#define TDC7200_NUM_OF_STOPS2					0x04
#define TDC7200_AVG_CYCLES0						0x08
#define TDC7200_AVG_CYCLES1						0x10
#define TDC7200_AVG_CYCLES2						0x20
#define TDC7200_CAL2_PERIODS0					0x40
#define TDC7200_CAL2_PERIODS1					0x80

#define TDC7200_NUM_OF_STOPS_0					0x00
#define TDC7200_NUM_OF_STOPS_1					TDC7200_NUM_OF_STOPS0
#define TDC7200_NUM_OF_STOPS_2					TDC7200_NUM_OF_STOPS1
#define TDC7200_NUM_OF_STOPS_3					TDC7200_NUM_OF_STOPS1 + TDC7200_NUM_OF_STOPS0
#define TDC7200_NUM_OF_STOPS_4					TDC7200_NUM_OF_STOPS2
// Reserved
// #define TDC7200_NUM_OF_STOPS_5					TDC7200_NUM_OF_STOPS2 + TDC7200_NUM_OF_STOPS0
// #define TDC7200_NUM_OF_STOPS_6					TDC7200_NUM_OF_STOPS2 + TDC7200_NUM_OF_STOPS1
// #define TDC7200_NUM_OF_STOPS_7					TDC7200_NUM_OF_STOPS2 + TDC7200_NUM_OF_STOPS1 + TDC7200_NUM_OF_STOPS0

#define TDC7200_AVG_CYCLES_0					0x00
#define TDC7200_AVG_CYCLES_1					TDC7200_AVG_CYCLES0
#define TDC7200_AVG_CYCLES_2					TDC7200_AVG_CYCLES1
#define TDC7200_AVG_CYCLES_3					TDC7200_AVG_CYCLES1 + TDC7200_AVG_CYCLES0
#define TDC7200_AVG_CYCLES_4					TDC7200_AVG_CYCLES2
#define TDC7200_AVG_CYCLES_5					TDC7200_AVG_CYCLES2 + TDC7200_AVG_CYCLES0
#define TDC7200_AVG_CYCLES_6					TDC7200_AVG_CYCLES2 + TDC7200_AVG_CYCLES1
#define TDC7200_AVG_CYCLES_7					TDC7200_AVG_CYCLES2 + TDC7200_AVG_CYCLES1 + TDC7200_AVG_CYCLES0

#define TDC7200_CAL2_PERIODS_0					0x00
#define TDC7200_CAL2_PERIODS_1					TDC7200_CAL2_PERIODS0
#define TDC7200_CAL2_PERIODS_2					TDC7200_CAL2_PERIODS1
#define TDC7200_CAL2_PERIODS_3					TDC7200_CAL2_PERIODS1 + TDC7200_CAL2_PERIODS0

#define TDC7200_NUM_OF_STOPS__1					TDC7200_NUM_OF_STOPS_0
#define TDC7200_NUM_OF_STOPS__2					TDC7200_NUM_OF_STOPS_1
#define TDC7200_NUM_OF_STOPS__3					TDC7200_NUM_OF_STOPS_2
#define TDC7200_NUM_OF_STOPS__4					TDC7200_NUM_OF_STOPS_3
#define TDC7200_NUM_OF_STOPS__5					TDC7200_NUM_OF_STOPS_4

#define TDC7200_AVG_CYCLES__1					TDC7200_AVG_CYCLES_0
#define TDC7200_AVG_CYCLES__2					TDC7200_AVG_CYCLES_1
#define TDC7200_AVG_CYCLES__4					TDC7200_AVG_CYCLES_2
#define TDC7200_AVG_CYCLES__8					TDC7200_AVG_CYCLES_3
#define TDC7200_AVG_CYCLES__16					TDC7200_AVG_CYCLES_4
#define TDC7200_AVG_CYCLES__32					TDC7200_AVG_CYCLES_5
#define TDC7200_AVG_CYCLES__64					TDC7200_AVG_CYCLES_6
#define TDC7200_AVG_CYCLES__128					TDC7200_AVG_CYCLES_7

#define TDC7200_CAL2_PERIODS__2					TDC7200_CAL2_PERIODS_0
#define TDC7200_CAL2_PERIODS__10				TDC7200_CAL2_PERIODS_1
#define TDC7200_CAL2_PERIODS__20				TDC7200_CAL2_PERIODS_2
#define TDC7200_CAL2_PERIODS__40				TDC7200_CAL2_PERIODS_3

/***************************************/
/*** TDC7200 Interrupt Status (0x02) ***/
/***************************************/

#define TDC7200_NEW_MEAS_INT					0x01
#define TDC7200_COARSE_CNT_OF_INT				0x02
#define TDC7200_CLOCK_CNT_OF_INT				0x04
#define TDC7200_MEAS_STARTED_FLAG				0x08
#define TDC7200_MEAS_COMPLETED_FLAG				0x10
// Reserved										0x20
// Reserved										0x40
// Reserved										0x80

/************************************/
/*** TDC7200 Mask Register (0x03) ***/
/************************************/

#define TDC7200_NEW_MEAS_MASK					0x01
#define TDC7200_COARSE_CNT_OF_MASK				0x02
#define TDC7200_CLOCK_CNT_OF_MASK				0x04
// Reserved										0x08
// Reserved										0x10
// Reserved										0x20
// Reserved										0x40
// Reserved										0x80

void		TDC_SPISetup(void);

void		TDC7200_SPIByteWriteReg(uint8_t, uint8_t);
uint8_t		TDC7200_SPIByteReadReg(uint8_t);

void		TDC7200_SPIAutoIncWriteReg(uint8_t, uint8_t *, uint8_t);
void		TDC7200_SPIAutoIncReadReg(uint8_t, uint8_t *, uint8_t);

uint32_t	TDC7200_SPILongReadReg(uint8_t);
#endif
