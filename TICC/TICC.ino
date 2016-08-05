// TICC.ino - master sketch file

// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.60 -- 30 July 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Tom McDermott N5EG 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

/**********************************************************************************************************
 Set these constants to match your hardware configuration
***********************************************************************************************************/
#define CLOCK_FREQ            (uint32_t)  1e7    // Reference Clock frequency in Hz
#define CALIBRATION2_PERIODS              20     // Can only be 2, 10, 20, or 40.
#define PICTICK_PS            (uint32_t)  1e8    // 100 uS PICDIV strap is 1e8 picoseconds, 1ms is 1e9
#define FUDGE0_PS             (uint32_t)  0      // adjust for propagation delays, in picoseconds
/**********************************************************************************************************/

#include <stdint.h>   // define unint16_t, uint32_t
#include <SPI.h>      // SPI support
#include "TICC.h"     // Register and structure definitions

// use BigNumber library from http://www.gammon.com.au/Arduino/BigNumber.zip
// #include <BigNumber.h>

volatile unsigned long long PICcount;  //volatile because in ISR

unsigned long long PICstop;
unsigned long long ts;
unsigned long long tof;
unsigned long totalize = 0;
unsigned long start_micros;
unsigned long end_micros;
char str[128];

// Enumerate the TDC7200 channel structures
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
static tdc7200Channel channels[] = {
	tdc7200Channel('A', ENABLE_A, INTB_A, CSB_A, STOP_A),
	tdc7200Channel('B', ENABLE_B, INTB_B, CSB_B, STOP_B),
};


void setup() {
  int i;

  PICcount = 0;
  // start the SPI library:
  SPI.begin();
  
  pinMode(interruptPin, INPUT);

  for(i = 0; i < ARRAY_SIZE(channels); ++i)
    channels[i].setup();

  // Don't know why, but need to catch on falling to be in sync
  attachInterrupt(digitalPinToInterrupt(interruptPin), coarseTimer, FALLING);
  
  Serial.begin(115200);
  Serial.println("");
  Serial.println("# timestamp (seconds)");


  for(i = 0; i < ARRAY_SIZE(channels); ++i)
      channels[i].ready_next();
  delay(100);
}  

void loop() {
  int i;
  for(i = 0; i < ARRAY_SIZE(channels); ++i) {

    if (channels[i].idle == true)  // ignore an idle or stuck channel
      continue;
      
    start_micros = micros();

    // STOP means an event has occurred
    if(digitalRead(channels[i].STOP) == 1) {    
        
        // grab this just as soon as we can
        PICstop = PICcount;
        
        // wait until INTB goes low to indicate TDC measurement complete
        unsigned int deadlock = 100000;
        while (digitalRead(channels[i].INTB)==1) {
          delayMicroseconds(5);
          if (--deadlock == 0) {
            channels[i].idle = true;
            Serial.print(" Channel "),Serial.print(channels[i].ID),Serial.println(" is stuck or idle. Now disabled.");
            break;
          }
        }

        // read registers and calculate tof
        tof = channels[i].read();
        
        // done with chip, so get ready for next reading
        channels[i].ready_next(); // Re-arm for next measurement, clear TDC INTB
              
       
       
        ts = ((PICstop * PICTICK_PS) - tof);
 
       
        if (totalize > 0) {  // first few readings likely to be bogus
          print_picos_as_seconds(ts);
        }
        
        totalize++; // number of measurements
        
#ifdef DETAIL_TIMING      
        end_micros = micros();         
        Serial.print(" execution time (us): ");
        Serial.print(end_micros - start_micros);
#endif
        delay(10);  // it locks up sometimes without this..............
        Serial.println();


    } // if
  } // for
} // loop

/**************************************************************************************/
 
// ISR for timer. NOTE: uint_64 rollover would take                          
// 62 million years at 100 usec interrupt rate.
// NOTE: change to uint32 for now

void coarseTimer() {
  PICcount++;
}


// Constructor
tdc7200Channel::tdc7200Channel(char id, int enable, int intb, int csb, int stop):
	ID(id), ENABLE(enable), INTB(intb), CSB(csb), STOP(stop) {

	pinMode(ENABLE,OUTPUT);
	pinMode(INTB,INPUT);
	pinMode(CSB,OUTPUT);
	pinMode(STOP,INPUT);

  idle = false;   // assume the channel has signal hooked up triggering
};

// Initial config for TDC7200
void tdc7200Channel::setup() {
  digitalWrite(ENABLE, LOW);
  delay(5);  
  digitalWrite(ENABLE, HIGH);  // Needs a low-to-high transition to enable
  delay(5);  // 1.5ms minimum recommended to allow chip LDO to stabilize
  
  
  write(CONFIG2, 0x81);  // SHOULD BE 0x80 for Cal2 (20 clocks, 1 meas cycle, 1 stop) but that doesn't work
 // write(INT_MASK, 0x01);  // was 0x01 disable clock overflow interrupts, allow only measurement interrupt
  
  
 // write(CLOCK_CNTR_STOP_MASK_H, 0x00); // was 0x00
 // write(CLOCK_CNTR_STOP_MASK_L, 0x01); // was 0x00 // hold off no clocks before enabling STOP

 // write(COARSE_CNTR_OVF_H, 0xFF);  //was 0xF0 
 // write(COARSE_CNTR_OVF_L, 0xFF);

 write(CLOCK_CNTR_OVF_H, 0xF0);   // was 0xF0
 write(CLOCK_CNTR_OVF_L, 0x00);  // was 0x00
  
}

