// TICC Time interval Counter based on TICC Shield using TDC7200
// version -0.5 6 March 2016
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
const int ENABLE_0 = 13;
const int INTB_0 = 12;
const int CSB_0 = 11;
const int ENABLE_1 = 10;
const int INTB_1 = 9;
const int CSB_1 = 8;
const int STOP_0 = 7;
const int STOP_1 = 8;

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

// global vars
long long unsigned stop_time_0;
long long unsigned stop_time_1;
long result_0;
long result_1;

  int timeResult_0 = 0;
  int clockResult_0 = 0;
  int calResult_0 = 0;
  int timeResult_1 = 0;
  int clockResult_1 = 0;
  int calResult_1 = 0;

void setup() {
  // start the SPI library:
  SPI.begin();
  
  pinMode(int_pin, INPUT);
  
  pinMode(ENABLE_0,OUTPUT);
  pinMode(INTB_0,INPUT);
  pinMode(CSB_0,OUTPUT);
  pinMode(STOP_0,INPUT);
  pinMode(ENABLE_1,OUTPUT);
  pinMode(INTB_1,INPUT);
  pinMode(CSB_1,OUTPUT);
  pinMode(STOP_1,INPUT);
  
  attachInterrupt(interrupt, coarse_timer, RISING);
  
  TDC_setup_0();
  TDC_setup_1();
  delay(10);
  ready_next_0();
  ready_next_1();
  
  Serial.begin(115200);
  Serial.println("Starting...");
}

void loop() {
 
 if (STOP_0) {
   stop_time_0 = coarse_count; // capture time stamp of gated stop clock
 }
 if (STOP_1) {
    stop_time_1 = coarse_count;
 } 

  if (INTB_0) { // channel 1 measurement complete
    result_0 = TDC_calc(0); // get registers and calc TOF
    ready_next_0(); // enable next measurement
  }
  if (INTB_1) {
    result_1 = TDC_calc(1);
    ready_next_1();
  }
   
   // if we have both channels, subtract channel 0 from channel 1, print result, and reset vars
   if (result_0 && result_1) { 
    output_ti();
    result_0 = 0;
    result_1 = 0;
    stop_time_0 = 0;
    stop_time_1 = 0;
   } 
  
}  

// ISR for timer NOTE: need to deal with rollover
void coarse_timer() {
  coarse_count++;
}

// Initial config for TDC7200
int TDC_setup( int ch) {
  return 0;
}

int TDC_setup_0() {
  digitalWrite(ENABLE_0, HIGH);
}

int TDC_setup_1() {
  digitalWrite(ENABLE_1, HIGH);
}

// Fetch and calculate results from TDC7200
int TDC_calc(int ch) {
  if (ch == 0) {
    TDC_read_0();
    // calc the values (John...)
  }
  else if (ch == 1){
    TDC_read_1();
    // calc the values (John...)
  }
}

