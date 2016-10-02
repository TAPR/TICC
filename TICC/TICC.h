#ifndef TICC_H
#define TICC_H

// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.79 2 Octobert 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <EEPROM.h>
#include <Arduino.h>  // for type definitions

#define PS_PER_SEC        (int64_t)  1000000000000  // ps/s
#define SPI_SPEED         (int32_t)  20000000 // 20MHz maximum
#define CONFIG_START      (byte)     0 // first byte of config in eeprom
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

// interrupt assignments
const int COARSEint = 18;   // Interrupt for COARSE clock
const int STOPAint =  19;   // Interrupt for STOPA
const int STOPBint =  20;   // Interrupt for STOP

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

// configuration structure type
struct config_t {
  const byte EEPROM_VERSION = 0x01; // one byte   
  char       SW_VERSION[17];        // up to 16 bytes plus term
  char       BOARD_SER_NUM[17];     // up to 16 bytes plus term
  
  // global settings:
  char       MODE;                  // time(S)tamp, time (I)nterval
                                    // Time(L)ab, (P)eriod (default 'S')
  int64_t    CLOCK_HZ;              // clock in Hz (default 10 000 000)
  int64_t    PICTICK_PS;            // coarse tick in ps (default 100 000 000) (100us)
  int16_t    CAL_PERIODS;           // calibration periods 2, 10, 20, 40 (default 20)
  // per-channel settings, so arrays of 2 values for channels A and B:
  char       START_EDGE[2];         // start on (R)ising (default) or (F)alling edge
  int64_t    TIME_DILATION[2];      // time dilation factor (default TBD)
  int64_t    FIXED_TIME2[2];        // if >0 use to replace time2 (default 0)
  int64_t    FUDGE0[2];             // fudge factor (ps) (default 0)
};

// Channel structure type representing one TDC7200 Channel
class tdc7200Channel {

private:
  const int ENABLE;
  const int CSB; 
  uint32_t time1Result;
  uint32_t time2Result;
  uint32_t time3Result;
  uint32_t clock1Result;
  uint32_t cal1Result;
  uint32_t cal2Result;
  
public:
  const char ID;   // channel letter
  const int STOP;  // pin number on Arduino
  const int INTB;  // pin number on Arduino
  // NOTE: changed all from signed to unsigned while working on TINT
  volatile int64_t PICstop;
  int64_t tof;
  int64_t last_tof;
  int64_t ts;
  int64_t last_ts; 
  int64_t period;
  int64_t totalize;
  int64_t time_dilation;
  int64_t fixed_time2;
  int64_t fudge0;
  
  tdc7200Channel(char id, int enable, int intb, int csb, int stop);
  int64_t read();
  uint32_t readReg24(byte address);
  uint8_t readReg8(byte address);
  void tdc_setup();
  void ready_next();
  void write(byte address, byte value);
};

// These allow us to read/write struct in eeprom
template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
    const byte* p = (const byte*)(const void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
    return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
    return i;
}

#endif	/* TICC_H */

