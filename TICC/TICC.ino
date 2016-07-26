// TICC.ino - master sketch file

// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.53 -- 24 July 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Tom McDermott N5EG 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license



/*******************************************************************************************
 Set these constants to match your hardware configuration
********************************************************************************************/
#define CLOCK_FREQ            (uint32_t)1e7    // Hz
#define CALIBRATION2_PERIODS  20               // Can only be 2, 10, 20, or 40.
#define PICTICK_PS            (uint32_t)1e8    // 100 uS PICDIV strap is 1e8 picoseconds, 1ms is 1e9
#define FUDGE0_PS             (uint32_t)0      // adjust for propagation delays, in picoseconds
/*******************************************************************************************/

#include <stdint.h>   // define unint16_t, uint32_t
#include <SPI.h>      // SPI support
#include "TICC.h"     // Register and structure definitions

//#include <stdio.h>    // format strings for printing. Eliminate???
// use BigNumber library from http://www.gammon.com.au/Arduino/BigNumber.zip
#include <BigNumber.h>

volatile unsigned long PICCount;
long int ti_0;
long int ti_1;

unsigned long tmp;
long tof_rollovers = 0;
long PICstop_long, tof_long, last_tof_long;

// BigNumbers seem to need initialized with "1", "0" causes undertermined value.  BigNums are in string form
BigNumber PICstop = "1";
BigNumber period = "1";
BigNumber PICtick_ps = "100000000";  // we should get this from define, but not sure how to
BigNumber tof = "1";
BigNumber ts = "1";
BigNumber tsprev = "1";
char buf[20];

// Enumerate the TDC7200 channel structures
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
static tdc7200Channel channels[] = {
	tdc7200Channel('A', ENABLE_A, INTB_A, CSB_A, STOP_A),
	tdc7200Channel('B', ENABLE_B, INTB_B, CSB_B, STOP_B),
};

// properties of the TDC7200 chip:
// Most Significant Bit is clocked first (MSBFIRST)
// clock is low when idle
// data is clocked on the rising edge of the clock (seems to be SPI_MODE0)
// max clock speed: 20 mHz
// Using TDC7200 timing mode 2...
void setup() {
  int i;

  PICCount = 0;
  // start the SPI library:
  SPI.begin();

  pinMode(interruptPin, INPUT);

  for(i = 0; i < ARRAY_SIZE(channels); ++i)
    channels[i].setup();

  attachInterrupt(digitalPinToInterrupt(interruptPin), coarseTimer, RISING);

  for(i = 0; i < ARRAY_SIZE(channels); ++i)
      channels[i].ready_next();

  BigNumber::begin();
 // BigNumber::setScale (0);
  
  Serial.begin(115200);
  Serial.println("");
  Serial.println("TOF   PICstop   Timestamp   Period");
  
}  

void loop() {
  int i;
  for(i = 0; i < ARRAY_SIZE(channels); ++i) {

    // If the TDC7200 has completed a measurement, INTB will be false.
    // Read and print the registers and computations, then clear the interrupt.
    if(digitalRead(channels[i].INTB)==0) {
      
        // grab this as soon as we can
        PICstop_long = PICCount;

        // read tof into tmp   
        last_tof_long = tof_long;
        tof_long = channels[i].read();

        // get ready for next reading
        channels[i].ready_next(); // Re-arm for next measurement, clear TDC INTB
        
        // has the TOF rolled over or under?  This depends on PICtick_ps being 1e8  HELP ME!!!
        if ((tof_long <  4e7L) && (last_tof_long > 6e7L)) {
          tof_rollovers--;
        }
        if ((tof_long >  4e7L) && (last_tof_long < 6e7L)) {
          tof_rollovers++;
        }
        
        PICstop_long += tof_rollovers;
        
        // convert long to string, then to bignumber and multiply to get ps result
        ltoa(PICstop_long, buf, 10);
        // calculate the "raw" timestamp as BigNumber
        PICstop = BigNumber(buf) * PICtick_ps;
        
        
        //convert long to string
        ltoa(tof_long, buf, 10);
        // copy into BigNumber
        tof = BigNumber(buf);

        
        
        // do some math
        tsprev = ts;
        ts = PICstop - tof;
        period = ts - tsprev;

        printBigNum(tof);
        Serial.print("  ");
        printBigNum(PICstop);
        Serial.print("  ");
        printBigNum(ts);
        Serial.print("  ");
        printBigNum(period);
        Serial.println();      
        
        
    } // if
  } // for
} // loop
 
// ISR for timer. NOTE: uint_64 rollover would take
// 62 million years at 100 usec interrupt rate.
// NOTE: change to uint32 for now

void coarseTimer() {
  PICCount++;
}

// Constructor
tdc7200Channel::tdc7200Channel(char id, int enable, int intb, int csb, int stop):
	ID(id), ENABLE(enable), INTB(intb), CSB(csb), STOP(stop) {

	pinMode(ENABLE,OUTPUT);
	pinMode(INTB,INPUT);
	pinMode(CSB,OUTPUT);
	pinMode(STOP,INPUT);
};

// Initial config for TDC7200

void tdc7200Channel::setup() {
  digitalWrite(ENABLE, LOW);
  delay(1);
  digitalWrite(ENABLE, HIGH);  // Needs a low-to-high transition to enable
  delay(1);
  
  write(INT_MASK, 0x01);  // disable clock overflow interrupts, allow only measurement interrupt
  write(CONFIG2, 0x83);  // Cal2 is 20 clocks, 1 meas cycle, 1 stop
  
  write(CLOCK_CNTR_STOP_MASK_H, 0x00);
  write(CLOCK_CNTR_STOP_MASK_L, 0x00);  // hold off no clocks before enabling STOP

  write(COARSE_CNTR_OVF_H, 0xF0);   
  write(COARSE_CNTR_OVF_L, 0x00);

  write(CLOCK_CNTR_OVF_H, 0xF0);   
  write(CLOCK_CNTR_OVF_L, 0x00);
  
}

void tdc7200Channel::reset() {
	tof = 0;
	//PICstop = 0;
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
  
  long ringticks;  //  time1Result - time2Result, unitless
  long ringps;  // ps per ring cycle, picosends, result of calibration formula, nominal 55ps 
  long ringtime;  // ringticks * ringperiod, picoseconds
  unsigned long clocktime;    // clock1Result * CLOCK_PERIOD, picoseconds
  unsigned long tof; // clocktime - ringtime, picoseconds; "time of flight" per datasheet

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

// Enable next measurement cycle
void tdc7200Channel::ready_next() {
    // needs to set the enable bit (START_MEAS in CONFIG1)
    // clears interrupt bits
    delay(10);  
    write(CONFIG1, 0x83);  // Measurement mode 2 - force cal

}

// Calculate and print time interval to serial
void output_ti() {
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

void printps(unsigned long long x) {
 char buffer[100];
 sprintf(buffer, "%0lu", x/1000000L);
 Serial.print(buffer);  
 sprintf(buffer, "%0lu", x%1000000L);
 Serial.print(buffer);  

 // long long int sec, secx, frac;    
 // sec = x / 1e12;
 // secx = sec * 1e12;
 // frac = x - secx;
 // Serial.print((unsigned long)sec), Serial.print("."), Serial.print((unsigned long)frac);
}      

void printBigNum(BigNumber & n) {
  char * s = n.toString();
  Serial.print(s);
  free(s);
}