void TDC_read_0() {
  byte inByte = 0;
  int timeResult = 0;
  int clockResult = 0;
  int calResult = 0;

  // read the TIMER1 register
    // take the chip select low to select the device:
  digitalWrite(CSB_0, LOW);

  SPI.transfer(TIME1);
  inByte = SPI.transfer(0x00);
  timeResult |= inByte;
  inByte = SPI.transfer(0x00);
  timeResult = timeResult<<8 | inByte;
  inByte = SPI.transfer(0x00);
  timeResult = timeResult<<8 | inByte;
  
  digitalWrite(CSB_0, HIGH);

// read the CLOCK1 register
    // take the chip select low to select the device:
  digitalWrite(CSB_0, LOW);

  SPI.transfer(CLOCK_COUNT1);
  inByte = SPI.transfer(0x00);
  clockResult |= inByte;
  inByte = SPI.transfer(0x00);
  clockResult = clockResult<<8 | inByte;
  inByte = SPI.transfer(0x00);
  clockResult = clockResult<<8 | inByte;
  
  digitalWrite(CSB_0, HIGH);

// read the CALIBRATION1 register
   // take the chip select low to select the device:
  digitalWrite(CSB_0, LOW);

  SPI.transfer(CALIBRATION1);
  inByte = SPI.transfer(0x00);
  calResult |= inByte;
  inByte = SPI.transfer(0x00);
  calResult = calResult<<8 | inByte;
  inByte = SPI.transfer(0x00);
  calResult = calResult<<8 | inByte;
  
  digitalWrite(CSB_0, HIGH);

  timeResult_0 = timeResult;
  clockResult_0 = clockResult;
  calResult_0 = calResult;

  return;  
}
void TDC_read_1() {
  byte inByte = 0;
  int timeResult = 0;
  int clockResult = 0;
  int calResult = 0;

  // read the TIMER1 register
    // take the chip select low to select the device:
  digitalWrite(CSB_1, LOW);

  SPI.transfer(TIME1);
  inByte = SPI.transfer(0x00);
  timeResult |= inByte;
  inByte = SPI.transfer(0x00);
  timeResult = timeResult<<8 | inByte;
  inByte = SPI.transfer(0x00);
  timeResult = timeResult<<8 | inByte;
  
  digitalWrite(CSB_1, HIGH);

// read the CLOCK1 register
    // take the chip select low to select the device:
  digitalWrite(CSB_1, LOW);

  SPI.transfer(CLOCK_COUNT1);
  inByte = SPI.transfer(0x00);
  clockResult |= inByte;
  inByte = SPI.transfer(0x00);
  clockResult = clockResult<<8 | inByte;
  inByte = SPI.transfer(0x00);
  clockResult = clockResult<<8 | inByte;
  
  digitalWrite(CSB_1, HIGH);

// read the CALIBRATION1 register
   // take the chip select low to select the device:
  digitalWrite(CSB_1, LOW);

  SPI.transfer(CALIBRATION1);
  inByte = SPI.transfer(0x00);
  calResult |= inByte;
  inByte = SPI.transfer(0x00);
  calResult = calResult<<8 | inByte;
  inByte = SPI.transfer(0x00);
  calResult = calResult<<8 | inByte;
  
  digitalWrite(CSB_1, HIGH);

  timeResult_1 = timeResult;
  clockResult_1 = clockResult;
  calResult_1 = calResult;

  return;  
}

// Enable next measurement cycle
void ready_next_0() {
  // needs to set the enable bit (START_MEAS in CONFIG1)
  writeTDC7200_0(CONFIG1, 0x03);  // measurement mode 2 ('01'), 
}
void ready_next_1() {
  // needs to set the enable bit (START_MEAS in CONFIG1)
  writeTDC7200_1(CONFIG1, 0x03);  // measurement mode 2 ('01'), 
}

// Calculate and print time interval to serial
void output_ti() {
}

void writeTDC7200_0(byte address, byte value) {
  // take the chip select low to select the device:
  digitalWrite(CSB_0, LOW);

  SPI.transfer(address);
  SPI.transfer(value);
  
  digitalWrite(CSB_0, HIGH);
}
void writeTDC7200_1(byte address, byte value) {
  // take the chip select low to select the device:
  digitalWrite(CSB_1, LOW);

  SPI.transfer(address);
  SPI.transfer(value);
  
  digitalWrite(CSB_1, HIGH);
}

byte readTDC7200_0(byte address) {
  byte inByte = 0;
    // take the chip select low to select the device:
  digitalWrite(CSB_0, LOW);

  SPI.transfer(address);
  inByte = SPI.transfer(0x00);
  
  digitalWrite(CSB_0, HIGH);

  return inByte;
}

byte readTDC7200_1(byte address) {
  byte inByte = 0;
    // take the chip select low to select the device:
  digitalWrite(CSB_1, LOW);

  SPI.transfer(address);
  inByte = SPI.transfer(0x00);
  
  digitalWrite(CSB_1, HIGH);

  return inByte;
}

