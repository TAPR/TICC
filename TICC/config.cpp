// config.cpp -- set/read/write configuration
// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>           // define unint16_t, uint32_t
#include <EEPROM.h>           // read/write EEPROM
#include <SPI.h>
#include "ticc.h"             // general config
#include "misc.h"             // random functions
#include "board.h"            // Arduino pin definitions
#include "config.h"           // config and eeprom
#include "tdc7200.h"          // TDC registers and structures

extern const char SW_VERSION[17];
extern const char BOARD_SER_NUM[17];

MeasureMode UserConfig() {
////////////////////////////////////////////////////////////////////////////
// Test for operator request to go to setup mode.
// If setup mode not entered, or invalid command, defaults to Timestamp mode.
// Commands:
// T    - Timestamp mode
// P    - Period mode
// I    - time Interval (A-B) mode
// L    - Dual Channel TI mode for TimeLab
//
// BUGBUG - This is buggy right now

  char inputline[128];
  char modechar;

  Serial.println("Type a few characters to go into setup mode. ");

  delay(3000);

  int j = 0;

  while (Serial.available())
    inputline[j++] = Serial.read();

  inputline[j] = '/0';
  
// Serial.print("# chars received = "),Serial.println(j);
// Serial.print("Received input chars: ");

 for(int i=0; i<j; i++) {
   Serial.print(inputline[i]);
 }

 
  if(j==0) {
    Serial.println("No setup command received");Serial.println();
    return NoChange;
    }

  Serial.println("Setup mode.  Valid single-letter commands are:"), Serial.println();
  Serial.println("   T     (T)imestamp mode");
  Serial.println("   P     (P)eriod mode");
  Serial.println("   I     time (I)nterval A->B mode");
  Serial.println("   L     Time(L)ab interval mode");
  Serial.println(),Serial.print("Enter mode: ");

  while (!Serial.available())
    ;

  modechar = toupper(Serial.read());
  
  Serial.print(modechar),Serial.println();Serial.println();

  MeasureMode m;

  switch (modechar) {
    case 'T':
      m = Timestamp;
      return(m);  // Timestamp mode
      break;
    case 'P':
      m = Period;
      return(m);  // Period mode
      break;
    case 'I':
      m = Interval;
      return(m);  // time Interval mode
      break;
    case 'L':
      m = timeLab;
      return(m); // TimeLab mode
      break;
    default :
      Serial.println("Invalid entry; no change made");
      return(NoChange);
   }  
}

// Pretty-print mode
void print_MeasureMode(MeasureMode x) {
  switch (x) {
    case Timestamp:
      Serial.println("Timestamp");
      break;
    case Period:
      Serial.println("Period");
      break;
    case Interval:
      Serial.println("Time Interval A->B");
      break;
    case timeLab:
      Serial.println("TimeLab 3-channel");     
  }  
}

uint16_t eeprom_write_config_default (uint16_t offset) {
  config_t x;
  strncpy(x.SW_VERSION,SW_VERSION,sizeof(SW_VERSION));
  strncpy(x.BOARD_SER_NUM,BOARD_SER_NUM,sizeof(BOARD_SER_NUM));
  x.MODE = Timestamp; // MODE
  x.CLOCK_HZ = 10000000; // 10 MHz
  x.PICTICK_PS = 100000000; // 100us
  x.CAL_PERIODS = 20; // CAL_PERIODS (2, 10, 20, 40)
  x.TIME_DILATION[0] = 2500;  // 2500 seems right for chA on C1
  x.TIME_DILATION[1] = 2500;
  x.FIXED_TIME2[0] = 0;
  x.FIXED_TIME2[1] = 0;
  x.FUDGE0[0] = 0;
  x.FUDGE0[1] = 0;
  EEPROM_writeAnything(offset,x);
}


