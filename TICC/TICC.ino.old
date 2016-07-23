// TICC.ino - master sketch file

// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.51 -- 5 July 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Tom McDermott N5EG 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>   // define unint16_t, uint32_t
#include <SPI.h>      // SPI support
#include "TICC.h"     // Register and structure definitions

volatile unsigned long long coarseCount = 0;
long int ti_0;
long int ti_1;

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
  // start the SPI library:
  SPI.begin();

  pinMode(interruptPin, INPUT);

  for(i = 0; i < ARRAY_SIZE(channels); ++i)
    channels[i].setup();

//  attachInterrupt(digitalPinToInterrupt(interruptPin), coarseTimer, RISING);

//  delay(1000);

  for(i = 0; i < ARRAY_SIZE(channels); ++i)
      channels[i].ready_next();

  Serial.begin(115200);
  Serial.println("");
  Serial.println("Starting...");
}

void loop() {
  int i;

  delay(5000);

  for(i = 0; i < ARRAY_SIZE(channels); ++i) {
    if(channels[i].STOP)
      channels[i].stopTime = coarseCount;

// If the TDC7200 has completed a measurement, INTB will be false.
// Read and print the registers and computations, then clear the interrupt.

    if(digitalRead(channels[i].INTB)==0) {

      Serial.print(" Int Status = ");
      Serial.print(channels[i].readReg8(INT_STATUS), HEX);   
 //     Serial.print("   Int_mask = ");
 //     Serial.println(channels[i].readReg8(INT_MASK), HEX);
      
      channels[i].time_interval = channels[i].read();
      channels[i]. write(INT_STATUS, 0x1f);  //clear interrupt

      channels[i].time_stamp = channels[i].stopTime - channels[i].time_interval;

      Serial.print("channel "), Serial.print(i);
      Serial.print("  stop time: "), Serial.print(channels[i].stopTime);      
      Serial.print("  time interval: "), Serial.print(channels[i].time_interval);
      Serial.print("  timestamp: "), Serial.println(channels[i].time_stamp);

      channels[i].ready_next(); // Re-arm for next measurement   
    }
  
  }
  Serial.println("");
}

// ISR for timer. NOTE: uint_64 rollover would take
// 62 million years at 100 usec interrupt rate.
// NOTE: change to uint32 for now

void coarseTimer() {
  coarseCount++;
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
  delay(10);
  digitalWrite(ENABLE, HIGH);  // Needs a low-to-high transition to enable
  delay(10);
  
  write(INT_MASK, 0x01);  // disable clock overflow interrupts, allow only measurement interrupt
  write(CONFIG2, 0x80);  // Cal2 is 20 clocks
  
  write(CLOCK_CNTR_STOP_MASK_H, 0x00);
  write(CLOCK_CNTR_STOP_MASK_L, 0x01);  // hold off one clock before enabling STOP

//  write(COARSE_CNTR_OVF_H, 0x00);   // gives longest time until overflow
//  write(COARSE_CNTR_OVF_L, 0x00);

//  write(CLOCK_CNTR_OVF_H, 0x00);   // gives longest time until overflow
//  write(CLOCK_CNTR_OVF_L, 0x00);
  
}

void tdc7200Channel::reset() {
	time_interval = 0;
	stopTime = 0;
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
  

  //  These values are 24-bit and we're reading them into a 32-bit
  //  variable.
  //
  //  This does some fancy pointer games to read it into the int starting
  //  at the 2nd byte in rather than the 1st.  That should get the correct
  //  2-4bit value read.
  //  This clobbers the stack at runtime:
//	SPI.transfer((uint8_t *) &value + 1, 3);

	return value;
}

// Read TDC for channel
long int tdc7200Channel::read() {
  //  Start a SPI transaction
  //  Max speed for the tdc7200 is 20MHz
  //  CPOL = 0; CPHA = 0
//  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
//  digitalWrite(CSB, LOW);

  time1Result = readReg24(TIME1);
  time2Result  = readReg24(TIME2);
  clock1Result = readReg24(CLOCK_COUNT1);
  cal1Result = readReg24(CALIBRATION1);
  cal2Result = readReg24(CALIBRATION2);

  Serial.print(" time1Result="), Serial.print(time1Result);
  Serial.print(" time2Result="), Serial.print(time2Result);
  Serial.print(" clock1Result="), Serial.println(clock1Result);
  Serial.print(" cal1Result="), Serial.print(cal1Result);
  Serial.print(" cal2Result="), Serial.println(cal2Result);
  
  uint32_t calc_time;
  uint16_t tempu16;
  uint32_t tempu32;

  // since time1Result will always be > time2Result, this is still 16 bit.
  tempu16 = (time1Result - time2Result); 
  // After multiplying by the constants, you will now be a 32 bit number.
  tempu32 = tempu16 * (CLOCK_PERIOD_PS * (CALIBRATION2_PERIODS-1));
  // This division sort of sucks, but since I assume these must be 
  // variables there's no way around it.
  tempu32 = (tempu32 + ((cal2Result - cal1Result + 1) >> 1)) /
    (cal2Result - cal1Result);
  // Add in another 32bit variable.  Given the limitations on
  // inputs, these two 32 bits still won't overflow.
  tempu32 += clock1Result * CLOCK_PERIOD_PS;
  // return tempu32;
//  digitalWrite(CSB, HIGH);
//  SPI.endTransaction();

  return (unsigned long)tempu32;
}

// Enable next measurement cycle
void tdc7200Channel::ready_next() {
    // needs to set the enable bit (START_MEAS in CONFIG1)

    delay(10);  
    write(CONFIG1, 0x83);  // Measurement mode 2 - force cal

//      write(CONFIG1, 0x03);  // Measurement mode 2 - don't force cal     
//      write(CONFIG1, 0x81);  // Measurement mode 1
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


