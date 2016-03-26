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

uint8_t action_items;
uint8_t TDC_chip_reset;

void main(void)
{
	WDTCTL = WDTPW+WDTHOLD;									// Stop WDT

	init_board();											// Init I/O, clock and other peripherals

	TDC_SPISetup();											// Init SPI

	Init_TDC1000_TDC7200();									// Init TDC chip set


#ifdef GUI
	P4SEL0		|= BIT2 + BIT3;								// Enable UART
	UCA0CTL1	&= ~UCSWRST;								// when measuring system current consumption,
	UCA0IFG		&= ~UCRXIFG;								// un-define "GUI" and remove the connection of UART
	UCA0IE		|= UCRXIE;

	TDC_chip_reset = 0;

	set_dcdc_voltage(DCDC_3_3V);							// set the Vcc to 3.3V
	TDC_Chips_On();											// sequence to turn on TDC1000 , TDC7200 to init them and 8MHz Oscillator
	TDC_Chips_Off();										// a sequence to turn off TDC1000, TDC7200 and 8MHz Oscillator

	receiving_cmd |= BIT1;									// Initialize with STOP command from GUI

	timer_trigger_freq = 9;									// default set to 1 measurement per time interval of 1 second.
															// time interval = 100ms * (timer_trigger_freq + 1)

#else
	receiving_cmd = 0;
	timer_trigger_freq = 9;									// default set to 1 measurement per time interval of 1 second.
															// time interval = 100ms * (timer_trigger_freq + 1)
	tdc_state_flag |= TDC_CONTINUOUS_TRIGGER_STATE ;        // continuous trigger
#endif

	interval_timer_init();									// start the timer


		while(1)
		{
			LPM3;											// enter Low power mode 3

#ifdef GUI

			Handle_GUI_Command();							// check for GUI command

			if (action_items & HANDLE_NEXT_TRIGGER)
			{ action_items &= ~HANDLE_NEXT_TRIGGER;

			if ((!(receiving_cmd&BIT1))||(receiving_cmd&BIT2)|| (tdc_state_flag &TDC_CONTINUOUS_TRIGGER_STATE ))  //
				{

#endif

			LCDCCTL0 &= ~LCDON;

			set_dcdc_voltage(DCDC_3_3V);					// set the Vcc to 3.3V
			TDC_Chips_On();									// sequence to turn on TDC1000 , TDC7200 and 8MHz Oscillator
			tdc_trigger_measure();							// a call to make a measurement of one upstream and one downstream
			TDC_Chips_Off();								// a sequence to turn off TDC1000, TDC7200 and 8MHz Oscillator


#ifdef GUI

				}

					if ((!(receiving_cmd&BIT1))||(receiving_cmd&BIT2))
					{	receiving_cmd &= ~BIT2;
						Send_Data_To_GUI();                 // send the registers reading from TDC7200 to GUI

					}

			}
#else

	action_items &= ~HANDLE_NEXT_TRIGGER;
	set_dcdc_voltage(DCDC_2_5V);							// set the Vcc to 2.5V and enter LPM3 in next instruction, to save power consumption
#endif

#ifndef DISABLE_LCD
	Display_Result_LCD();
#endif
		}

}

void Handle_GUI_Command(void)
{
unsigned int i=0;

	if (action_items & HANDLE_COMMAND)									// check if GUI command received via UART
			{
				TDC_Chips_On();
				handleHostCommand();									// execute the command

				UART_Send_Bytes(cmdResponseString, MAX_STR_LENGTH);     // send back the command respond to GUI

				TDC_Chips_Off();


				receiving_cmd &= ~BIT0;

				for (i = 0; i < MAX_STR_LENGTH; i++)
				{
					cmdResponseString[i] = 0x00;
				}
				action_items &= ~HANDLE_COMMAND;

			}

}


