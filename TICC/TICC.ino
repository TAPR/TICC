// TICC.ino - master sketch file
// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

const char SW_VERSION[17] = "0.79a";  // 3 October 2016
const char BOARD_SER_NUM[17] = "0123456789";  // how to set this for each board?

#if (defined(__GNUC__) || defined(__GNUG__))
  #pragma message("GCC compiler detected")
#endif
#pragma message("Compiler version: ")
#pragma message(__VERSION__)


//#define PRINT_REG_RESULTS // if enabled, prints time1, time2, clock1, cal1, cal2 before timestamp
//#define DETAIL_TIMING     // if enabled, prints execution time

#ifdef DETAIL_TIMING
  int32_t start_micros;
  int32_t end_micros;
#endif

#include <stdint.h>           // define unint16_t, uint32_t
#include <SPI.h>              // SPI support
#include <EnableInterrupt.h>  // use faster interrupt library
#include <EEPROM.h>           // read/write EEPROM
#include "TICC.h"             // register and structure definitions
#include "UserConfig.h"       // user configuration of TICC

// NOTE: changed from uint to int while working on TINT calc
volatile int64_t PICcount;
int64_t tint;
int64_t CLOCK_HZ;
int64_t PICTICK_PS; 
int64_t CLOCK_PERIOD;
int16_t CAL_PERIODS;

enum MeasureMode m;
config_t config;

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

  
  /*******************************************
   * Configuration read/change/store
  *******************************************/
  // if no config stored, or wrong version, restore from default
  if ( EEPROM.read(CONFIG_START) != config.EEPROM_VERSION) {
    eeprom_write_default(CONFIG_START);  
  }
  
  // read config and set global vars
  eeprom_read_config(CONFIG_START, config);
  CLOCK_HZ = config.CLOCK_HZ;
  CLOCK_PERIOD = (PS_PER_SEC/CLOCK_HZ);
  PICTICK_PS = config.PICTICK_PS;
  CAL_PERIODS = config.CAL_PERIODS;

  // NOTE NOTE NOTE -- need to figure out how to make enums work with config struct
  // T - timestamp; P - period; L - timelab; I - interval
  //  m = UserConfig();  

  PICcount = 0;
  pinMode(COARSEint, INPUT);
  pinMode(STOP_A, INPUT);
  pinMode(STOP_B, INPUT);
  pinMode(STOPAint, INPUT);
  pinMode(STOPBint, INPUT);
   
  for(i = 0; i < ARRAY_SIZE(channels); ++i) {
    channels[i].totalize = 0;
    channels[i].PICstop = 0;
    channels[i].tof = 0;
    channels[i].ts = 0;
    channels[i].time_dilation = config.TIME_DILATION[i];
    channels[i].fixed_time2 = config.FIXED_TIME2[i];
    channels[i].fudge0 = config.FUDGE0[i];
    
    channels[i].tdc_setup();
    channels[i].ready_next();
  }
  
  enableInterrupt(COARSEint, coarseTimer, FALLING);  // if using NEEDFORSPEED, don't declare this
  enableInterrupt(STOP_A, catch_stopA, RISING);
  enableInterrupt(STOP_B, catch_stopB, RISING);
  
  Serial.println("");
  Serial.println("");
  #ifdef PRINT_REG_RESULTS
    Serial.println("# time1 time2 clock1 cal1 cal2 timestamp");
  #else
    if (config.MODE == 1) {
      Serial.println("# time interval A->B (seconds)");
    } else {
      Serial.println("# timestamp (seconds)");
    }
  #endif
}  

void loop() {
  int i;
 
  for(i = 0; i < ARRAY_SIZE(channels); ++i) {
     
    // Only need to do anything if INTB is low, otherwise no work to do.
     if(digitalRead(channels[i].INTB)==0) {
       #ifdef DETAIL_TIMING
         start_micros = micros();
       #endif
       
       // read registers and calculate tof
       channels[i].last_tof = channels[i].tof;
       channels[i].tof = channels[i].read();
        
       // done with chip, so get ready for next reading
       channels[i].ready_next(); // Re-arm for next measurement, clear TDC INTB
      
       channels[i].ts = (channels[i].PICstop * (int64_t)PICTICK_PS) - channels[i].tof;
       channels[i].period = channels[i].ts - channels[i].last_ts;
       channels[i].totalize++;

#ifdef DETAIL_TIMING      
       end_micros = micros();         
       Serial.print(" execution time before output (us): ");
       Serial.print(end_micros - start_micros);
       Serial.println();
#endif

       // NOTE NOTE NOTE -- need to change to use enum
       // simple timestamp mode
       if ( (config.MODE == 0) && 
            (channels[i].totalize > 2) )
         {
         print_signed_picos_as_seconds(channels[i].ts);
         Serial.print( " ch");Serial.println(channels[i].ID);    
         }
       
       // time interval mode (A->B)
       if ( (config.MODE == 1) && 
            (channels[0].ts > 0) && 
            (channels[1].ts > 0) && 
            (channels[0].totalize > 2) ) 
         {
         print_signed_picos_as_seconds(channels[1].ts - channels[0].ts);
         Serial.println(" TI(A->B)");
         channels[0].last_ts = channels[0].ts;
         channels[0].ts = 0;
         channels[1].last_ts = channels[1].ts;
         channels[1].ts = 0;
         }          
       
       // period mode -- subtract last timestamp from current
       if ( (config.MODE == 2) && 
            (channels[i].totalize > 2) ) 
         {
         print_signed_picos_as_seconds(channels[i].ts - channels[i].last_ts);
         Serial.print( " ch");Serial.println(channels[i].ID);    
         }  
       
       // timelab mode -- output period data plus TI data
       if ( (config.MODE == 3) &&
            (channels[0].ts > 0) &&
            (channels[1].ts > 0) &&
            (channels[0].totalize > 2) ) {
         print_signed_picos_as_seconds(channels[0].ts);Serial.println(" chA");
         print_signed_picos_as_seconds(channels[1].ts);Serial.println(" chB");
         print_signed_picos_as_seconds( (channels[1].ts - channels[0].ts) +
           ( (channels[1].totalize * (int64_t)PS_PER_SEC) - 1) );
         Serial.println(" chC (B-A)");  
         channels[0].last_ts = channels[0].ts;
         channels[0].ts = 0;
         channels[1].last_ts = channels[1].ts;
         channels[1].ts = 0;
         }
         
#ifdef DETAIL_TIMING      
        end_micros = micros();         
        Serial.print(" execution time (us) after output: ");
        Serial.print(end_micros - start_micros);
        Serial.println();
#endif

    } // if
  } // for
} // loop()

