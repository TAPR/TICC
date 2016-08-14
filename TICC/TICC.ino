// TICC.ino - master sketch file

// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.70 -- 14 August 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Tom McDermott N5EG 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

/*******************************************************************************
 Set these constants to match your hardware configuration
*******************************************************************************/
#define CLOCK_FREQ       (uint32_t) 1e7      // Reference Clock frequency in Hz
#define CAL_PERIODS      (uint16_t) 20       // Can only be 2, 10, 20, or 40.
#define PICTICK_PS       (uint32_t) 1e8      // 100 uS PICDIV strap is 1e8
#define FUDGE0           (uint64_t) 0        // fudge for system delays, in picoseconds
#define TIME_DILATION    (int64_t)  2000     // multiplier for normLSB;
#define FIXED_TIME2      (int64_t)  1125     // If 0, use measured time2Result.  Otherwise replace
                                             // with this value.  Nominal on boardc1/chA is 1125.
/*****************************************************************************/

//#define DETAIL_TIMING   // if enabled, prints execution time

#include <stdint.h>   // define unint16_t, uint32_t
#include <SPI.h>      // SPI support
#include "TICC.h"     // Register and structure definitions

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
  
  // start the serial library
  Serial.begin(115200);
  // start the SPI library:
  SPI.begin();
   
  PICcount = 0;
  pinMode(interruptPin, INPUT);
  // Don't know why, but need to catch on falling to be in sync
  attachInterrupt(digitalPinToInterrupt(interruptPin), coarseTimer, FALLING);
  
  for(i = 0; i < ARRAY_SIZE(channels); ++i) {
    channels[i].setup();
    channels[i].ready_next();
  }
 
  Serial.println("");
  Serial.println("# timestamp (seconds)");
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
                 
        ts = (uint64_t)(PICstop * PICTICK_PS) - tof;
        
        if (totalize > 1) {  // first few readings likely to be bogus
          print_picos_as_seconds(ts);
          Serial.print( "  CH: ");Serial.println(channels[i].ID);
        }
        
        totalize++; // number of measurements
        
#ifdef DETAIL_TIMING      
        end_micros = micros();         
        Serial.print(" execution time (us): ");
        Serial.print(end_micros - start_micros);
#endif

    } // if
  } // for
} // loop

/**************************************************************************************/
 
// ISR for timer. 
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
  byte CALIBRATION2_PERIODS, AVG_CYCLES, NUM_STOP, reg_byte;
  
  digitalWrite(ENABLE, LOW);
  delay(5);  
  digitalWrite(ENABLE, HIGH);  // Needs a low-to-high transition to enable
  delay(5);  // 1.5ms minimum recommended to allow chip LDO to stabilize

  switch (CAL_PERIODS) {
    case  2: CALIBRATION2_PERIODS = 0x00; break;
    case 10: CALIBRATION2_PERIODS = 0x40; break;
    case 20: CALIBRATION2_PERIODS = 0x80; break;
    case 40: CALIBRATION2_PERIODS = 0xC0; break;
  }
  
  AVG_CYCLES = 0x00;  // 0x00 for 1 measurement cycle
  NUM_STOP = 0x01;    // SHOULD BE 0x00 for 1 stop but that doesn't work

  reg_byte = CALIBRATION2_PERIODS | AVG_CYCLES | NUM_STOP;
  write(CONFIG2, reg_byte);  
 
 
 // write(INT_MASK, 0x01);  // was 0x01 disable clock overflow interrupts, allow only measurement interrupt 
 // write(CLOCK_CNTR_STOP_MASK_H, 0x00); // was 0x00
 // write(CLOCK_CNTR_STOP_MASK_L, 0x01); // was 0x00 // hold off no clocks before enabling STOP
 write(COARSE_CNTR_OVF_H, 0x0F);  //was 0xF0  -- only doing this because STOP doesn't raise INTB 
 write(COARSE_CNTR_OVF_L, 0x00);
 //write(CLOCK_CNTR_OVF_H, 0xF0);   // was 0xF0
 //write(CLOCK_CNTR_OVF_L, 0x00);  // was 0x00
  
}

