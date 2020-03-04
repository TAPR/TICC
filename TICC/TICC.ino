
// TICC.ino - master sketch file
// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2020
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

extern const char SW_VERSION[17] = "20200303.1";    // 3 March 2020 - version 1

//#define DETAIL_TIMING     // if enabled, prints execution time

#include <stdint.h>           // define unint16_t, uint32_t
#include <SPI.h>              // SPI support
#include <EEPROM.h>           // eeprom library

#include "board.h"            // LED macros

// install EnableInterrupt from the .zip file in the main TICC folder
// or download from https://github.com/GreyGnome/EnableInterrupt
// use "Sketch/Include Library/Add .ZIP Library" to install
#include <EnableInterrupt.h>  // use faster interrupt library

#include "config.h"           // config and eeprom
#include "misc.h"             // random functions
#include "board.h"            // Arduino pin definitions
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
int64_t WRAP;

config_t config;
MeasureMode MODE, lastMODE;

static tdc7200Channel channels[] = {
        tdc7200Channel('0', ENABLE_0, INTB_0, CSB_0, STOP_0,LED_0),
        tdc7200Channel('1', ENABLE_1, INTB_1, CSB_1, STOP_1,LED_1),
};

/****************************************************************
We don't use the default setup() routine -- see
ticc_setup() below
****************************************************************/
void setup() { }

/****************************************************************
Here is where setup really happensw
****************************************************************/
void ticc_setup() {
   
  int i;
  boolean last_pin;
  
  pinMode(COARSEint, INPUT);
  pinMode(OUT1, OUTPUT);
  pinMode(OUT2, OUTPUT);
  pinMode(EXT_LED_0, OUTPUT);  // need to set these here; on-board LEDs are set up in TDC7200::setup
  pinMode(EXT_LED_1, OUTPUT);
  pinMode(EXT_LED_CLK,OUTPUT);
  
  // turn on the LEDs to show we're alive -- use macros from board.h
  SET_LED_0;
  SET_EXT_LED_0;
  SET_LED_1;
  SET_EXT_LED_1;
   
  // start the serial library
  Serial.end();               // first close in case we've come here from a break
  Serial.begin(115200);
  // start the SPI library:
  SPI.end();                  // first close in case we've come here from a break
  SPI.begin();
  
  /*******************************************
   * Configuration read/change/store
  *******************************************/
  // check or assign serial number
  get_serial_number();
  
  // if no config stored, or wrong version, restore from default 
  if ( EEPROM.read(CONFIG_START) != EEPROM_VERSION) { 
    Serial.println("No config found.  Writing default...");
    eeprom_write_config_default(CONFIG_START);  
    }

  // read config and set global vars
  EEPROM_readAnything(CONFIG_START, config); 
  lastMODE = config.MODE;
  
  // print banner -- all non-data output lines begin with "#" so they're seen as comments
  Serial.println();
  Serial.println("# TAPR TICC Timestamping Counter");
  Serial.println("# Copyright 2016-2020 N8UR, K9TRV, NH6Z, WA8YWQ");
  Serial.println();

  Serial.println("#####################");
  Serial.println("# TICC Configuration: ");
  print_config(config);
  Serial.println("#####################");
  
  // get and save config change
  UserConfig(&config);
  MODE = config.MODE;
  
  CLOCK_HZ = config.CLOCK_HZ;
  CLOCK_PERIOD = (PS_PER_SEC/CLOCK_HZ);
  PICTICK_PS = config.PICTICK_PS;
  CAL_PERIODS = config.CAL_PERIODS;
  WRAP = config.WRAP;
       
  for(i = 0; i < ARRAY_SIZE(channels); ++i) {
    // initialize the channels struct variables
    channels[i].totalize = 0;
    channels[i].PICstop = 0;
    channels[i].tof = 0;
    channels[i].ts = 0;
    channels[i].name = config.NAME[i];
    channels[i].prop_delay = config.PROP_DELAY[i];
    channels[i].time_dilation = config.TIME_DILATION[i];
    channels[i].fixed_time2 = config.FIXED_TIME2[i];
    // For user convenience, we allow two settings that additively determine delay
    channels[i].fudge = config.PROP_DELAY[i] + config.FUDGE0[i];

    // set up the chips
    channels[i].tdc_setup();
    channels[i].ready_next();
  }
  
  /*******************************************
   * Synchronize multiple TICCs sharing common 10 MHz and 10 kHz clocks.
  *******************************************/ 
  if (config.SYNC_MODE == 'M') {                     // if we are master, send sync by sending CLIENT_SYNC (A8) high
    delay(2000);                                     // but first sleep to allow client boards to get ready
    pinMode(CLIENT_SYNC, OUTPUT);                    // set CLIENT_SYNC as output (defaults to input)
    digitalWrite(CLIENT_SYNC, LOW);                  // make sure it's low
    delay(1000);                                     // wait a bit in case other boards need to catch up
    last_pin = digitalRead(COARSEint);               // get current state of COARSE_CLOCK
    while (digitalRead(COARSEint) == last_pin) {     // loop until COARSE_CLOCK changes
      delayMicroseconds(5);                          // wait a bit
      if (i <= 50) {                                 // should never get above 20 (100us)
        i++;
        if (i == 50) {                               // something's probably wrong
          Serial.println("");
          Serial.println("");
          Serial.println("# No COARSE_CLOCK... is 10 MHz connected?");
        }
      }
    }
    digitalWrite(CLIENT_SYNC, HIGH);                  // send sync pulse
  } else {
    Serial.println("");
    Serial.println("");
    Serial.println("# In client mode and waiting for sync...");
  }
  
  while (!digitalRead(CLIENT_SYNC)) {}               // whether master or client, spin until CLIENT_SYNC asserts
  PICcount = 0;                                      // initialize counter
  enableInterrupt(COARSEint, coarseTimer, FALLING);  // enable counter interrupt
  enableInterrupt(STOP_0, catch_stop0, RISING);      // enable interrupt to catch channel A
  enableInterrupt(STOP_1, catch_stop1, RISING);      // enable interrupt to catch channel 1
  digitalWrite(CLIENT_SYNC, LOW);                    // unassert -- results in ~22uS sync pulse
  pinMode(CLIENT_SYNC, INPUT);                       // set back to input just to be neat
  
  // print header to stdout
  Serial.println("");
  Serial.println("");
  switch (config.MODE) {
    case Timestamp:
      Serial.print("# timestamp (seconds with ");Serial.print(PLACES);Serial.println(" decimal places)");
      break;
    case Interval:
      Serial.print("# time interval A->B (seconds with ");Serial.print(PLACES);Serial.println(" decimal places)");
      break;
    case Period:
      Serial.print("# period (seconds with ");Serial.print(PLACES);Serial.println(" decimal places)");
      break;
    case timeLab:
      Serial.print("# timestamp chA, chB; interval chA->B (seconds with ");Serial.print(PLACES);Serial.println(" decimal places)");
      break;
    case Debug:
      Serial.println("# time1 time2 clock1 cal1 cal2 PICstop tof timestamp");
      break;
  } // switch
  
// turn the LEDs off
  CLR_LED_0;
  CLR_EXT_LED_0;
  CLR_LED_1;
  CLR_EXT_LED_1;
  CLR_EXT_LED_CLK;
   
} // ticc_setup  

