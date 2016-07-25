
// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.5 29 June 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Licensed under BSD 2-clause license

#include "TICC.h"     //Register and structure definitions

// the TI TDC7200 communicates using SPI, so include the library:
#include <SPI.h>

volatile unsigned long long coarseCount = 0;

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
// Using TDC7200 timing mode 1...
void setup() {
  int i;

  delay(1000);   // make sure power has stabilized

  
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
  byte RegAddr;
  byte RegResult;
  

/*  for(i = 0; i < ARRAY_SIZE(channels); ++i) {
    if(channels[i].STOP)
      channels[i].stopTime = coarseCount;

    if(channels[i].INTB) {
      channels[i].result = channels[i].calc();
      channels[i].ready_next();
    }
  }

   // if we have both channels, subtract channel 0 from channel 1, print result, and reset vars
  if (channels[0].result && channels[1].result) {
    output_ti();
    for(i = 0; i < ARRAY_SIZE(channels); ++i)
    	channels[i].reset();
  }
  */



for (int chan=0; chan<2; chan++) {
  

  // Read registers 00-09 one at a time.  Check defaults (page 24 of datasheet).

  Serial.print("Channel #");
  Serial.println(chan);
  
  for (int RegAddr=0; RegAddr<10; RegAddr++) {
    unsigned int RegResult = channels[chan].readReg8(RegAddr);
    Serial.print("RegAddr = ");
    Serial.print(RegAddr, HEX);
    Serial.print("   RegResult = ");
    Serial.println(RegResult, HEX);
  }

  // Read registers 10-xx one at a time. These are 24-bit registers, default to zero.

  for (int RegAddr=10; RegAddr<0x1c; RegAddr++) {
    unsigned int RegResult = channels[chan].readReg24(RegAddr);
    Serial.print("RegAddr = ");
    Serial.print(RegAddr, HEX);
    Serial.print("   RegResult = ");
    Serial.println(RegResult, HEX);
  }
}

  Serial.println("");
   
  delay(5000);  // wait for 5 second
  
}

// ISR for timer. NOTE: uint_64 rollover would take
// 62 million years at 100 usec interrupt rate.

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
  delay(1000);
  digitalWrite(ENABLE, HIGH);  // Needs a low-to-high transition to enable
  delay(10);
}

void tdc7200Channel::reset() {
	result = 0;
	stopTime = 0;
}

// Fetch and calculate results from TDC7200
int tdc7200Channel::calc() {
//    read();
    // calc the values (John...)
}

unsigned int tdc7200Channel::readReg24(uint8_t address) {
	unsigned int value = 0;

  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CSB, LOW);

  SPI.transfer(address & 0x1f);

	//  These values are 24-bit and we're reading them into a 32-bit
	//  variable.
	//
	//  This does some fancy pointer games to read it into the int starting
	//  at the 2nd byte in rather than the 1st.  That should get the correct
	//  2-4bit value read.
	SPI.transfer((uint8_t *) &value + 1, 3);
  digitalWrite(CSB, HIGH);
  SPI.endTransaction();

	return value;
}


// A general 24 bit read routine has replaced this in order to allow 
// access to more registers than these 3.
// Read TDC for channel
//void tdc7200Channel::read() {
	//  Start a SPI transaction
	//  Max speed for the tdc7200 is 20MHz
	//  CPOL = 0; CPHA = 0
//	SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
//	digitalWrite(CSB, LOW);

//	timeResult = readReg24(TIME1);
//	clockResult = readReg24(CLOCK_COUNT1);
//	calResult = readReg24(CALIBRATION1);

//	digitalWrite(CSB, HIGH);
//	SPI.endTransaction();

//  	return;
// }


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

// New routine for testing and debug. Read one TDC register (8 bits).
unsigned int tdc7200Channel::readReg8(byte address) {

  unsigned char value = 0;
  
  // The upper two bits of address determine mode.
  // A7 = 1 --> address autoincrement mode. = 0 --> single address.
  // A6 = 1 --> write. = 0 --> read.
  // A5 = 1 --> not valid address range.
  
  //  Start a SPI transaction
  //  Max speed for the tdc7200 is 20MHz
  //  CPOL = 0; CPHA = 0

  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CSB, LOW);
  
  SPI.transfer(address & 0x1f);
  value = SPI.transfer(0x00);
  
  digitalWrite(CSB, HIGH);
  SPI.endTransaction();
  
  return value;
}

