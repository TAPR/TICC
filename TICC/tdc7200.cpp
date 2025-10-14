// tdc7200 - read/write/setup TDC7200 chip

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>           // define unint16_t, uint32_t
#include <SPI.h>              // SPI support

#include "board.h"            // Arduino pin definitions
#include "config.h"           // config and eeprom
#include "tdc7200.h"          // TDC registers and structures
#include "timing_test.h"      // Performance timing configuration

extern config_t config;
extern int64_t PICTICK_PS; 
extern int64_t CLOCK_PERIOD;
extern int16_t CAL_PERIODS;

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
  byte CALIBRATION2_PERIODS = 0x80;  // default to 20 periods
  byte AVG_CYCLES, NUM_STOP;
   
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

  NUM_STOP = 0x00;    // default 0x00 for 1 stop; 0x01 for 2 stops

  config_byte2 = CALIBRATION2_PERIODS | AVG_CYCLES | NUM_STOP;

  write(CONFIG2, config_byte2);

  // enable interrupts:
  // 0x01 new measurement, 0x02 COARSE_OVF, 0x04 CLOCK_OVF 
  write(INT_MASK, 0x07);           // default 0x07 

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
  if ( (ID == '0') && (config.START_EDGE[0] == 'F') ) {
    START_EDGE = 0x08;
    }
  if ( (ID == '1') && (config.START_EDGE[1] == 'F') ) {
    START_EDGE = 0x08;
    }

  byte RESERVED;              // high bit for MEASUREMENT MODE; reserved
  byte MEASURE_MODE = 0x02;   // 0x00 for mode 1, 0x02 for mode 2
  byte START_MEAS = 0x01;     // 0x01 to start measurement, 0x00 for no effect

  config_byte1 = FORCE_CAL | PARITY_EN | TRIGG_EDGE | STOP_EDGE | \
            START_EDGE | MEASURE_MODE | START_MEAS;

  // ack all existing interrupt conditions
  tdc_ack_int();
  }

// Acknowledge interrupts
void tdc7200Channel::tdc_ack_int() {
  uint8_t intstat = readReg8(INT_STATUS);
  write(INT_STATUS, intstat);
}

// Enable next measurement cycle
void tdc7200Channel::ready_next() {
#ifdef TIMING_TEST_READY_NEXT
  TIMING_PIN_HIGH;
#endif
  write(CONFIG1, config_byte1);
#ifdef TIMING_TEST_READY_NEXT
  TIMING_PIN_LOW;
#endif
  }

// Flush partial measurements and reset TDC7200 state
void tdc7200Channel::flush_and_reset() {
  // Acknowledge any pending interrupts to clear them
  tdc_ack_int();
  
  // Stop current measurement by clearing START_MEAS bit
  byte stop_config = config_byte1 & ~0x01;  // Clear START_MEAS bit
  write(CONFIG1, stop_config);
  
  // Wait a moment for any in-progress measurement to complete
  delay(10);
  
  // Acknowledge interrupts again to clear any that occurred during stop
  tdc_ack_int();
  
  // Re-enable measurement for next cycle
  ready_next();

  // Reset channel state variables
  reset_channel_state();
}

// Reset channel state variables without hardware reset
void tdc7200Channel::reset_channel_state() {
  // Clear measurement data
  time1Result = 0;
  time2Result = 0;
  time3Result = 0;
  clock1Result = 0;
  cal1Result = 0;
  cal2Result = 0;
  
  // Clear timestamp data
  tof = 0;
  last_tof = 0;
  optimized_tof = 0;
  new_ts_ready = 0;
  timestamp.seconds = 0;
  timestamp.picos = 0;
  last_timestamp.seconds = 0;
  last_timestamp.picos = 0;
  
  // Reset coarse-time cache
  last_picstop = 0;
  cached_sec = 0;
  cached_rem_ticks = 0;
  
  
  // Note: We deliberately do NOT reset totalize counter or PICstop
  // as these should maintain continuity across config changes
}

