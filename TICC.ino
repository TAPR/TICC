// TICC Time interval Counter based on TICC Shield using TDC7200
// version -0.5 6 March 2016
// Copyright John Ackermann N8UR 2016
// Licensed under BSD 2-clause license

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

// global vars
long long unsigned stop_time_0;
long long unsigned stop_time_1;
long result_0;
long result_1;

void setup() {
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
  
  TDC_setup(0);
  TDC_setup(1);
  delay(10);
  ready_next(0);
  ready_next(1);
  
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
    ready_next(0); // enable next measurement
  }
  if (INTB_1) {
    result_1 = TDC_calc(1);
    ready_next(1);
  }
   
   // if we have both channels, subtract channel 0 from channel 1, print result, and reset vars
   if (result_0 || result_1) { 
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
}

// Fetch and calculate results from TDC7200
int TDC_calc(int ch) {
}

// Enable next measurement cycle
void ready_next(int ch) {
}

// Calculate and print time interval to serial
void output_ti() {
}