void TDC_Chips_On(void)
{
	UCB1CTLW0 &= ~UCSWRST;								// Switch on SPI module
	tdc_enable_clock();									// Enable 8MHz Oscillator

	TDC1000_ENABLE_PxOUT |= TDC1000_ENABLE_PIN;			// Enable TDC1000
	TDC7200_ENABLE_PxOUT |= TDC7200_ENABLE_PIN;			// Enable TDC7200

	TDC7200_INTB_PxREN	|= TDC7200_INTB_PIN;			// Pull-up Resistor enable for Interrupt input

	TDC1000_CS_DIS;										// TDC1000 chip disable for SPI
	TDC7200_CS_DIS;										// TDC7200 chip disable for SPI

	TDC1000_reg_init();									// init registers of TDC1000
	TDC7200_reg_init();									// init registers of TDC7200

	TDC1000_CS_DIS;
	TDC7200_CS_DIS;

}

void TDC_Chips_Off(void)
{

#ifdef GUI

unsigned int i;

	// Read and store Register settings of TDC7200
	TDC7200_SPIAutoIncReadReg(TDC7200_CONFIG1_REG, TDC7200_reg_local_copy, TDC7200_TOTAL_NUM_CONFIG_REG);
	// reset "Start New Measurement" bit to avoid measurement being triggered
	TDC7200_reg_local_copy[TDC7200_CONFIG1_REG] &= ~0x01;

	// Read and store Register settings of TDC1000
	for (i=0; i< 10; i++)
	{TDC1000_reg_local_copy[i] = TDC1000_SPIByteReadReg(i) ;}

#endif
//#esle

	tdc_disable_clock();								// Disable 8MHz oscillator

	TDC1000_ENABLE_PxOUT &= ~TDC1000_ENABLE_PIN;		// Disable TDC1000
	TDC7200_ENABLE_PxOUT &= ~TDC7200_ENABLE_PIN;		// Disable TDC7200

	TDC7200_INTB_PxREN &= ~TDC7200_INTB_PIN;			// Pull-up Resistor disabled

	TDC1000_CS_EN;										// Pull low the I/O pins for SPI selection
	TDC7200_CS_EN;

	UCB1CTLW0 |= UCSWRST;							    // Switch off SPI

//#endif

}


void Send_Data_To_GUI(void)
{

if (tdc_state_flag & TDC_UP_STREAM_BUFFER_READY)													// Send the data to GUI for the upstream measure, via UART
				{																					// Data is the register reading from TDC7200
	if(!(receiving_cmd&BIT0))  UART_Send_Bytes(upStreamBuf,BUF_LENGTH);

					tdc_state_flag &= ~TDC_UP_STREAM_BUFFER_READY;
					if (tdc_state_flag & TDC_SINGLE_SHOT_MEASURE_STATE)
					{
						tdc_state_flag &= ~(TDC_SINGLE_SHOT_MEASURE_STATE + TDC_DOWN_STREAM_BUFFER_READY + TDC_RTD_BUFFER_READY);

					}

					// Wait for the packet to be completely sent out.
					delay_ACLK(327);
				}


if (tdc_state_flag & TDC_DOWN_STREAM_BUFFER_READY)													// Send the data to GUI for the downstream measure, via UART
				{																					// Data is the register reading from TDC7200
	if(!(receiving_cmd&BIT0)) 	UART_Send_Bytes(downStreamBuf,BUF_LENGTH);

					tdc_state_flag &= ~TDC_DOWN_STREAM_BUFFER_READY;
					// Wait for the packet to be completely sent out.
					delay_ACLK(327);
				}

if (tdc_state_flag & TDC_RTD_BUFFER_READY)															// Send the data of temperature sensor via UART
				{
	if(!(receiving_cmd&BIT0))   UART_Send_Bytes(RTDBuf,BUF_LENGTH);

					tdc_state_flag &= ~TDC_RTD_BUFFER_READY;
					// Wait for the packet to be completely sent out.
					delay_ACLK(327);
				}

    receiving_cmd &= ~BIT0;

}

