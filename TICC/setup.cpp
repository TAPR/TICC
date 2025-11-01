// setup.cpp -- TICC initialization functions
//
// Copyright John Ackermann N8UR 2016-2025
// Licensed under BSD 2-clause license

#include <Arduino.h>
#include <SPI.h>
#include <EEPROM.h>
#include "board.h"
#include "config.h"
#include "tdc7200.h"
#include "print.h"
#include "setup.h"
#include "config_menu_text.h"
#include "TICC.h"

// Forward declarations for ISRs (defined in TICC.ino)
void coarseTimer();
void catch_stop0();
void catch_stop1();

// Forward declaration for enableInterrupt (from EnableInterrupt.h in TICC.ino)
void enableInterrupt(uint8_t interruptDesignator, void (*userFunc)(void), uint8_t mode);

// External global variables from TICC.ino
extern volatile int64_t PICcount;
extern int64_t PICTICK_PS;
extern int64_t CLOCK_PERIOD;
extern int16_t CAL_PERIODS;
extern config_t config;
extern MeasureMode MODE, lastMODE;
extern uint8_t skip_config_prompt_once;
extern volatile uint8_t request_restart;
extern tdc7200Channel channels[];

void ticc_setup() {
  size_t i;
  boolean last_pin;

  pinMode(COARSEint, INPUT);
  pinMode(OUT1, OUTPUT);
  pinMode(OUT2, OUTPUT);
  pinMode(EXT_LED_0, OUTPUT);  // need to set these here; on-board LEDs are set up in TDC7200::setup
  pinMode(EXT_LED_1, OUTPUT);
  pinMode(EXT_LED_CLK, OUTPUT);

  // turn on the LEDs to show we're alive -- use macros from board.h
  SET_LED_0;
  SET_EXT_LED_0;
  SET_LED_1;
  SET_EXT_LED_1;

  /*******************************************
   * Configuration read/change/store
   *******************************************/
  
  // Initialize serial at default baud rate for all operations
  Serial.end();  // first close in case we've come here from a break
  Serial.begin(115200);  // Use default baud rate for initial operations
  delay(1500);
  Serial.flush();
  
  // check or assign serial number (must be done before checking/config defaults)
  get_serial_number();
  
  // if no config stored, or wrong version, restore from default
  if (EEPROM.read(CONFIG_START) != EEPROM_VERSION) {
    configPrintlnProg(startup_no_config);
    eeprom_write_config_default(CONFIG_START);
  }
  
  // read config and set global vars
  eeprom_read_config();
  lastMODE = config.MODE;

  // start the serial library with configured baud rate
  Serial.end();  // first close in case we've come here from a break
  Serial.begin(config.BAUD_RATE);
  // Allow host CDC/TTY stack to settle to avoid buffered prompts on reconnect
  delay(1500);
  Serial.flush();
  
  // start the SPI library:
  SPI.end();  // first close in case we've come here from a break
  SPI.begin();

  // print banner -- all non-data output lines begin with "#" so they're seen as comments
  Serial.println("# ");
  configPrintlnProg(startup_ticc_title);
  configPrintlnProg(startup_copyright);
  Serial.println("# ");
  
  configPrintlnProg(startup_config_separator);
  configPrintlnProg(startup_config_header);
  print_config(config);
  configPrintlnProg(startup_config_separator);
  Serial.println("# ");

  // get and save config change (skip once after exiting config menu via '#')
  if (!skip_config_prompt_once) {
    // New config system startup check
    configPrintlnProg(startup_config_request);
    Serial.print("# ");
    bool configRequested = false;
    for (int i = 6; i >= 0; --i) {  // wait ~6 sec so user can type something
      delay(250);   Serial.print('.'); if (Serial.available()) { configRequested = true; break; }
      delay(250);   Serial.print('.'); if (Serial.available()) { configRequested = true; break; }
      delay(250);   Serial.print('.'); if (Serial.available()) { configRequested = true; break; }
      delay(250);   Serial.print('.'); if (Serial.available()) { configRequested = true; break; }
    }
    Serial.println();
    while (Serial.available()) { char c = Serial.read(); }   // eat any characters entered before starting config menu
    if (configRequested) {
      backup_config();
      show_config_menu();
      handle_config_change_exit();
    }
  } else {
    skip_config_prompt_once = 0;
  }
  // Copy performance-critical config values to local variables for hot path efficiency
  MODE = config.MODE;
  PICTICK_PS = config.PICTICK_PS;
  CLOCK_PERIOD = (PS_PER_SEC / config.CLOCK_HZ);
  CAL_PERIODS = config.CAL_PERIODS;

  for (i = 0; i < 2; ++i) {  // 2 channels: 0 and 1
    // initialize the channels struct variables
    channels[i].totalize = 0;
    channels[i].PICstop = 0;
    channels[i].tof = 0;
    
    // Initialize timestamp structure
    channels[i].timestamp.seconds = 0;
    channels[i].timestamp.picos = 0;
    channels[i].last_timestamp.seconds = 0;
    channels[i].last_timestamp.picos = 0;
    
    channels[i].last_picstop = 0; // Initialize to 0 (will be updated after first measurement)
    channels[i].name = config.NAME[i];
    channels[i].prop_delay = config.PROP_DELAY[i];
    channels[i].time_dilation = config.TIME_DILATION[i];
    channels[i].fixed_time2 = config.FIXED_TIME2[i];
    channels[i].fudge = config.PROP_DELAY[i] + config.FUDGE0[i]; // these config options are additive.

    // Initialize coarse-time cache (always enabled)
    channels[i].last_picstop = 0;
    channels[i].cached_sec = 0;
    channels[i].cached_rem_ticks = 0;
    
    update_cached_config(); // Initialize cached config parameters for maximum print performance
    
    // set up the chips with aggressive reset to clear any stuck interrupt flags
    // Some TDC7200 chips occasionally get stuck interrupt flags that cause rapid
    // repeated processing of the same timestamp. flush_and_reset() is more thorough
    // than just ready_next() and should clear these stuck flags, making serial
    // port restarts as reliable as USB power cycles for fixing this hardware issue.
    channels[i].tdc_setup();
    channels[i].flush_and_reset();  // More thorough than just ready_next()
  }

  /*******************************************
   * Synchronize multiple TICCs sharing common
   * 10 MHz and 10 kHz clocks.
   *******************************************/
  if (config.SYNC_MODE == 'P') {                  // if we are primary, send sync by sending CLIENT_SYNC (A8) high
    delay(2000);                                  // but first sleep to allow client boards to get ready
    pinMode(CLIENT_SYNC, OUTPUT);                 // set CLIENT_SYNC as output (defaults to input)
    digitalWrite(CLIENT_SYNC, LOW);               // make sure it's low
    delay(1000);                                  // wait a bit in case other boards need to catch up
    last_pin = digitalRead(COARSEint);            // get current state of COARSE_CLOCK
    while (digitalRead(COARSEint) == last_pin) {  // loop until COARSE_CLOCK changes
      delayMicroseconds(5);                       // wait a bit
      if (i <= 50) {                              // should never get above 20 (100us)
        i++;
        if (i == 50) {  // something's probably wrong
          Serial.println("# ");
          Serial.println("# ");
          Serial.println("# No COARSE_CLOCK... is 10 MHz connected?");
        }
      }
    }
    digitalWrite(CLIENT_SYNC, HIGH);  // send sync pulse
  } else {
    Serial.println("# ");
    Serial.println("# ");
    Serial.println("# In secondary mode and waiting for sync...");
  }

  while (!digitalRead(CLIENT_SYNC)) {}               // spin until CLIENT_SYNC asserts
  PICcount = 0;                                      // initialize counter
  // trigger coarseTimer on falling edge to avoid possible
  // race condition since catch_stop needs to trigger on rising
  // edge.  This buys time.
  enableInterrupt(COARSEint, coarseTimer, FALLING);  // enable counter interrupt
  enableInterrupt(STOP_0, catch_stop0, RISING);      // enable interrupt to catch channel 0
  enableInterrupt(STOP_1, catch_stop1, RISING);      // enable interrupt to catch channel 1
  digitalWrite(CLIENT_SYNC, LOW);                    // unassert -- results in ~22uS sync pulse
  pinMode(CLIENT_SYNC, INPUT);                       // set back to input just to be neat

  // print header to stdout (unless restart is pending)
  if (!request_restart) {
    Serial.println("# ");
    char line[128];
    switch (config.MODE) {
    case Timestamp:
      strcpy_P(line, startup_mode_timestamp);
      Serial.print("# ");
      Serial.print(line);
      Serial.print(config.PLACES);
      strcpy_P(line, startup_decimal_places);
      Serial.println(line);
      break;
    case Paired_Timestamp:
      strcpy_P(line, startup_mode_paired);
      Serial.print("# ");
      Serial.print(line);
      Serial.print(config.PLACES);
      strcpy_P(line, startup_decimal_places);
      Serial.println(line);
      break;
    case Interval:
      strcpy_P(line, startup_mode_interval);
      Serial.print("# ");
      Serial.print(line);
      Serial.print(config.PLACES);
      strcpy_P(line, startup_decimal_places);
      Serial.println(line);
      break;
    case Period:
      strcpy_P(line, startup_mode_period);
      Serial.print("# ");
      Serial.print(line);
      Serial.print(config.PLACES);
      strcpy_P(line, startup_decimal_places);
      Serial.println(line);
      break;
    case Hat:
      strcpy_P(line, startup_mode_hat);
      Serial.print("# ");
      Serial.print(line);
      Serial.print(config.PLACES);
      strcpy_P(line, startup_decimal_places);
      Serial.println(line);
      break;
    case Debug:
      Serial.print("# ");
      strcpy_P(line, startup_mode_debug);
      Serial.println(line);
      break;
    case Binary:
      Serial.print("# ");
      strcpy_P(line, startup_mode_binary);
      Serial.println(line);
      Serial.print("# ");
      strcpy_P(line, startup_mode_binary_desc);
      Serial.println(line);
      Serial.println();
      break;
    case Null:
      Serial.print("# ");
      strcpy_P(line, startup_mode_null);
      Serial.println(line);
      break;
  }  // switch
  }  // if (!request_restart)

  // turn the LEDs off
  CLR_LED_0;
  CLR_EXT_LED_0;
  CLR_LED_1;
  CLR_EXT_LED_1;
  CLR_EXT_LED_CLK;

}  // ticc_setup
