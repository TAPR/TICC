// TICC.ino - master sketch file

// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.5 -- 3 July 2016

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

  attachInterrupt(digitalPinToInterrupt(interruptPin), coarseTimer, RISING);

  delay(10);

  for(i = 0; i < ARRAY_SIZE(channels); ++i)
      channels[i].ready_next();

  Serial.begin(115200);
  Serial.println("Starting...");
}

void loop() {
  int i;

  for(i = 0; i < ARRAY_SIZE(channels); ++i) {
    if(channels[i].STOP)
      channels[i].stopTime = coarseCount;

    if(channels[i].INTB) {
      channels[i].time_interval = channels[i].read();
      channels[i].time_stamp = channels[i].stopTime - channels[i].time_interval;
      channels[i].ready_next();
      Serial.print("channel ");
      Serial.print(i);
      Serial.print(" time interval: ");
      Serial.print(channels[i].time_interval);
      Serial.print(" timestamp: ");
      Serial.println(channels[i].time_stamp);
    }
  }
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
  digitalWrite(ENABLE, HIGH);
}

void tdc7200Channel::reset() {
	time_interval = 0;
	stopTime = 0;
}

int readReg24(uint8_t address) {
	int value = 0;
	SPI.transfer(address);

	//  These values are 24-bit and we're reading them into a 32-bit
	//  variable.
	//
	//  This does some fancy pointer games to read it into the int starting
	//  at the 2nd byte in rather than the 1st.  That should get the correct
	//  2-4bit value read.
	SPI.transfer((uint8_t *) &value + 1, 3);

	return value;
}

// Read TDC for channel
long int tdc7200Channel::read() {
	//  Start a SPI transaction
	//  Max speed for the tdc7200 is 20MHz
	//  CPOL = 0; CPHA = 0
	SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
	digitalWrite(CSB, LOW);

	time1Result = readReg24(TIME1);
  time2Result  = readReg24(TIME2);
	clock1Result = readReg24(CLOCK_COUNT1);
	cal1Result = readReg24(CALIBRATION1);
  cal2Result = readReg24(CALIBRATION2);
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
  return tempu32;
	digitalWrite(CSB, HIGH);
	SPI.endTransaction();

  	return (long)tempu32;
}

// Enable next measurement cycle
void tdc7200Channel::ready_next() {
    // needs to set the enable bit (START_MEAS in CONFIG1)
    write(CONFIG1, 0x03);  // measurement mode 2 ('01')
}

// Calculate and print time interval to serial
void output_ti() {
}

void tdc7200Channel::write(byte address, byte value) {
  // take the chip select low to select the device:
  	SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
	digitalWrite(CSB, LOW);

	SPI.transfer16(address << 8 | value);

	digitalWrite(CSB, HIGH);
	SPI.endTransaction();
}

/* byte readTDC7200(struct chanTDC7200 *channel, byte address) {
  byte inByte = 0;

  // take the chip select low to select the device:
  digitalWrite(channel->CSB, LOW);

  SPI.transfer(address);
  inByte = SPI.transfer(0x00);

  digitalWrite(channel->CSB, HIGH);

  return inByte;
} */

