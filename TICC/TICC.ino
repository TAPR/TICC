// TICC.ino - master sketch file
// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

//#define PRINT_REG_RESULTS // if enabled, print register values
//#define DETAIL_TIMING     // if enabled, prints execution time

extern const char SW_VERSION[17] = "0.80";  // 16 October 2016
extern const char BOARD_SER_NUM[17] = "0123456789";  // how to set this for each board?

#include <stdint.h>           // define unint16_t, uint32_t
#include <SPI.h>              // SPI support
#include <EnableInterrupt.h>  // use faster interrupt library

#include "ticc.h"             // general config
#include "misc.h"             // random functions
#include "board.h"            // Arduino pin definitions
#include "config.h"           // config and eeprom
#include "tdc7200.h"          // TDC registers and structures


#ifdef DETAIL_TIMING
  int32_t start_micros;
  int32_t end_micros;
#endif

volatile int64_t PICcount;
int64_t tint;
int64_t CLOCK_HZ;
int64_t PICTICK_PS; 
int64_t CLOCK_PERIOD;
int16_t CAL_PERIODS;

config_t config;
MeasureMode MODE, lastMODE;

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
  
  if ( EEPROM.read(CONFIG_START) != EEPROM_VERSION) { 
    Serial.println("No config found.  Writing default...");
    eeprom_write_config_default(CONFIG_START);  
    }

  // read config and set global vars
  EEPROM_readAnything(CONFIG_START, config);
 
  CLOCK_HZ = config.CLOCK_HZ;
  CLOCK_PERIOD = (PS_PER_SEC/CLOCK_HZ);
  PICTICK_PS = config.PICTICK_PS;
  CAL_PERIODS = config.CAL_PERIODS;
  lastMODE = config.MODE;
  
  Serial.println();
  Serial.println("TAPR TICC Timestamping Counter");
  Serial.print("EEPROM Version: ");Serial.println(EEPROM.read(CONFIG_START));
  Serial.print("Software Version: ");Serial.println(SW_VERSION);
  Serial.println("Copyright 2016 N8UR, K9TRG, NH6Z");
  Serial.println();
  Serial.print("CLOCK_HZ = ");Serial.println((int32_t)CLOCK_HZ);
  Serial.print("CAL_PERIODS = ");Serial.println(CAL_PERIODS);
  Serial.println();

  // get and save config change
  MODE = UserConfig();
  if (MODE == NoChange) {
    MODE = lastMODE;
  } else { 
    config.MODE = MODE;
    EEPROM_writeAnything(CONFIG_START, config);
  }
  Serial.print("Mode = ");print_MeasureMode(MODE);Serial.println();
  
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
    Serial.println("# time1 time2 clock1 cal1 cal2 PICstop tof timestamp");
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
       channels[i].totalize++;
       
       #ifdef PRINT_REG_RESULTS
         if (channels[i].totalize > 2) {
           Serial.print((int32_t)channels[i].PICstop);Serial.print("\t");
         }
       #endif
       
       channels[i].ts = (channels[i].PICstop * (int64_t)PICTICK_PS) - channels[i].tof;
       channels[i].period = channels[i].ts - channels[i].last_ts;
       
#ifdef DETAIL_TIMING      
       end_micros = micros();         
       Serial.print(" execution time before output (us): ");
       Serial.print(end_micros - start_micros);
       Serial.println();
#endif

       // NOTE NOTE NOTE -- need to change to use enum
       // simple timestamp mode
       if ( (config.MODE == Timestamp) && 
            (channels[i].totalize > 2) )
         {
         print_signed_picos_as_seconds(channels[i].ts);
         Serial.print( " ch");Serial.println(channels[i].ID);    
         }
       
       // time interval mode (A->B)
       if ( (config.MODE == Interval) && 
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
       if ( (config.MODE == Period) && 
            (channels[i].totalize > 2) ) 
         {
         print_signed_picos_as_seconds(channels[i].ts - channels[i].last_ts);
         Serial.print( " ch");Serial.println(channels[i].ID);    
         }  
       
       // timelab mode -- output period data plus TI data
       if ( (config.MODE == timeLab) &&
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

void catch_stopA() {
  channels[0].PICstop = PICcount;
}

void catch_stopB() {
  channels[1].PICstop = PICcount;
}

