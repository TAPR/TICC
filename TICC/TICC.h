#ifndef TICC_H
#define TICC_H


//#define DETAIL_TIMING   // if enabled, prints execution time


// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.62 6 August 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Tom McDermott N5EG 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#define PS_PER_SEC        1e12  // ps/s
#define CLOCK_PERIOD_PS   (PS_PER_SEC/CLOCK_FREQ)  // ps

// hardware connections to TDC2700. Defines Arduino IDE pin number.
// changed for Rev. C board
const int ENABLE_A =		4;
const int INTB_A =		10;
const int CSB_A	=  		6;
const int STOP_A =		12;

const int ENABLE_B =		5;
const int INTB_B =		11;
const int CSB_B	=	  	7;
const int STOP_B =		13;

// TDC7200 register addresses
const int CONFIG1 =		0x00;           // default 0x00
const int CONFIG2 =		0x01;           // default 0x40
const int INT_STATUS =		0x02;           // default 0x00
const int INT_MASK =		0x03;           // default 0x07
const int COARSE_CNTR_OVF_H =	0x04;       	// default 0xff
const int COARSE_CNTR_OVF_L =	0x05;       	// default 0xff
const int CLOCK_CNTR_OVF_H =	0x06;        	// default 0xff
const int CLOCK_CNTR_OVF_L =	0x07;        	// default 0xff
const int CLOCK_CNTR_STOP_MASK_H = 0x08;	// default 0x00
const int CLOCK_CNTR_STOP_MASK_L = 0x09;	// default 0x00
// gap from 0x0A thru 0x0F...
const int TIME1 =		0x10;           // default 0x00_0000
const int CLOCK_COUNT1 =	0x11;           // default 0x00_0000
const int TIME2	=		0x12;           // default 0x00_0000
const int CLOCK_COUNT2 =	0x13;           // default 0x00_0000
const int TIME3	=		0x14;           // default 0x00_0000
const int CLOCK_COUNT3 =	0x15;           // default 0x00_0000
const int TIME4	=		0x16;           // default 0x00_0000
const int CLOCK_COUNT4 =	0x17;           // default 0x00_0000
const int TIME5	=		0x18;           // default 0x00_0000
const int CLOCK_COUNT5 =	0x19;           // default 0x00_0000
const int TIME6	=		0x1A;           // default 0x00_0000
const int CALIBRATION1 =	0x1B;           // default 0x00_0000
const int CALIBRATION2 =	0x1C;           // default 0x00_0000


// Coarse count interrupt assignments
// changed for Rev. C board
const int interruptPin =      18;		// Interrupt IDE Pin on Mega

// Channel structure type representing one TDC7200 Channel
class tdc7200Channel {
private:
	//  Some of these really should be private and hidden...
	const int ENABLE;
	const int CSB; 
	unsigned long time1Result;
  unsigned long time2Result;
	unsigned long clock1Result;
	unsigned long cal1Result;
  unsigned long cal2Result;
  
public:
  const char ID;   // channel letter
  const int STOP;  // pin number on Arduino
  const int INTB;  // pin number on Arduino
 
  long long int PICstop;
  unsigned long tof;
	long long int time_stamp;
  boolean idle;     // true = the channel is idle or stuck

	tdc7200Channel(char id, int enable, int intb, int csb, int stop);
	void setup();
	unsigned long read();
  unsigned long readReg24(byte address);
  byte readReg8(byte address);
	void ready_next();
	void reset();
	void write(byte address, byte value);
};
#endif	/* TICC_H */

