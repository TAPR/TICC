// tdc7200 - read/write/setup TDC7200 chip

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>           // define unint16_t, uint32_t
#include <SPI.h>              // SPI support

#include "misc.h"             // random functions
#include "board.h"            // Arduino pin definitions
#include "config.h"           // config and eeprom
#include "tdc7200.h"          // TDC registers and structures

extern config_t config;
extern int64_t CLOCK_HZ;
extern int64_t PICTICK_PS; 
extern int64_t CLOCK_PERIOD;
extern int16_t CAL_PERIODS;
extern 

// Constructor
tdc7200Channel::tdc7200Channel(char id, int enable, int intb, int csb, int stop, int led) :
	ID(id), ENABLE(enable), INTB(intb), CSB(csb), STOP(stop), LED(led) {
	pinMode(ENABLE,OUTPUT);
	pinMode(INTB,INPUT);
	pinMode(CSB,OUTPUT);
	pinMode(STOP,INPUT);
  pinMode(LED, OUTPUT);
};

// TDC7200 configure
void tdc7200Channel::tdc_setup() {
  byte CALIBRATION2_PERIODS, AVG_CYCLES, NUM_STOP;
   
  digitalWrite(ENABLE, LOW);
  delay(5);  
  digitalWrite(ENABLE, HIGH);  // Needs a low-to-high transition to enable
  delay(5);  // 1.5ms minimum recommended to allow chip LDO to stabilize

  switch (CAL_PERIODS) { // convert actual cal periods to bitmask
    case  2: CALIBRATION2_PERIODS = 0x00; break;
    case 10: CALIBRATION2_PERIODS = 0x40; break;
    case 20: CALIBRATION2_PERIODS = 0x80; break; // default
    case 40: CALIBRATION2_PERIODS = 0xC0; break;
  }
  
  AVG_CYCLES = 0x00;  // default 0x00 for 1 measurement cycle

  // NOTE NOTE NOTE
  // The TDC chip is *supposed* to set INTB after seeing the
  // first stop edge.  That doesn't work -- the interrupt never happens.
  // Workaround is to set for two stops, and rely on the COUNTER_OVERFLOW
  // timer below.  It's ugly, and it slows down the maximum measurement
  //rate, but it works.
  NUM_STOP = 0x01;    // default 0x00 for 1 stop; 0x01 for 2 stops

  config_byte2 = CALIBRATION2_PERIODS | AVG_CYCLES | NUM_STOP;

  // TODO: check whether this is necessary; may be cruft from early testing
  boolean state = true;
  boolean last_state = true;
  while (state || last_state) { // catch COARSE falling edge tO align phase
    last_state = state;
    state = digitalRead(COARSEint);
    }
  write(CONFIG2, config_byte2);

  // enable interrupts:
  // 0x01 new measurement, 0x02 COARSE_OVF, 0x04 CLOCK_OVF 
  write(INT_MASK, 0x04);           // default 0x07 

  // coarse counter overflow occurs when timeN/63 > mask
  //write(COARSE_CNTR_OVF_H, 0x00);  // default is 0xFF 
  //write(COARSE_CNTR_OVF_L, 0x00);  // default is 0xFF

  // NOTE NOTE NOTE
  // See comment above -- setting CLOCK_CNTR_OVF to 0x05, 0x00 provides
  // a reasonable timeout after measurement completes.
  // When this occurs, INTB is set and the chip returns.
  
  //clock counter overflow occurs when clock_countN > mask
  write(CLOCK_CNTR_OVF_H, config.TIMEOUT);     // default is 0xFF
  write(CLOCK_CNTR_OVF_L, 0x00);     // default is 0xFF

  // now build config1 register byte
  // sets trigger edge
  // sets enable bit (START_MEAS in CONFIG1)
  // clears interrupt bits
      
  byte FORCE_CAL = 0x80;      // 0x80 forces cal; 0x00 means no cal interrupted 
  byte PARITY_EN = 0x00;      // parity on would be 0x40
  byte TRIGG_EDGE = 0x00;     // TRIGG rising edge; falling edge would be 0x20 -- not used
  byte STOP_EDGE = 0x00;      // STOP rising edge; falling edge would be 0x10
  
  // set trigger edge
  byte START_EDGE = 0x00;  // START default 0x00 for rising edge; falling edge would be 0x08
  if ( (ID == 'A') && (config.START_EDGE[0] == 'F') ) {
    START_EDGE = 0x08;
    }
  if ( (ID == 'B') && (config.START_EDGE[1] == 'F') ) {
    START_EDGE = 0x08;
    }

  byte RESERVED;              // high bit for MEASUREMENT MODE; reserved
  byte MEASURE_MODE = 0x02;   // 0x00 for mode 1, 0x02 for mode 2
  byte START_MEAS = 0x01;     // 0x01 to start measurement, 0x00 for no effect

  config_byte1 = FORCE_CAL | PARITY_EN | TRIGG_EDGE | STOP_EDGE | \
            START_EDGE | MEASURE_MODE | START_MEAS;   
  }

