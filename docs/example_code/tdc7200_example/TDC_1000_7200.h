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

#ifndef TDC_1000_7200_H
#define TDC_1000_7200_H

#define USE_OSC

#define TDC7200_WAKEUP_PERIOD			400

#define TOF_MEASUREMENT					0
#define TMP_MEASUREMENT					1

#define TDC_CONTINUOUS_TRIGGER_STATE	0x0001
#define TDC_TOF_GRAPTH_STATE			0x0002
#define TDC_SINGLE_SHOT_MEASURE_STATE	0x0004
#define TDC_POWER_CYCLE_FLAG			0x0008
#define TDC_DOUBLE_RESOLUTION_FLAG		0x0010
#define TDC_UP_STREAM_BUFFER_READY		0x0020
#define TDC_DOWN_STREAM_BUFFER_READY	0x0040
#define TDC_RTD_BUFFER_READY			0x0080
#define TDC_INTERLEAVED_TEMP_MEASURE	0x0100
#define TDC_TDC7200_ENABLED				0x0200

#define TDC_DEFAULT_FLAG				0x0000

#define TDC7200_ALL_DATA_SIZE	39
#define NUM_SAMPLES				1						// number of samples per block
#define SAMPLE_SIZE				TDC7200_ALL_DATA_SIZE 
#define NUM_BLOCKS				1  

#define BUF_LENGTH				(NUM_SAMPLES * SAMPLE_SIZE * NUM_BLOCKS) + 1

#define MULTI_CYCLE 2

// Following are TDC1000 operation modes
#define REGULAR_TOF		0
#define REGULAR_TMP		1
#define AUTO_FLOW		2
#define MANUAL_FLOW		3

#define ENABLED			1
#define DISABLED		0

extern uint8_t TDC7200_reg_local_copy[10];
extern uint8_t TDC1000_reg_local_copy[10];

extern uint8_t upStreamBuf[BUF_LENGTH];
extern uint8_t downStreamBuf[BUF_LENGTH];
extern uint8_t RTDBuf[BUF_LENGTH];

extern volatile uint16_t tdc_state_flag;
extern volatile uint16_t count_per_temp;
extern volatile uint16_t count_meassure_temp;

void Init_TDC1000_TDC7200(void);
void tdc1000_reset(void);
void tdc7200_reset(void);
void tdc_enable_clock(void);
void tdc_disable_clock(void);
void tdc_trigger_measure(void);
void TDC1000_reg_init(void);
void TDC7200_reg_init(void);
void tdc_power_cycle_on(void);
void tdc_power_cycle_off(void);

#endif
