
// TICC Time interval Counter based on TICC Shield using TDC7200
// version -0.5 6 March 2016
// refactor to rename Ch0 to ChA and Ch 1 to ChB, combine duplicate
// subroutiones
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Licensed under BSD 2-clause license

// the TI TDC7200 communicates using SPI, so include the library:
#include <SPI.h>

// set up for ISR
int interrupt = 1; // interrupt for Uno pin 3
int int_pin = 3; // physical pin
volatile unsigned long long coarse_count = 0;

// hardware connections to TDC2700 -- need to reassign these more logically (todo in Eagle)

struct chanTDC7200_t {
  const int ENABLE;
  const int INTB;
  const int CSB;
  const int STOP;
  int timeresult;
  int clockresult;
  int calresult;
  long result;
  long long unsigned stop_time;
};

static chanTDC7200_t ChanA = { 13, 12, 11, 6, 0, 0, 0, 0, 0 }; // I/O pins on Arduino
static chanTDC7200_t ChanB = { 10, 9, 8, 7, 0, 0, 0, 0, 0 };
chanTDC7200_t* ChA = &ChanA;  // pointer to Channel A
chanTDC7200_t* ChB = &ChanB;


// TDC7200 register addresses
const int CONFIG1 = 0x00;                 // default 0x00
const int CONFIG2 = 0x01;                 // default 0x40
const int INT_STATUS = 0x02;              // default 0x00
const int INT_MASK = 0x03;                // default 0x07
const int COARSE_CNTR_OVF_H = 0x04;       // default 0xff
const int COARSE_CNTR_OVF_L = 0x05;       // default 0xff
const int CLOCK_CNTR_OVF_H = 0x06;        // default 0xff
const int CLOCK_CNTR_OVF_L = 0x07;        // default 0xff
const int CLOCK_CNTR_STOP_MASK_H = 0x08;  // default 0xff
const int CLOCK_CNTR_STOP_MASK_L = 0x09;  // default 0xff
// gap from 0x0A thru 0x0F...
const int TIME1 = 0x10;                   // default 0x00_0000
const int CLOCK_COUNT1 = 0x11;            // default 0x00_0000
const int TIME2 = 0x12;                   // default 0x00_0000
const int CLOCK_COUNT2 = 0x13;            // default 0x00_0000
const int TIME3 = 0x14;                   // default 0x00_0000
const int CLOCK_COUNT3 = 0x15;            // default 0x00_0000
const int TIME4 = 0x16;                   // default 0x00_0000
const int CLOCK_COUNT4 = 0x17;            // default 0x00_0000
const int TIME5 = 0x18;                   // default 0x00_0000
const int CLOCK_COUNT5 = 0x19;            // default 0x00_0000
const int TIME6 = 0x1A;                   // default 0x00_0000
const int CALIBRATION1 = 0x1B;            // default 0x00_0000
const int CALIBRATION2 = 0x1C;            // default 0x00_0000

// properties of the TDC7200 chip:
// Most Significant Bit is clocked first (MSBFIRST)
// clock is low when idle
// data is clocked on the rising edge of the clock (seems to be SPI_MODE0)
// max clock speed: 20 mHz
// Using TDC7200 timing mode 1...


void setup() {
  // start the SPI library:
  SPI.begin();
  
  pinMode(int_pin, INPUT);
  
  pinMode(ChA->ENABLE,OUTPUT);
  pinMode(ChA->INTB,INPUT);
  pinMode(ChA->CSB,OUTPUT);
  pinMode(ChA->STOP,INPUT);
  pinMode(ChB->ENABLE,OUTPUT);
  pinMode(ChB->INTB,INPUT);
  pinMode(ChB->CSB,OUTPUT);
  pinMode(ChB->STOP,INPUT);
  
  attachInterrupt(interrupt, coarse_timer, RISING);
  
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
    ChA->stop_time = coarse_count; // capture time stamp of gated stop clock
 
  if (ChB->STOP) 
    ChB->stop_time = coarse_count;

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
    ChA->stop_time = 0;
    ChB->stop_time = 0;
   } 
}  

// ISR for timer. NOTE: uint_64 rollover would take
// 62 million years at 100 usec interrupt rate.

void coarse_timer() {
  coarse_count++;
}

// Initial config for TDC7200

int TDC_setup(chanTDC7200_t* channel) {
  digitalWrite(channel->ENABLE, HIGH);
}


// Fetch and calculate results from TDC7200
int TDC_calc(chanTDC7200_t* channel) {
    TDC_read(channel);
    // calc the values (John...)
}

// Read TDC for channel
void TDC_read(chanTDC7200_t* channel) {

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

  channel->timeresult = timeResult;
  channel->clockresult = clockResult;
  channel->calresult = calResult;
  return;  
}


// Enable next measurement cycle
void ready_next(chanTDC7200_t* channel) {
  // needs to set the enable bit (START_MEAS in CONFIG1)
    writeTDC7200(channel, CONFIG1, 0x03);  // measurement mode 2 ('01')
}

// Calculate and print time interval to serial
void output_ti() {
}

void writeTDC7200(chanTDC7200_t* channel, byte address, byte value) {

  // take the chip select low to select the device:
  digitalWrite(channel->CSB, LOW);

  SPI.transfer(address);
  SPI.transfer(value);
  
  digitalWrite(channel->CSB, HIGH);
}


byte readTDC7200(chanTDC7200_t* channel, byte address) {
  byte inByte = 0;

    // take the chip select low to select the device:
  digitalWrite(channel->CSB, LOW);

  SPI.transfer(address);
  inByte = SPI.transfer(0x00);
  
  digitalWrite(channel->CSB, HIGH);

  return inByte;
}