// Enable next measurement cycle
void tdc7200Channel::ready_next() {
  write(CONFIG1, config_byte1);
  }

// Read TDC
int64_t tdc7200Channel::read() {
  int64_t normLSB;
  int64_t calCount;
  int64_t ring_ticks;
  int64_t ring_ps;
  int64_t tof; 

  //*****************************************************************
  // Datasheet says:
  // normLSB = config.CLOCKPERIOD / calCount  // config.CLOCK_PERIOD = 1e5 ps
  // calCount =  (cal2Result - cal1Result) / (cal2Periods - 1)
  // tof = normLSB(time1Result - time2Result) + (clock1Result)(config.CLOCK_PERIOD)
  //
  // tof is the final result, a value from 0 to 99us 999ns 999ps.
  // It can never be larger because the STOP signal comes from the
  // 100us timer and the next edge will terminate the measurement.
  //
  // These steps truncate ringps at 1ps resolution. Since normLSB is 
  // multiplied by up to a few thousand ringticks, the truncation 
  // error is multiplied as well.  So use mult/div to improve resolution.
 
  // For reference, by default:
  // CLOCK_PERIOD is 1e5 picosecond
  // FUDGE0 is 0 
  // FIX_TIME2 is 0
  // CAL_PERIODS is 20
  // time_dilation is 2500 (adjusts for non-linearity)
  //*****************************************************************
  
  // these variables are all int32_t members of the tdc7200Channel class
  time1Result = readReg24(TIME1);         // START to next 100ns tick
  time2Result  = readReg24(TIME2);        // 100ns tick to STOP
  clock1Result = readReg24(CLOCK_COUNT1); // number of 100ns ticks
  cal1Result = readReg24(CALIBRATION1);   // value of 1 cal cycle
  cal2Result = readReg24(CALIBRATION2);   // value of CAL_PERIODS cycle
  
  tof = (int64_t)(clock1Result * CLOCK_PERIOD);
  tof -= (int64_t)fudge0; // subtract delay due to silicon
  
  // calCount *= 10e6; divide back later
  // time_dilation adjusts for non-linearity at 100ns overflow
  calCount = ((int64_t)(cal2Result - cal1Result) * (int64_t)(1000000 - time_dilation) ) / (int64_t)(CAL_PERIODS - 1); 

  // if FIXED_TIME2 is set, substitute measured time2Result (which should be a fixed value,
  // with any variation being noise, with the provided value.  This reduces jitter.
  if (fixed_time2) {
    time2Result = (int64_t)fixed_time2;
  }
  
  // normLSB *= 10e6, but we've already multiplied the divisor
  // above so we need to do 10e12 here
  normLSB = ( (int64_t)CLOCK_PERIOD * (int64_t)1000000000000 ) / (int64_t)calCount;

  ring_ticks = (int64_t)time1Result - (int64_t)time2Result;
 
  // ring_ps *= 10e-6 to get rid of earlier scaling
  ring_ps = ((int64_t)normLSB * (int64_t)ring_ticks) / (int64_t)1000000;
  
  tof += (int64_t)ring_ps;
 
  return (int64_t)tof;
}


/*************************************************************************
SPI read/write
*************************************************************************/
// Chip properties:
// Most Significant Bit is clocked first (MSBFIRST)
// clock is low when idle
// data is clocked on the rising edge of the clock (seems to be SPI_MODE0)
// max clock speed: 20 mHz

byte tdc7200Channel::readReg8(byte address) {
  byte inByte = 0;

  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  // take the chip select low to select the device:
  digitalWrite(CSB, LOW);

  SPI.transfer(address & 0x1f);
  inByte = SPI.transfer(0x00);

  digitalWrite(CSB, HIGH);
  SPI.endTransaction();

  return inByte;
}

uint32_t tdc7200Channel::readReg24(byte address) {
  uint32_t long value = 0;

  // CSB needs to be toggled between 24-bit register reads
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(CSB, LOW);

  SPI.transfer(address & 0x1f);

  uint16_t msb = SPI.transfer(0x00);
  uint16_t mid = SPI.transfer(0x00);
  uint16_t lsb = SPI.transfer(0x00);

  value = (msb << 16) + (mid << 8) + lsb;

  digitalWrite(CSB, HIGH);
  SPI.endTransaction();
  delayMicroseconds(5);
  return value;
}

void tdc7200Channel::write(byte address, byte value) {

  // take the chip select low to select the device:
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(CSB, LOW);

  // Force Address bit 6 to one for a write
  SPI.transfer16((address | 0x40) << 8 | value);

  digitalWrite(CSB, HIGH);
  SPI.endTransaction();
}
