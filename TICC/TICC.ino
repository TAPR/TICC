/*
 *
 * TICC.ino - master sketch file
 * TICC Time interval Counter based on TICC Shield using TDC7200
 *
 * Copyright John Ackermann N8UR 2016-2025
 * Portions Copyright George Byrkit K9TRV 2016
 * Portions Copyright Jeremy McDermond NH6Z 2016
 * Licensed under BSD 2-clause license
 *
 * See doc/TICC_architecture.md for details about how the TICC
 * firmware works.
 */

// 6 October 2025 - version 20251006.1
extern const char SW_VERSION[17] = "20251006.1";
extern const char SW_TAG[6] = "BETA";

#include <stdint.h>             // define unint16_t, uint32_t
#include <SPI.h>                // SPI support
#include <EEPROM.h>             // eeprom library
#include "EnableInterrupt.h"    // use faster interrupt library
#include "board.h"              // LED macros and Arduino pin definitions
#include "config.h"             // config and eeprom
#include "tdc7200.h"            // TDC registers and structures
#include "timestamps.h"    // timestamp utility functions
#include "print.h"              // optimized 64-bit printing routines

volatile int64_t PICcount;
int64_t CLOCK_HZ;
int64_t PICTICK_PS;
int64_t CLOCK_PERIOD;
int16_t CAL_PERIODS;
int16_t WRAP;
int64_t ticksPerSecond;  // number of coarse ticks per second

config_t config;
MeasureMode MODE, lastMODE;
static uint8_t skip_config_prompt_once = 0;
volatile uint8_t request_restart = 0;
static uint8_t just_restarted = 1;  // Flag to track if we just restarted

// Configuration change tracking
static config_t config_backup;  // Backup of config before changes
uint8_t config_changed = 0;  // Flag indicating config was modified (global for config.cpp access)
static uint8_t config_requested = 0;  // Flag indicating user requested config menu

static tdc7200Channel channels[] = {
  tdc7200Channel('0', ENABLE_0, INTB_0, CSB_0, STOP_0, LED_0),
  tdc7200Channel('1', ENABLE_1, INTB_1, CSB_1, STOP_1, LED_1),
};

