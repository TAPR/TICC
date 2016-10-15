// tdc7200.cpp -- chip functions
// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>           // define unint16_t, uint32_t
#include "ticc.h"             // general config
#include "misc.h"             // random functions
#include "board.h"            // Arduino pin definitions
#include "config.h"           // config and eeprom
#include "tdc7200.h"          // TDC registers and structures
#include "ticc_SPI.h"         // SPI read/write
#include "UserConfig.h"       // user configuration of TICC


extern const int16_t CAL_PERIODS;
extern const int64_t CLOCK_PERIOD;

// Constructor
tdc7200Channel::tdc7200Channel(char id, int enable, int intb, int csb, int stop):
        ID(id), ENABLE(enable), INTB(intb), CSB(csb), STOP(stop) {

        pinMode(ENABLE,OUTPUT);
        pinMode(INTB,INPUT);
        pinMode(CSB,OUTPUT);
        pinMode(STOP,INPUT);
};


// Initial config for TDC7200
void tdc7200Channel::tdc_setup() {
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
  
  AVG_CYCLES = 0x00;  // default 0x00 for 1 measurement cycle
  NUM_STOP = 0x01;    // default 0x00 for 1 stop; 0x01 for 2 stops

  reg_byte = CALIBRATION2_PERIODS | AVG_CYCLES | NUM_STOP;

  boolean state = true;
  boolean last_state = true;
  while (state || last_state) { // wait until COARSE falling edge so we start in same phase, maybe
    last_state = state;
    state = digitalRead(COARSEint);
    }
  SPI_write(CONFIG2, reg_byte, CSB);
  // enable interrupts -- 0x01 new measurement, 0x02 COARSE_OVF, 0x04 CLOCK_OVF 
  SPI_write(INT_MASK, 0x04, CSB);           // default 0x07 
  // coarse counter overflow occurs when timeN/63 > mask
  //write(COARSE_CNTR_OVF_H, 0x04);  // default is 0xFF 
  //write(COARSE_CNTR_OVF_L, 0x00);  // default is 0xFF
  //clock counter overflow occurs when clock_countN > mask
  SPI_write(CLOCK_CNTR_OVF_H, 0x05, CSB);     // default is 0xFF
  SPI_write(CLOCK_CNTR_OVF_L, 0x00, CSB);     // default is 0xFF
  
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
 SPI_write(CONFIG1, reg_byte, CSB);     
}


// Read TDC for channel
int64_t tdc7200Channel::read() {
  int64_t normLSB;
  int64_t calCount;
  int64_t ring_ticks;
  int64_t ring_ps;
  int64_t tof; 
  
  time1Result = SPI_readReg24(TIME1, CSB);
  time2Result  = SPI_readReg24(TIME2, CSB);
  clock1Result = SPI_readReg24(CLOCK_COUNT1, CSB);
  cal1Result = SPI_readReg24(CALIBRATION1, CSB);
  cal2Result = SPI_readReg24(CALIBRATION2, CSB);

  #ifdef PRINT_REG_RESULTS
    if (totalize > 1) {  // first few readings likely to be bogus
    Serial.print(time1Result);Serial.print(" ");
    Serial.print(time2Result);Serial.print(" ");
    Serial.print(clock1Result);Serial.print(" ");
    Serial.print(cal1Result);Serial.print(" ");
    Serial.print(cal2Result);Serial.print(" ");
    }
  #endif

     
  // Datasheet says:
  // normLSB = CLOCKPERIOD / calCount   (for 10 MHz clock, CLOCK_PERIOD = 1e5 ps)
  // calCount =  (cal2Result - cal1Result) / (cal2Periods - 1)
  // tof = normLSB(time1Result - time2Result) + (clock1Result)(CLOCK_PERIOD)
  //
  // But these steps truncate ringps at 1ps. Since normLSB is multiplied
  // by up to a few thousand ringticks, the truncation error is 
  // multiplied as well.  So use mult/div to improve resolution 

 
  tof = (int64_t)(clock1Result * CLOCK_PERIOD);
  //  tof -= (int64_t)FUDGE0; // subtract delay due to silicon
  
  // calCount *= 10e6
  // time_dilation adjusts for non-linearity at 100ns overflow
  calCount = ((int64_t)(cal2Result - cal1Result) * (int64_t)(1000000 - time_dilation) ) / (int64_t)(CAL_PERIODS - 1); 

  // if FIXED_TIME2 is set, override time2Result with that value
  if (fixed_time2) {
    time2Result = (int64_t)fixed_time2;
  }
  
  // normLSB *= 10e6, but we've already multiplied the divisor so we need to double the 0s here
  normLSB = ( (int64_t)CLOCK_PERIOD * (int64_t)1000000000000 ) / (int64_t)calCount;
  ring_ticks = (int64_t)time1Result - (int64_t)time2Result;
 
  // ring_ps *= 10e-6 to get rid of earlier scaling
  ring_ps = ((int64_t)normLSB * (int64_t)ring_ticks) / (int64_t)1000000;
  
  tof += (int64_t)ring_ps;
  
//  print_unsigned_picos_as_seconds(tof);Serial.print(" ");  
//  Serial.print("normLSB: ");print_unsigned_picos_as_seconds(normLSB);Serial.print(" ");
//  Serial.print("ring_ticks: ");print_signed_picos_as_seconds(ring_ticks);Serial.print(" ");
//  print_signed_picos_as_seconds(ring_ps);Serial.print(" ");
//  Serial.print("ring_ps: ");print_signed_picos_as_seconds(ring_ps);Serial.print(" ");
//  Serial.print("clock1Result: ");Serial.print(clock1Result);Serial.print(" ");  
//  Serial.print("tof: ");print_unsigned_picos_as_seconds(tof);Serial.print(" ");
  
  return (int64_t)tof;
}