// Enable next measurement cycle
void tdc7200Channel::ready_next() {
// needs to set the enable bit (START_MEAS in CONFIG1)
// clears interrupt bits
 byte FORCE_CAL = 0x80;      // 0x80 forces cal even if timeout; 0x00 means no cal if measurement interrupted 
 byte PARITY_EN = 0x00;      // parity on would be 0x40
 byte TRIGG_EDGE = 0x00;     // TRIGG rising edge; falling edge would be 0x20
 byte STOP_EDGE = 0x00;      // STOP rising edge; falling edge would be 0x10
 byte START_EDGE = 0x00;     // START rising dege; falling edge would be 0x08
 byte RESERVED;              // high bit for MEASUREMENT MODE; reserved, would add 0x04
 byte MEASURE_MODE = 0x02;   // Measurement mode; 0x00 for mode 1, 0x02 for mode 2
 byte START_MEAS = 0x01;     // Start measurement; 0x01 to start, 0x00 for no effect
 byte reg_byte;
 
 reg_byte = FORCE_CAL | PARITY_EN | TRIGG_EDGE | STOP_EDGE | START_EDGE | MEASURE_MODE | START_MEAS;   
 write(CONFIG1, reg_byte);     
}


// Read TDC for channel
  unsigned long long tdc7200Channel::read() {
  int64_t normLSB;
  int64_t calCount;
  int64_t ring_ticks;
  int64_t ring_ps;
  uint64_t tof; 
  
  time1Result = readReg24(TIME1);
  time2Result  = readReg24(TIME2);
  clock1Result = readReg24(CLOCK_COUNT1);
  cal1Result = readReg24(CALIBRATION1);
  cal2Result = readReg24(CALIBRATION2);
  
  // Datasheet says:
  // normLSB = CLOCKPERIOD / calCount   (for 10 MHz clock, CLOCK_PERIOD = 1e5 ps)
  // calCount =  (cal2Result - cal1Result) / (cal2Periods - 1)
  // tof = normLSB(time1Result - time2Result) + (clock1Result)(CLOCK_PERIOD)
  //
  // But these steps truncate ringps at 1ps. But normLSB is multiplied
  // by up to a few thousand ringticks, so the truncation error is 
  // multiplied as well.  So we use mult/div to improve resolution 

  tof = (uint64_t)(clock1Result * CLOCK_PERIOD);
  //  tof -= (uint64_t)FUDGE0; // subtract delay due to silicon
  
  // calCount *= 10e6
  calCount = ((int64_t)(cal2Result - cal1Result) * (int64_t)(1000000 - TIME_DILATION)) / (int64_t)(CAL_PERIODS - 1); 

  // maybe time2Result doesn't add value, so we substitute a fixed value
  if (FIXED_TIME2) {
    time2Result = FIXED_TIME2;
  }
  
  // normLSB *= 10e6 -- remember that we've already multiplied the divisor so we use twice that here
  normLSB = ((int64_t)CLOCK_PERIOD *(int64_t)1000000000000) / calCount;
  ring_ticks = (int64_t)time1Result - (int64_t)time2Result;
 
   // ring_ps *= 10e-6 to get rid of earlier scaling
  ring_ps = (normLSB * ring_ticks) / (int64_t)1000000;
  
  tof += (uint64_t)ring_ps;

  
//  Serial.print("cal1Result: ");Serial.print(cal1Result);Serial.print(" ");
//  Serial.print("cal2Result: ");Serial.print(cal2Result);Serial.print(" ");
//  Serial.print("calCount: ");print_picos_as_seconds(calCount);Serial.print(" ");
//  Serial.print("normLSB: ");print_picos_as_seconds(normLSB);Serial.print(" ");
//  Serial.print("ring_ticks: ");print_signed_picos_as_seconds(ring_ticks);Serial.print(" ");
//  print_signed_picos_as_seconds(ring_ps);Serial.print(" ");
//  Serial.print("ring_ps: ");print_signed_picos_as_seconds(ring_ps);Serial.print(" ");
//  Serial.print("clock1Result: ");Serial.print(clock1Result);Serial.print(" ");  
//  Serial.print("time1Result: ");Serial.print(time1Result);Serial.print(" ");  
//  Serial.print("time2Result: ");Serial.print(time2Result);Serial.print(" ");
//  Serial.print(time2Result);
//  Serial.print("time3Result: ");Serial.print(time3Result);Serial.print(" ");

//  Serial.print("tof: ");print_picos_as_seconds(tof);Serial.print(" ");
  
  
  return (uint64_t)tof;
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
  delayMicroseconds(5);
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

void print_signed_picos_as_seconds (int64_t x) {
  int64_t sec, secx, frac, frach, fracx, fracl;    
  sec = x / 1000000000000;
  secx = sec * 1000000000000;
  frac = x - secx;

  // break fractional part of seconds into two 6 digit numbers

  frach = frac / 1000000;
  fracx = frach * 1000000;
  fracl = frac - fracx;

  sprintf(str,"%ld",sec), Serial.print(str), Serial.print(".");
  sprintf(str, "%06ld", frach), Serial.print(str);
  sprintf(str, "%06ld", fracl), Serial.print(str);
  
} 