/****************************************************************/
void loop() {

  ticc_setup();                                     // initialize and optionally go to config

  while (1) {
    if ( Serial.read() == '#') {        // test for break character
      break;
    }
  
    int i;
    static  int32_t last_micros = 0;                // Loop watchdog timestamp
    static  int64_t last_PICcount = 0;              // Counter state memory
 
    // Ref Clock indicator:
    // Test every 2.5 coarse tick periods for PICcount changes,
    // and turn on EXT_LED_CLK if changes are detected
    if( (micros() - last_micros) > 250 ) {  // hard coded to avoid fp math; breaks if PICTICK_PS changes 
      last_micros = micros();               // Update the watchdog timestamp
      if(PICcount != last_PICcount) {       // Has the counter changed since last sampled?
        SET_EXT_LED_CLK;                    // Yes: LED goes on
        last_PICcount = PICcount;           // Save the current counter state
      } else CLR_EXT_LED_CLK;               // No: LED goes off
    }
 
    for(i = 0; i < ARRAY_SIZE(channels); ++i) {
     
      // Only need to do anything if INTB is low, otherwise no work to do.
       if(digitalRead(channels[i].INTB)==0) {
         #ifdef DETAIL_TIMING
           start_micros = micros();
         #endif
       
         // turn LED on -- use board.h macro for speed
         if (i == 0) {SET_LED_0;SET_EXT_LED_0;};
         if (i == 1) {SET_LED_1;SET_EXT_LED_1;};

         channels[i].last_tof = channels[i].tof;  // preserve last value
         channels[i].last_ts = channels[i].ts;    // preserve last value
         channels[i].tof = channels[i].read();    // get data from chip
         channels[i].ts = (channels[i].PICstop * (int64_t)PICTICK_PS) - channels[i].tof;
         channels[i].ts -= channels[i].prop_delay;     // subtract propagation delay
         channels[i].period = channels[i].ts - channels[i].last_ts;
         channels[i].totalize++;                  // increment number of events   
         channels[i].ready_next();                // Re-arm for next measurement, clear TDC INTB
       
      // if poll character is not null, only output if we've received that character via serial
      // NOTE: this may provide random results if measuring timestamp from both channels!
      if ( (channels[i].totalize > 2) &&             // throw away first readings      
         ( (!config.POLL_CHAR)  ||                 // if unset, output everything
           ( (Serial.available() > 0) && (Serial.read() == config.POLL_CHAR) ) ) ) {   
      
         switch (config.MODE) {
           case Timestamp:
               print_signed_picos_as_seconds(channels[i].ts, PLACES);
               Serial.print( " ch");Serial.println(channels[i].name);
           break;
       
           case Interval:
             if ( (channels[0].ts > 1) && (channels[1].ts > 1) ) { // need both channels to be sane
               print_signed_picos_as_seconds(channels[1].ts - channels[0].ts, PLACES);
               Serial.println(" TI(A->B)");
               channels[0].ts = 0; // set to zero for test next time
               channels[1].ts = 0; // set to zero for test next time
             }
           break;
       
           case Period:
             print_signed_picos_as_seconds(channels[i].period, PLACES);
             Serial.print( " ch");Serial.println(channels[i].name);
           break;
       
           case timeLab:
             if ( (channels[0].ts > 1) && (channels[1].ts > 1) ) { // need both channels to be sane
               print_signed_picos_as_seconds(channels[0].ts, PLACES);
               Serial.print(" ");Serial.println(channels[0].name);
               print_signed_picos_as_seconds(channels[1].ts, PLACES);
               Serial.print(" ");Serial.println(channels[1].name);
           
               // horrible hack that creates a fake timestamp for
               // TimeLab -- it's actually tint(1-0) stuck onto the
               // integer part of the channel 1 timestamp.
               print_signed_picos_as_seconds( (channels[1].ts - channels[0].ts, PLACES) +
                 ( (channels[1].totalize * (int64_t)PS_PER_SEC) - 1), PLACES );
               Serial.print("chC (");Serial.print(channels[1].name);Serial.print("-");Serial.print(channels[0].name);Serial.println(")"); 
               channels[0].ts = 0; // set to zero for test next time
               channels[1].ts = 0; // set to zero for test next time
             } 
           break;
  
           case Debug:
             char tmpbuf[8];
             sprintf(tmpbuf,"%06u ",channels[i].time1Result);Serial.print(tmpbuf);
             sprintf(tmpbuf,"%06u ",channels[i].time2Result);Serial.print(tmpbuf);
             sprintf(tmpbuf,"%06u ",channels[i].clock1Result);Serial.print(tmpbuf);
             sprintf(tmpbuf,"%06u ",channels[i].cal1Result);Serial.print(tmpbuf);
             sprintf(tmpbuf,"%06u ",channels[i].cal2Result);Serial.print(tmpbuf);
             Serial.print((int32_t)channels[i].PICstop);Serial.print(" ");
             print_signed_picos_as_seconds(channels[i].tof, PLACES);Serial.print(" ");
             print_signed_picos_as_seconds(channels[i].ts, PLACES);
             Serial.print( " ch");Serial.println(channels[i].name);    
           break;

         case Null:
           break;
    } // switch


 } // print result

       // turn LED off
       if (i == 0) {CLR_LED_0;CLR_EXT_LED_0;};
       if (i == 1) {CLR_LED_1;CLR_EXT_LED_1;};
       
      #ifdef DETAIL_TIMING
        end_micros = micros() - start_micros;               
        Serial.println(end_micros);
      #endif

      } // if INTB
    } // for
  } // while (1) loop

  Serial.println();
  Serial.println("Got break character... exiting loop");
  Serial.println();
  delay(100);

} // main loop()


/****************************************************************
 * Interrupt Service Routines
 ****************************************************************/
 
// ISR for timer. Capture PICcount on each channel's STOP 0->1 transition.
void coarseTimer() {
  PICcount++;
  if (PICcount == WRAP) {
    PICcount = 0;
  }
}  

void catch_stop0() {
  channels[0].PICstop = PICcount;
}

void catch_stop1() {
  channels[1].PICstop = PICcount;  
}
/****************************************************************/
       
