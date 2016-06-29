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

#ifndef MSP430_HARDWARE_H
#define MSP430_HARDWARE_H

void init_board(void);
void Init_Clock(void);
void Init_Power(void);
void Init_Ports(void);

// ******** DCDC Power ******** //
#define VSEL_PxDIR						P9DIR
#define VSEL_PxOUT						P9OUT
#define VSEL1							BIT0
#define VSEL2							BIT1
#define VSEL3							BIT2
#define VSEL4							BIT3

#define DCDC_1_8V						0x00
#define DCDC_1_9V						VSEL1
#define DCDC_2_0V						VSEL2
#define DCDC_2_1V						VSEL2 + VSEL1
#define DCDC_2_2V						VSEL3
#define DCDC_2_3V						VSEL3 + VSEL1
#define DCDC_2_4V						VSEL3 + VSEL2
#define DCDC_2_5V						VSEL3 + VSEL2 + VSEL1
#define DCDC_2_6V						VSEL4
#define DCDC_2_7V						VSEL4 + VSEL1
#define DCDC_2_8V						VSEL4 + VSEL2
#define DCDC_2_9V						VSEL4 + VSEL2 + VSEL1
#define DCDC_3_0V						VSEL4 + VSEL3
#define DCDC_3_1V						VSEL4 + VSEL3 + VSEL1
#define DCDC_3_2V						VSEL4 + VSEL3 + VSEL2
#define DCDC_3_3V						VSEL4 + VSEL3 + VSEL2 + VSEL1
void set_dcdc_voltage(uint8_t voltage);

// ******** Power for RF supercap ******** //
#define CHARGE_PxDIR					P9DIR
#define CHARGE_PxOUT					P9OUT
#define FAST_CHARGE_PIN					BIT7
#define SLOW_CHARGE_PIN					BIT6

// Charge mode
#define CHARGE_MODE_FAST				FAST_CHARGE_PIN
#define CHARGE_MODE_SLOW				SLOW_CHARGE_PIN
#define CHARGE_MODE_NONE				0x00
void charge_mode(uint8_t charge_mode);

// Power for TDC1000 VDD and OPAMP circuits
#define VDD_ENABLE_PxDIR				P1DIR
#define VDD_ENABLE_PxOUT				P1OUT
#define VDD_ENABLE_PIN					BIT4
#define VDD_ENABLE						VDD_ENABLE_PxOUT |= VDD_ENABLE_PIN
#define VDD_DISABLE						VDD_ENABLE_PxOUT &= ~VDD_ENABLE_PIN

// Power for TDC chips and external 5V output
#define LOAD_ENABLE_PxDIR				P9DIR
#define LOAD_ENABLE_PxOUT				P9OUT
#define LOAD_ENABLE_PIN					BIT4
#define LOAD_ENABLE						LOAD_ENABLE_PxOUT |= LOAD_ENABLE_PIN
#define LOAD_DISABLE					LOAD_ENABLE_PxOUT &= ~LOAD_ENABLE_PIN

// Power for RF power amplifier
#define PA_POWER_PxDIR					P9DIR
#define PA_POWER_PxOUT					P9OUT
#define PA_POWER_PIN					BIT5
#define PA_POWER_ENABLE					PA_POWER_PxOUT |= PA_POWER_PIN
#define PA_POWER_DISABLE				PA_POWER_PxOUT &= ~PA_POWER_PIN


#define IM_CH_SEL_PxDIR					P2DIR
#define IM_CH_SEL_PxOUT					P2OUT
#define IM_CH1_SEL_PIN					BIT5
#define IM_CH2_SEL_PIN					BIT4


#define IM_CH1_SEL						IM_CH1_SEL_PIN
#define IM_CH2_SEL						IM_CH2_SEL_PIN
void im_ch_sel(uint8_t channel);

	
// ******** Host Interface Pins ******** //
#define UCB1_PxSEL0						P3SEL0
#define UCB1_PxSEL1						P3SEL1
#define UCB1_PxDIR						P3DIR
#define UCB1_PxIN						P3IN
#define UCB1_SIMO_PIN					BIT1
#define UCB1_SOMI_PIN					BIT2
#define UCB1_UCLK_PIN					BIT0

