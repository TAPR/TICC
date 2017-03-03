#ifndef TDC7200_H
#define TDC7200_H

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license


#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define SPI_SPEED         (int32_t)  20000000 // 20MHz maximum

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

// Channel structure type representing one TDC7200 Channel
class tdc7200Channel {
private:
  const int ENABLE;
  
public:
  const char ID;   // channel letter
  const int STOP;  // pin number on Arduino
  const int INTB;  // pin number on Arduino
  const int CSB;   // pin number on Arduino
  const int LED;   // pin number on Arduino

  // NOTE: changed all from signed to unsigned while working on TINT
  volatile int64_t PICstop;
  uint32_t time1Result;
  uint32_t time2Result;
  uint32_t time3Result;
  uint32_t clock1Result;
  uint32_t cal1Result;
  uint32_t cal2Result;
  
  int64_t tof;
  int64_t last_tof;
  int64_t ts;
  int64_t last_ts; 
  int64_t period;
  int64_t totalize;
  int64_t time_dilation;
  int64_t fixed_time2;
  int64_t fudge0;

  byte    config_byte1;
  byte    config_byte2;
  
  tdc7200Channel(char id, int enable, int intb, int csb, int stop, int led);
  int64_t read();
  void tdc_setup();
  void ready_next();
  byte readReg8(byte address);
  uint32_t readReg24(byte address);
  void write(byte address, byte value);
};

#endif	/* TDC7200_H */