/**************************************************************************************/
 
// ISR for timer. Capture PICcount on each channel's STOP 0->1 transition.
void coarseTimer() {
  PICcount++;
}  
//    if (digitalRead(channels[i].gotSTOP)==false) 
//     channels[i].previousSTOP = false;
//    else 
//      if (channels[i].previousSTOP==false) {
//        channels[i].PICstop = PICcount;      // update PICstop on 0->1 transition of STOP
//        channels[i].previousSTOP = true;
//     } 
//   }
//}

void catch_stopA() {
  channels[0].PICstop = PICcount;
}

void catch_stopB() {
  channels[1].PICstop = PICcount;
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
  write(CONFIG2, reg_byte);
  // enable interrupts -- 0x01 new measurement, 0x02 COARSE_OVF, 0x04 CLOCK_OVF 
  write(INT_MASK, 0x04);           // default 0x07 
  // coarse counter overflow occurs when timeN/63 > mask
  //write(COARSE_CNTR_OVF_H, 0x04);  // default is 0xFF 
  //write(COARSE_CNTR_OVF_L, 0x00);  // default is 0xFF
  //clock counter overflow occurs when clock_countN > mask
  write(CLOCK_CNTR_OVF_H, 0x05);     // default is 0xFF
  write(CLOCK_CNTR_OVF_L, 0x00);     // default is 0xFF
  
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
int64_t tdc7200Channel::read() {
  int64_t normLSB;
  int64_t calCount;
  int64_t ring_ticks;
  int64_t ring_ps;
  int64_t tof; 
  
  time1Result = readReg24(TIME1);
  time2Result  = readReg24(TIME2);
  clock1Result = readReg24(CLOCK_COUNT1);
  cal1Result = readReg24(CALIBRATION1);
  cal2Result = readReg24(CALIBRATION2);

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

void print_unsigned_picos_as_seconds (uint64_t x) {
  uint64_t sec, secx, frac, frach, fracx, fracl;    
  char str[128];
  
  sec = x / 1000000000000;
  secx = sec * 1000000000000;
  frac = x - secx;

  // break fractional part of seconds into two 6 digit numbers

  frach = frac / 1000000;
  fracx = frach * 1000000;
  fracl = frac - fracx;

  sprintf(str,"%lu.",sec), Serial.print(str);
  sprintf(str, "%06lu", frach), Serial.print(str);
  sprintf(str, "%06lu", fracl), Serial.print(str);  
} 

void print_signed_picos_as_seconds (int64_t x) {
  int64_t sec, secx, frac, frach, fracx, fracl;    
  char str[128];
  
  sec = x / 1000000000000;
  secx = sec * 1000000000000;
  frac = abs(x - secx);
  
  // break fractional part of seconds into two 6 digit numbers

  frach = frac / 1000000;
  fracx = frach * 1000000;
  fracl = frac - fracx;

  if (x < 0) {
    Serial.print("-");
  }
  sprintf(str,"%ld.",sec), Serial.print(str);
  sprintf(str, "%06ld", frach), Serial.print(str);
  sprintf(str, "%06ld", fracl), Serial.print(str);  
}

// read and write config struct in eeprom
uint16_t eeprom_write_config(uint16_t offset, config_t x) {
  uint16_t num_bytes;
  num_bytes = EEPROM_writeAnything(offset, x);
  return num_bytes;
}

uint16_t eeprom_read_config(uint16_t offset, config_t x) {
  uint16_t num_bytes;
  num_bytes = EEPROM_readAnything(offset, x);
  return num_bytes;
}

uint16_t eeprom_write_default (uint16_t offset) {
  config_t x;
  strncpy(x.SW_VERSION,SW_VERSION,sizeof(SW_VERSION));
  strncpy(x.BOARD_SER_NUM,BOARD_SER_NUM,sizeof(BOARD_SER_NUM));
  x.MODE = 0; // MODE -- 0 is timestamp, 3 is TimeLab 
  x.CLOCK_HZ = 10000000; // 10 MHz
  x.PICTICK_PS = 100000000; // 100us
  x.CAL_PERIODS = 20; // CAL_PERIODS (2, 10, 20, 40)
  x.TIME_DILATION[0] = 2500;  // 2500 seems right for chA on C1
  x.TIME_DILATION[1] = 2500;
  x.FIXED_TIME2[0] = 0;
  x.FIXED_TIME2[1] = 0;
  x.FUDGE0[0] = 0;
  x.FUDGE0[1] = 0;
  eeprom_write_config(offset,x);
}