void setup() {} // we don't use default setup(), but ticc_setup() below

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
  // check or assign serial number
  get_serial_number();

  // if no config stored, or wrong version, restore from default
  if (EEPROM.read(CONFIG_START) != EEPROM_VERSION) {
    // Need to initialize serial at default rate to show message
    Serial.end();  // first close in case we've come here from a break
    Serial.begin(115200);  // Use default baud rate for initial message
    delay(1500);
    Serial.flush();
    Serial.println("No config found.  Writing default...");
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
  Serial.println("# TAPR TICC Timestamping Counter");
  Serial.println("# Copyright 2016-2025 N8UR, K9TRV, NH6Z, WA8YWQ");
  Serial.println("# ");
  
  
  Serial.println("#####################");
  Serial.println("# TICC Configuration: ");
  print_config(config);
  Serial.println("#####################");
  Serial.println("# ");

  // get and save config change (skip once after exiting config menu via '#')
  if (!skip_config_prompt_once) {
    // New config system startup check
    Serial.println("# Type any character for config menu");
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
  MODE = config.MODE;

  CLOCK_HZ = config.CLOCK_HZ;
  CLOCK_PERIOD = (PS_PER_SEC / CLOCK_HZ);
  PICTICK_PS = config.PICTICK_PS;
  CAL_PERIODS = config.CAL_PERIODS;
  WRAP = config.WRAP;
  ticksPerSecond = PS_PER_SEC / PICTICK_PS;

  for (i = 0; i < ARRAY_SIZE(channels); ++i) {
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
  enableInterrupt(COARSEint, coarseTimer, FALLING);  // enable counter interrupt
  enableInterrupt(STOP_0, catch_stop0, RISING);      // enable interrupt to catch channel A
  enableInterrupt(STOP_1, catch_stop1, RISING);      // enable interrupt to catch channel B
  digitalWrite(CLIENT_SYNC, LOW);                    // unassert -- results in ~22uS sync pulse
  pinMode(CLIENT_SYNC, INPUT);                       // set back to input just to be neat

  // print header to stdout (unless restart is pending)
  if (!request_restart) {
    Serial.println("# ");
    switch (config.MODE) {
    case Timestamp:
      Serial.print("# timestamp (seconds with ");
      Serial.print(config.PLACES);
      Serial.println(" decimal places)");
      break;
    case Paired_Timestamp:
      Serial.print("# paired channel-order timestamp (seconds with ");
      Serial.print(config.PLACES);
      Serial.println(" decimal places)");
      break;
    case Interval:
      Serial.print("# time interval A->B (seconds with ");
      Serial.print(config.PLACES);
      Serial.println(" decimal places)");
      break;
    case Period:
      Serial.print("# period (seconds with ");
      Serial.print(config.PLACES);
      Serial.println(" decimal places)");
      break;
    case Hat:
      Serial.print("# timestamp ch0, ch1; interval chA->B (seconds with ");
      Serial.print(config.PLACES);
      Serial.println(" decimal places)");
      break;
    case Debug:
      Serial.println("# time1 time2 clock1 cal1 cal2 PICstop tof timestamp");
      break;
    case Binary:
      Serial.println("# Binary Timestamp mode - 12 byte frames:");
      Serial.println("# header (0x55,0xAA), channel (1 byte), PICstop (4 bytes), tof (4 bytes), CRC (1 byte)");
      Serial.println();
      break;
    case Null:
      Serial.println("# null output mode - no data");
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

// Check reference clock and handle reference lost condition
// Returns true if reference clock is OK, false if reference lost (restart needed)
bool check_reference_clock() {
  // Ref Clock indicator:
  // Test every 2.5 coarse tick periods for PICcount changes,
  // and turn on EXT_LED_CLK if changes are detected
  static uint32_t last_micros = 0;    // Loop watchdog timestamp
  static int64_t last_PICcount = 0;   // Counter state memory
  static uint8_t ext_clk_led_on = 0;  // LED state cache to avoid redundant writes

  // Reset static variables if we just restarted to prevent false "Reference lost" messages
  if (just_restarted) {
    last_micros = 0;
    last_PICcount = PICcount;  // Initialize with current PICcount value
    ext_clk_led_on = 0;
    just_restarted = 0;  // Clear the flag
    delay(100); // Small delay to allow coarseTimer ISR to start firing
  }

  uint32_t now = micros();
  if ((now - last_micros) > 250) {       // 2.5 ticks at 100 uS/tick
    last_micros = now;                   // Update the watchdog timestamp
    int64_t pc_snapshot;
    noInterrupts(); // protect read from ISR trips
    pc_snapshot = PICcount;
    interrupts();
    if (pc_snapshot != last_PICcount) {  // Has the counter changed since last sampled?
      if (!ext_clk_led_on) {             // turn on only if was off
        SET_EXT_LED_CLK;
        ext_clk_led_on = 1;
      }
      last_PICcount = pc_snapshot;  // Save the current counter state
    } else {
      if (ext_clk_led_on) {  // turn off only if was on
        CLR_EXT_LED_CLK;
        Serial.println("# 10 MHZ Reference lost!");
        Serial.println("# Press any key to restart after reference is restored.");
        ext_clk_led_on = 0;
        // Wait for a key press, then restart (reinitialize on next loop entry)
        while (Serial.available() == 0) { delay(10); }
        (void)Serial.read();
        return false; // Reference lost, restart needed
      }
    }
  }
  
  return true; // Reference clock OK
}

// Check if both channels have ready timestamps with sufficient totalize
bool both_channels_ready() {
  return (channels[0].new_ts_ready && channels[1].new_ts_ready) && 
         (channels[0].totalize > 2) && (channels[1].totalize > 2);
}

// Check poll character gating - returns true if output should proceed
bool poll_gating_ok() {
  bool ok = (!config.POLL_CHAR);
  if (!ok) {
    if ((Serial.available() > 0) && (Serial.read() == config.POLL_CHAR)) ok = true;
  }
  return ok;
}

// Global poll gating check - returns true if output should proceed
// This implements proper poll gating: when POLL_CHAR is set, only output after receiving the character
bool check_poll_gating() {
  static bool poll_character_received = false;
  
  // If no poll character is configured, always allow output
  if (!config.POLL_CHAR) {
    return true;
  }
  
  // Check if poll character is available
  if (Serial.available() > 0) {
    if (Serial.read() == config.POLL_CHAR) {
      poll_character_received = true;
    }
  }
  
  // Return true if poll character was received, false otherwise
  if (poll_character_received) {
    poll_character_received = false;  // Reset for next measurement
    return true;
  }
  
  return false;
}

// Consume new_ts_ready flags for both channels
void consume_both_flags() {
  channels[0].new_ts_ready = 0;
  channels[1].new_ts_ready = 0;
}

/****************************************************************/
void loop() {
  ticc_setup();  // initialize and optionally go to config
  
  // Check if restart was requested after config processing
  if (request_restart) {
    request_restart = 0;  // Clear the flag
    just_restarted = 1;   // Set flag for next loop iteration
    Serial.println("# Restart requested, reinitializing system...");
    return;  // Exit loop to trigger fresh ticc_setup() call
  }
  
  while (1) {    // this is the actual loop!

    if ( (Serial.read() == '#') ) {        // direct entry to config menu
      config_requested = 1; // Set flag to enter config at end of current loop iteration
      while (Serial.available()) (void)Serial.read(); // Clear serial buffer (like <enter> from "#<enter>")
    }

    // Check reference clock and handle reference lost condition
    if (!check_reference_clock()) {
      return; // Reference lost, restart requested
    }


    // Check both channels simultaneously for better timestamp ordering
    bool ready_chA = (digitalRead(channels[0].INTB) == 0);
    bool ready_chB = (digitalRead(channels[1].INTB) == 0);
    
    if (ready_chA && ready_chB) {
      // Both channels ready - process both simultaneously for better ordering
      
      // Turn on both LEDs
      SET_LED_0; SET_EXT_LED_0;
      SET_LED_1; SET_EXT_LED_1;
      
      // Process binary mode if selected
      if (config.MODE == Binary) {
        if (process_binary_mode(&channels[0])) {
          CLR_LED_0; CLR_EXT_LED_0;
        }
        if (process_binary_mode(&channels[1])) {
          CLR_LED_1; CLR_EXT_LED_1;
        }
      } else {
        // Calculate timestamps for both channels
        calculate_timestamp(&channels[0], PICTICK_PS);
        calculate_timestamp(&channels[1], PICTICK_PS);
        
        // Turn off both LEDs
        CLR_LED_0; CLR_EXT_LED_0;
        CLR_LED_1; CLR_EXT_LED_1;
      }
      
    } else if (ready_chA) {
      // Only channel A ready
      SET_LED_0; SET_EXT_LED_0;
      
      if (config.MODE == Binary) {
        if (process_binary_mode(&channels[0])) {
          CLR_LED_0; CLR_EXT_LED_0;
        }
      } else {
        calculate_timestamp(&channels[0], PICTICK_PS);
        CLR_LED_0; CLR_EXT_LED_0;
        
        // Check if channel B became ready during processing
        if (digitalRead(channels[1].INTB) == 0) {
          SET_LED_1; SET_EXT_LED_1;
          calculate_timestamp(&channels[1], PICTICK_PS);
          CLR_LED_1; CLR_EXT_LED_1;
        }
      }
      
    } else if (ready_chB) {
      // Only channel B ready
      SET_LED_1; SET_EXT_LED_1;
      
      if (config.MODE == Binary) {
        if (process_binary_mode(&channels[1])) {
          CLR_LED_1; CLR_EXT_LED_1;
        }
      } else {
        calculate_timestamp(&channels[1], PICTICK_PS);
        CLR_LED_1; CLR_EXT_LED_1;
        
        // Check if channel A became ready during processing
        if (digitalRead(channels[0].INTB) == 0) {
          SET_LED_0; SET_EXT_LED_0;
          calculate_timestamp(&channels[0], PICTICK_PS);
          CLR_LED_0; CLR_EXT_LED_0;
        }
      }
    }

    // Check if config was requested during this loop iteration (before early exit)
    if (config_requested) {
      config_requested = 0;  // Clear the flag
      
      // Stop TDC7200 measurements to prevent new data during config
      Serial.println("# Stopping measurements for config...");
      stop_all_measurements();
      
      // Flush any pending measurements from TDC7200 chips to prevent
      // them from appearing after returning from config menu
      Serial.println("# Flushing pending measurements before config...");
      flush_all_channels();
      
      // Small delay to ensure buffer is fully cleared
      delay(10);
      
      // Double-check buffer is clear
      while (Serial.available()) (void)Serial.read();
      
      // Enter config menu directly (no T/P choice needed)
      backup_config();
      show_config_menu();
      handle_config_change_exit();
      
      // Clear any remaining characters after config menu exits
      while (Serial.available()) (void)Serial.read();
      
      // Handle exit from config menu
      // Always handle config changes (whether written to EEPROM or not)
      handle_config_change_exit();
      
      // If restart required, exit loop to reinitialize
      if (config_change_requires_restart()) {
        skip_config_prompt_once = 1;
        just_restarted = 1;  // Set flag for next loop iteration
        return; // reinitialize via ticc_setup() on next loop entry
      }
      
      // Restart measurements after config changes
      Serial.println("# Restarting measurements...");
      start_all_measurements();
      
      // Clear the config_changed flag for next time
      config_changed = 0;
    }

    // Early exit optimization: skip output processing if no new timestamps are ready
    bool has_ready_timestamps = false;
    for (int ci = 0; ci < 2; ++ci) {
      if (channels[ci].new_ts_ready) {
        has_ready_timestamps = true;
        break;
      }
    }
    if (!has_ready_timestamps) {
      continue; // Skip all output processing
    }

    // Timestamp mode: print each timestamp immediately as ready (with strict ordering)
    if (config.MODE == Timestamp) {
      for (int ci = 0; ci < 2; ++ci) {
        if (channels[ci].new_ts_ready && (channels[ci].totalize > 2)) {
          if (check_poll_gating()) {
            char line[64];
            print_timestamp(line, sizeof(line), &channels[ci].timestamp, (char)channels[ci].name);
          }
          channels[ci].new_ts_ready = 0;  // consume
        }
      }
    }

    // Period mode: print period (timestamp - previous_timestamp) for each channel
    if (config.MODE == Period) {
      // Static buffer to store previous timestamps for each channel
      static Timestamp64 prev_timestamp[2] = {{0, 0}, {0, 0}};
      
      for (int ci = 0; ci < 2; ++ci) {
        if (channels[ci].new_ts_ready && (channels[ci].totalize > 2)) {
          if (check_poll_gating()) {
            // Calculate period: current timestamp - previous timestamp from buffer
            Timestamp64 period = timestamp_difference(&channels[ci].timestamp, 
                &prev_timestamp[ci]);
            char line[64];
            print_timestamp(line, sizeof(line), &period, (char)channels[ci].name, false);  // No wrap
          }
          channels[ci].new_ts_ready = 0;  // consume
          
          // Update buffer with current timestamp for next calculation
          prev_timestamp[ci] = channels[ci].timestamp;
        }
      }
    }


    // Paired_Timestamp mode: assemble and print pairs without timeout
    if (config.MODE == Paired_Timestamp) {
      // Two-slot buffer; accumulate two successive samples (across either channel)
      // then emit exactly two lines per pair in fixed order: if both channels are
      // present print chA then chB; if both are the same channel, print that
      // channel twice.
      struct PairSlot {
        Timestamp64 t;
        uint8_t ch;
      };
      static PairSlot ts_pair[2];
      static uint8_t ts_pair_count = 0;

      // Ingest any fresh samples into the pair buffer
      for (int ci = 0; ci < 2; ++ci) {
        if (channels[ci].new_ts_ready && (channels[ci].totalize > 2)) {
          if (ts_pair_count < 2) {
            ts_pair[ts_pair_count].t = channels[ci].timestamp;
            ts_pair[ts_pair_count].ch = (uint8_t)ci;
            ts_pair_count++;
          }
          channels[ci].new_ts_ready = 0;  // consume
        }
      }

      // If we have a complete pair, emit in fixed order with poll gating
      if (ts_pair_count == 2) {
        if (check_poll_gating()) {
          // Determine composition and enforce chA then chB order when both present
          if ((ts_pair[0].ch == 0 && ts_pair[1].ch == 1) || (ts_pair[0].ch == 1 && ts_pair[1].ch == 0)) {
            // Mixed channels: find A then B
            const PairSlot *A = (ts_pair[0].ch == 0) ? &ts_pair[0] : &ts_pair[1];
            const PairSlot *B = (ts_pair[0].ch == 1) ? &ts_pair[0] : &ts_pair[1];
            // Print chA timestamp - OPTIMIZED
            {
              char line[64];
              // Debug code removed - issue resolved
              print_timestamp(line, sizeof(line), &A->t, (char)channels[0].name);
            }
            
            // Print chB timestamp
            {
              char line[64];
              print_timestamp(line, sizeof(line), &B->t, (char)channels[1].name);
            }
          } else {
            // Same channel twice: print both with that channel's name
            uint8_t ci = ts_pair[0].ch;
            char cname = channels[ci].name;
            for (int k = 0; k < 2; ++k) {
              char line[64];
              print_timestamp(line, sizeof(line), &ts_pair[k].t, cname);
            }
          }
          ts_pair_count = 0;  // clear pair buffer after printing
        }
      }
    }


    // Shared pairing logic for Interval and 3-Cornered Hat modes
    if (both_channels_ready()) {
      if (poll_gating_ok()) {
        switch (config.MODE) {
          case Interval:
            {
              // Calculate time interval A->B
              Timestamp64 interval = timestamp_difference(&channels[1].timestamp, &channels[0].timestamp);
              char line[64];
              print_timestamp(line, sizeof(line), &interval, '\0', false);  // No wrap, no channel name
              consume_both_flags();
              break;
            }
          case Hat:
            {
              // 3-Cornered Hat mode: chA, chB, and chC (synthesized)
              // chC = int(chB) + (chB - chA) - properly handle negative differences
              
              // Print chA and chB timestamps
              char line[64];
              print_timestamp(line, sizeof(line), &channels[0].timestamp, (char)channels[0].name);
              print_timestamp(line, sizeof(line), &channels[1].timestamp, (char)channels[1].name);
              
              // Calculate chC = int(chB) + (chB - chA)
              Timestamp64 interval = timestamp_difference(&channels[1].timestamp, &channels[0].timestamp);
              Timestamp64 chC;
              
              // chC uses the integer seconds from chB, plus the fractional difference
              chC.seconds = channels[1].timestamp.seconds;  // int(chB) - integer seconds from chB
              chC.picos = interval.picos;              // (chB - chA) fractional part
              
              // Handle negative fractional differences (interval.seconds < 0 means negative difference)
              if (interval.seconds < 0) {
                // The fractional part is in complement representation, convert to normal
                if (interval.picos != 0) {
                  chC.picos = PS_PER_SEC - interval.picos;
                  // Since we're subtracting from the integer seconds, borrow if needed
                  chC.seconds -= 1;
                }
              }
              
              // Print chC (synthesized)
              print_timestamp(line, sizeof(line), &chC, 'C');
              
              consume_both_flags();
              break;
            }
          default: break;
        }
      }
    }


    // Restart handling moved to beginning of loop() function

  }  // while (1) loop

}  // main loop()


/****************************************************************
 * Interrupt Service Routines
 ****************************************************************/

// ISR for timer. Capture PICcount on each channel's STOP 0->1 transition.
void coarseTimer() {
  PICcount++;
}

void catch_stop0() {
  channels[0].PICstop = PICcount;
}

void catch_stop1() {
  channels[1].PICstop = PICcount;
}