// Read TDC - optimized inline calculation for maximum throughput
int64_t tdc7200Channel::read() {
#if defined(TIMING_TEST_SPI_READS) || defined(TIMING_TEST_FULL_READ)
  TIMING_PIN_HIGH;
#endif
  
  // Read all measurement data once
  time1Result = readReg24(TIME1);         // START to next 100ns tick
  time2Result  = readReg24(TIME2);        // 100ns tick to STOP
  clock1Result = readReg24(CLOCK_COUNT1); // number of 100ns ticks
  cal1Result = readReg24(CALIBRATION1);   // value of 1 cal cycle
  cal2Result = readReg24(CALIBRATION2);   // value of CAL_PERIODS cycle
  
#ifdef TIMING_TEST_SPI_READS
  TIMING_PIN_LOW;  // End timing for just SPI reads
#endif
  
  // Optimized inline TOF calculation for maximum performance
  // Convert to signed for calculations
  int32_t time1_s = (int32_t)time1Result;
  int32_t time2_s = (int32_t)time2Result;
  int32_t clk1_s  = (int32_t)clock1Result;
  int32_t cal1_s  = (int32_t)cal1Result;
  int32_t cal2_s  = (int32_t)cal2Result;

  if (fixed_time2) {
    time2_s = (int32_t)fixed_time2;
  }

  // Base tof from 100 ns ticks: clk1 * 100,000 ps
  // clk1 is typically small (33-996 from test data), so fits in int32
  int32_t tof_base = clk1_s * (int32_t)CLOCK_PERIOD;

  // Apply fudge (typically small)
  tof_base -= (int32_t)fudge;

  // calCount calculation - use 64-bit only for the large product, then narrow
  int32_t cal_diff = cal2_s - cal1_s;
  int32_t scale = 1000000 - time_dilation;  // typically 997500
  int32_t denom = (int32_t)(CAL_PERIODS - 1);  // typically 19

  if (denom == 0) denom = 1;

  // Only use 64-bit for the product, then divide and narrow
  int64_t cal_prod = (int64_t)cal_diff * (int64_t)scale;
  int64_t calCount = (cal_prod + denom / 2) / denom;  // rounded division
  if (calCount < 1) calCount = 1;

  // ring_ticks = time1 - time2 (32-bit safe)
  int32_t ring_ticks = time1_s - time2_s;

  // Optimized ring_ps calculation:
  // Instead of: normLSB = (CLOCK_PERIOD * 10^12) / calCount, then ring_ps = (normLSB * ring_ticks) / 10^6
  // Use: ring_ps = (ring_ticks * CLOCK_PERIOD * 10^6) / calCount
  // This avoids the 10^12 intermediate and keeps precision
  
  int64_t num = (int64_t)ring_ticks * (int64_t)CLOCK_PERIOD;
  int64_t ring_ps = 0;
  if (num != 0) {
    // Scale by 10^6 first, then divide
    num *= 1000000LL;
    ring_ps = (num + calCount / 2) / calCount;  // rounded division
  }

  // Final tof calculation
  int64_t tof64 = (int64_t)tof_base + ring_ps;

  // Clamp to documented bounds
  if (tof64 < 300000LL) tof64 = 300000LL;
  if (tof64 > 100300000LL) tof64 = 100300000LL;

  // Store result
  tof = (int32_t)tof64;
  optimized_tof = (int32_t)tof64;  // Keep for compatibility if needed
  
  // Ack all interrupts
  tdc_ack_int();
  
#ifdef TIMING_TEST_FULL_READ
  TIMING_PIN_LOW;  // End timing for full read() function
#endif
  
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
  uint32_t value = 0;

  // CSB needs to be toggled between 24-bit register reads
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(CSB, LOW);

  SPI.transfer(address & 0x1f);

  uint16_t msb = SPI.transfer(0x00);
  uint16_t mid = SPI.transfer(0x00);
  uint16_t lsb = SPI.transfer(0x00);

  value = ((uint32_t)msb << 16) + (mid << 8) + lsb;

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

// Stop TDC7200 measurements
void tdc7200Channel::stop_measurements() {
  // Stop current measurement by clearing START_MEAS bit
  byte stop_config = config_byte1 & ~0x01;  // Clear START_MEAS bit
  write(CONFIG1, stop_config);
}

// Start TDC7200 measurements
void tdc7200Channel::start_measurements() {
  // Start measurement by setting START_MEAS bit
  write(CONFIG1, config_byte1);
}

// Global channel management functions (moved from TICC.ino)

// Flush all channels and reset their state
void flush_all_channels() {
  for (size_t i = 0; i < 2; ++i) {  // 2 channels: A and B
    channels[i].flush_and_reset();
  }
}

// Stop measurements on all channels
void stop_all_measurements() {
  for (size_t i = 0; i < 2; ++i) {  // 2 channels: A and B
    channels[i].stop_measurements();
  }
}

// Start measurements on all channels
void start_all_measurements() {
  for (size_t i = 0; i < 2; ++i) {  // 2 channels: A and B
    channels[i].start_measurements();
  }
}