// Enable next measurement cycle
void tdc7200Channel::ready_next() {
    // needs to set the enable bit (START_MEAS in CONFIG1)
    // clears interrupt bits
    delayMicroseconds(1);  // was 10ms  
    write(CONFIG1, 0x83);     // was 0x83 Measurement mode 2 - force cal
}


// Read TDC for channel
unsigned long tdc7200Channel::read() {
  //  Start a SPI transaction
  //  Max speed for the tdc7200 is 20MHz
  //  CPOL = 0; CPHA = 0

  time1Result = readReg24(TIME1);
  time2Result  = readReg24(TIME2);
  clock1Result = readReg24(CLOCK_COUNT1);
  cal1Result = readReg24(CALIBRATION1);
  cal2Result = readReg24(CALIBRATION2);

/*
  Serial.print(" time1Result="), Serial.print(time1Result);
  Serial.print(" time2Result="), Serial.print(time2Result);
  Serial.print(" clock1Result="), Serial.println(clock1Result);
  Serial.print(" cal1Result="), Serial.print(cal1Result);
  Serial.print(" cal2Result="), Serial.println(cal2Result);
*/
  
  long ringticks;            //  time1Result - time2Result, unitless
  long ringps;               // ps per ring cycle, picosends, result of calibration formula, nominal 55ps 
  long ringtime;             // ringticks * ringperiod, picoseconds
  unsigned long clocktime;   // clock1Result * CLOCK_PERIOD, picoseconds
  unsigned long tof;         // clocktime - ringtime, picoseconds; "time of flight" per datasheet

  // get ringperiod.  Datasheet says: (cal2Result - cal1Result)/(cal2Periods - 1)
  
  // TODO: 
  // 1. refactor so that we get 0.1ps effective ringps precision in the ringtime calculation
  // 2. optimize!!!
  ringps = CLOCK_PERIOD_PS / ((cal2Result - cal1Result)/(CALIBRATION2_PERIODS -1));
  ringticks = time1Result - time2Result;
  ringtime = ringticks * ringps;
  
  clocktime = clock1Result * CLOCK_PERIOD_PS;
  tof = clocktime - ringtime - FUDGE0_PS;  //FUDGE0 compensates for propagation delays
    
  /*  
  Serial.print(" ringps: ");Serial.print(ringps);
  Serial.print(" time1: ");Serial.print(time1Result);
  Serial.print(" time2: ");Serial.print(time2Result);
  Serial.print(" ringticks: ");Serial.print(ringticks); 
  Serial.print(" ringtime: ");Serial.print(ringtime);
  Serial.print(" clock1: ");Serial.print(clock1Result);
  Serial.print(" tof: ");Serial.print(tof);
  Serial.print("  ");
  */
  
  return tof;
}

// Chip properties:
// Most Significant Bit is clocked first (MSBFIRST)
// clock is low when idle
// data is clocked on the rising edge of the clock (seems to be SPI_MODE0)
// max clock speed: 20 mHz

byte tdc7200Channel::readReg8(byte address) {
  byte inByte = 0;

  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  // take the chip select low to select the device:
  digitalWrite(CSB, LOW);

  SPI.transfer(address & 0x1f);
  inByte = SPI.transfer(0x00);

  digitalWrite(CSB, HIGH);
  SPI.endTransaction();

  return inByte;
}

unsigned long tdc7200Channel::readReg24(byte address) {
  unsigned long value = 0;

  // CSB needs to be toggled between 24-bit register reads

  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CSB, LOW);

  SPI.transfer(address & 0x1f);

  unsigned int msb = SPI.transfer(0x00);
  unsigned int mid = SPI.transfer(0x00);
  unsigned int lsb = SPI.transfer(0x00);

  value = (msb << 16) + (mid << 8) + lsb;

  digitalWrite(CSB, HIGH);
  SPI.endTransaction();
  return value;
}

void tdc7200Channel::write(byte address, byte value) {

  // take the chip select low to select the device:
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CSB, LOW);

  // Force Address bit 6 to one for a write
  SPI.transfer16((address | 0x40) << 8 | value);

  digitalWrite(CSB, HIGH);
  SPI.endTransaction();
}



void print_picos_as_seconds (uint64_t x) {
  uint64_t sec, secx, frac, frach, fracx, fracl;    
  sec = x / 1000000000000;
  secx = sec * 1000000000000;
  frac = x - secx;

  // break fractional part of seconds into two 6 digit numbers

  frach = frac / 1000000;
  fracx = frach * 1000000;
  fracl = frac - fracx;

  sprintf(str,"%lu",sec), Serial.print(str), Serial.print(".");
  sprintf(str, "%06lu", frach), Serial.print(str);
  sprintf(str, "%06lu", fracl), Serial.print(str);
  
} 



