
// TICC Time interval Counter based on TICC Shield using TDC7200
// version -0.5 6 March 2016
// refactor to rename Ch0 to ChA and Ch 1 to ChB, combine duplicate
// subroutiones
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Licensed under BSD 2-clause license


#include "TICC.h"     //Register and structure definitions

// the TI TDC7200 communicates using SPI, so include the library:
#include <SPI.h>

volatile unsigned long long coarseCount = 0;

// Enumerate the TDC7200 channel structures
static chanTDC7200 ChanA = {
  .ID = 'A',
  .ENABLE = ENABLE_A,
  .INTB = INTB_A,
  .CSB = CSB_A,
  .STOP = STOP_A };

static chanTDC7200 ChanB = {
  .ID = 'B',
  .ENABLE = ENABLE_B,
  .INTB = INTB_B,
  .CSB = CSB_B,
  .STOP = STOP_B };

chanTDC7200* ChA = &ChanA;  // pointer to Channel A
chanTDC7200* ChB = &ChanB;


// properties of the TDC7200 chip:
// Most Significant Bit is clocked first (MSBFIRST)
// clock is low when idle
// data is clocked on the rising edge of the clock (seems to be SPI_MODE0)
// max clock speed: 20 mHz
// Using TDC7200 timing mode 1...


void setup() {
  // start the SPI library:
  SPI.begin();
  
  pinMode(intPin, INPUT);
  
  pinMode(ChA->ENABLE,OUTPUT);
  pinMode(ChA->INTB,INPUT);
  pinMode(ChA->CSB,OUTPUT);
  pinMode(ChA->STOP,INPUT);
  pinMode(ChB->ENABLE,OUTPUT);
  pinMode(ChB->INTB,INPUT);
  pinMode(ChB->CSB,OUTPUT);
  pinMode(ChB->STOP,INPUT);
  
  attachInterrupt(interrupt, coarseTimer, RISING);
  
  TDC_setup(ChA);
  TDC_setup(ChB);
  delay(10);
  ready_next(ChA);
  ready_next(ChB);
  
  Serial.begin(115200);
  Serial.println("Starting...");
}

void loop() {
 
  if (ChA->STOP) 
    ChA->stopTime = coarseCount; // capture time stamp of gated stop clock
 
  if (ChB->STOP) 
    ChB->stopTime = coarseCount;

  if (ChA->INTB) { // channel measurement complete
    ChA->result = TDC_calc(ChA); // get registers and calc TOF
    ready_next(ChA); // enable next measurement
  }
  
  if (ChB->INTB) {
    ChB->result = TDC_calc(ChB);
    ready_next(ChB);
  }
   
   // if we have both channels, subtract channel 0 from channel 1, print result, and reset vars
   if (ChanA.result && ChanB.result) { 
    output_ti();
    ChA->result = 0;
    ChB->result = 0;
    ChA->stopTime = 0;
    ChB->stopTime = 0;
   } 
}  

// ISR for timer. NOTE: uint_64 rollover would take
// 62 million years at 100 usec interrupt rate.

void coarseTimer() {
  coarseCount++;
}

// Initial config for TDC7200

int TDC_setup(chanTDC7200* channel) {
  digitalWrite(channel->ENABLE, HIGH);
}


// Fetch and calculate results from TDC7200
int TDC_calc(chanTDC7200* channel) {
    TDC_read(channel);
    // calc the values (John...)
}

// Read TDC for channel
void TDC_read(chanTDC7200* channel) {

  byte inByte = 0;
  int timeResult = 0;
  int clockResult = 0;
  int calResult = 0;

  // read the TIMER1 register
    // take the chip select low to select the device:
  digitalWrite(channel->CSB, LOW);

  SPI.transfer(TIME1);
  inByte = SPI.transfer(0x00);
  timeResult |= inByte;
  inByte = SPI.transfer(0x00);
  timeResult = timeResult<<8 | inByte;
  inByte = SPI.transfer(0x00);
  timeResult = timeResult<<8 | inByte;
  
  digitalWrite(channel->CSB, HIGH);

// read the CLOCK1 register
    // take the chip select low to select the device:
  digitalWrite(channel->CSB, LOW);

  SPI.transfer(CLOCK_COUNT1);
  inByte = SPI.transfer(0x00);
  clockResult |= inByte;
  inByte = SPI.transfer(0x00);
  clockResult = clockResult<<8 | inByte;
  inByte = SPI.transfer(0x00);
  clockResult = clockResult<<8 | inByte;
  
  digitalWrite(channel->CSB, HIGH);

// read the CALIBRATION1 register
   // take the chip select low to select the device:
  digitalWrite(channel->CSB, LOW);

  SPI.transfer(CALIBRATION1);
  inByte = SPI.transfer(0x00);
  calResult |= inByte;
  inByte = SPI.transfer(0x00);
  calResult = calResult<<8 | inByte;
  inByte = SPI.transfer(0x00);
  calResult = calResult<<8 | inByte;
  
  digitalWrite(channel->CSB, HIGH);

  channel->timeResult = timeResult;
  channel->clockResult = clockResult;
  channel->calResult = calResult;
  return;  
}


// Enable next measurement cycle
void ready_next(chanTDC7200* channel) {
  // needs to set the enable bit (START_MEAS in CONFIG1)
    writeTDC7200(channel, CONFIG1, 0x03);  // measurement mode 2 ('01')
}

// Calculate and print time interval to serial
void output_ti() {
}

void writeTDC7200(chanTDC7200* channel, byte address, byte value) {

  // take the chip select low to select the device:
  digitalWrite(channel->CSB, LOW);

  SPI.transfer(address);
  SPI.transfer(value);
  
  digitalWrite(channel->CSB, HIGH);
}


byte readTDC7200(chanTDC7200* channel, byte address) {
  byte inByte = 0;

    // take the chip select low to select the device:
  digitalWrite(channel->CSB, LOW);

  SPI.transfer(address);
  inByte = SPI.transfer(0x00);
  
  digitalWrite(channel->CSB, HIGH);

  return inByte;
}