void Display_Result_LCD(void)
{

    long int us, ur;
    long int ds, dr;
    long int dn;
    long int dmy;

    double b;
    double d;
    float h;
    float k;

    long int uTime[6];
    long int uClk[5];
    long int uCal[2];

    long int dTime[6];
    long int dClk[5];
    long int dCal[2];

    unsigned int i;

#define STOP_NUM 3


   for (i=0; i<6; i++)
   {
   	uTime[i]   = 0;
   	uTime[i]   = upStreamBuf[6*i];
   	uTime[i] <<= 8;
   	uTime[i]  += upStreamBuf[6*i + 1];
   	uTime[i] <<= 8;
   	uTime[i]  += upStreamBuf[6*i + 2];

   	dTime[i]   = 0;
   	dTime[i]   = downStreamBuf[6*i];
   	dTime[i] <<= 8;
   	dTime[i]  += downStreamBuf[6*i + 1];
   	dTime[i] <<= 8;
   	dTime[i]  += downStreamBuf[6*i + 2];
   }


   for (i=0; i<5; i++)
   {
   	uClk[i]   = 0;
   	uClk[i]   = upStreamBuf[6*i + 3];
   	uClk[i] <<= 8;
   	uClk[i]  += upStreamBuf[6*i + 4];
   	uClk[i] <<= 8;
   	uClk[i]  += upStreamBuf[6*i + 5];

   	dClk[i]   = 0;
   	dClk[i]   = downStreamBuf[6*i + 3];
   	dClk[i] <<= 8;
   	dClk[i]  += downStreamBuf[6*i + 4];
   	dClk[i] <<= 8;
   	dClk[i]  += downStreamBuf[6*i + 5];
   }


   for (i=0; i<2; i++)
   {
   	uCal[i]   = 0;
   	uCal[i]   = upStreamBuf[33 + i*3];
   	uCal[i] <<= 8;
   	uCal[i]  += upStreamBuf[34 + i*3];
   	uCal[i] <<= 8;
   	uCal[i]  += upStreamBuf[35 + i*3];

   	dCal[i]   = 0;
   	dCal[i]   = downStreamBuf[33 + i*3 ];
   	dCal[i] <<= 8;
   	dCal[i]  += downStreamBuf[34 + i*3];
   	dCal[i] <<= 8;
   	dCal[i]  += downStreamBuf[35 + i*3];
   }


   // use double and float type for calculation
   // this is for reference only, User has to consider the cycle time using double or float varaible for calculation
      b = 9*0.125e-6 / (uCal[1] - uCal[0]);
      h = 9*0.125e-6 / (uCal[1] - uCal[0]);

      b = (uTime[0] - uTime[STOP_NUM])*b + uClk[STOP_NUM-1]*0.125e-6;
      h = (uTime[0] - uTime[STOP_NUM])*h + uClk[STOP_NUM-1]*0.125e-6;

      d = 9*0.125e-6 / (dCal[1] - dCal[0]);
      k = 9*0.125e-6 / (dCal[1] - dCal[0]);

      d = (dTime[0] - dTime[STOP_NUM])*d + dClk[STOP_NUM-1]*0.125e-6;
      k = (dTime[0] - dTime[STOP_NUM])*k + dClk[STOP_NUM-1]*0.125e-6;



   // all long int calculaion without floating point
      dmy = (uTime[0] - uTime[STOP_NUM])*140625;
      dn  = uCal[1] - uCal[0];
      us  = dmy / dn;
      ur  = dmy - us*dn;
      dmy = dn / 2;
      ur  = (ur*8 + dmy);
      ur  = ur / dn;
      us  = 8*us + ur;
      us  = us + uClk[STOP_NUM - 1 ] * 125000;


      dmy = (dTime[0] - dTime[STOP_NUM])*140625;
      dn  = dCal[1] - dCal[0];
      ds  = dmy / dn;
      dr  = dmy - ds*dn;
      dmy = dn / 2;
      dr  = (dr*8 + dmy);
      dr  = dr / dn;
      ds  = 8*ds + dr;
      ds  = ds + dClk[STOP_NUM-1] * 125000;


      b   = b - d;
      ds  =us - ds;


  	 LCDCCTL0 |= LCDON;	                					    // turn on LCD
     lcd_display_num((int)ds);

}