// ******** TDC1000 Pins ******** //
#define TDC1000_LINK_LED_PxOUT			PJOUT
#define TDC1000_LINK_LED_PxDIR			PJDIR
#define TDC1000_LINK_LED_PIN			BIT2

#define TDC1000_MEAS_LED_PxOUT			PJOUT
#define TDC1000_MEAS_LED_PxDIR			PJDIR
#define TDC1000_MEAS_LED_PIN			BIT3
                         
#define TDC1000_XCLK_PxOUT				P10OUT
#define TDC1000_XCLK_PxDIR				P10DIR
#define TDC1000_XCLK_PxSEL0				P10SEL0
#define TDC1000_XCLK_PxSEL1				P10SEL1
#define TDC1000_XCLK_PIN				BIT2

#define TDC1000_OSCENABLE_PxOUT			P5OUT
#define TDC1000_OSCENABLE_PxDIR			P5DIR
#define TDC1000_OSCENABLE_PIN			BIT1

#define TDC1000_ENABLE_PxOUT			P6OUT
#define TDC1000_ENABLE_PxDIR			P6DIR
#define TDC1000_ENABLE_PIN				BIT7

#define TDC1000_RESET_PxOUT				P10OUT
#define TDC1000_RESET_PxDIR				P10DIR
#define TDC1000_RESET_PIN				BIT1

#define TDC1000_ERRB_PORTx				PORT_7
#define TDC1000_ERRB_PxDIR				P7DIR
#define TDC1000_ERRB_PxIE				P7IE
#define TDC1000_ERRB_PxIES				P7IES
#define TDC1000_ERRB_PxIFG				P7IFG
#define TDC1000_ERRB_PxREN				P7REN
#define TDC1000_ERRB_PxOUT				P7OUT
#define TDC1000_ERRB_PxIN				P7IN
#define TDC1000_ERRB_PIN				BIT6
#define TDC1000_ERRB_VECTOR				PORT7_VECTOR

#define TDC1000_CHSEL_PxOUT				P7OUT
#define TDC1000_CHSEL_PxDIR				P7DIR
#define TDC1000_CHSEL_PIN				BIT5

#define TDC1000_CSn_PxOUT				P5OUT
#define TDC1000_CSn_PxDIR				P5DIR
#define TDC1000_CSn_PIN					BIT3

// ******** TDC7200 Pins ******** //
#define TDC7200_CSn_PxOUT				P5OUT
#define TDC7200_CSn_PxDIR				P5DIR
#define TDC7200_CSn_PIN					BIT2  

#define TDC7200_ENABLE_PxOUT			P1OUT
#define TDC7200_ENABLE_PxDIR			P1DIR
#define TDC7200_ENABLE_PIN				BIT5

#define TDC7200_INTB_PxDIR				P2DIR
#define TDC7200_INTB_PxIE				P2IE
#define TDC7200_INTB_PxIES				P2IES
#define TDC7200_INTB_PxIFG				P2IFG
#define TDC7200_INTB_PxREN				P2REN
#define TDC7200_INTB_PxOUT				P2OUT
#define TDC7200_INTB_PxIN				P2IN
#define TDC7200_INTB_PIN				BIT6
#define TDC7200_INTB_VECTOR				PORT2_VECTOR


#define TI_GPIO5_ON                    { P4DIR |= BIT7; P4OUT |= BIT7;}
#define TI_GPIO5_OFF                   { P4DIR |= BIT7; P4OUT &= ~BIT7;}
#define TI_GPIO5_Pulse				   { TI_GPIO5_ON;TI_GPIO5_OFF;}
#define TI_GPIO5_INPUT_MODE            { P4DIR &= ~BIT7;}


#endif                                                                         // HEADER_FILE_MSP430_HARDWARE_BOARD_H
