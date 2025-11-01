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
 * See docs/TICC_architecture.md for details about how the TICC
 * firmware works.
 */

extern const char SW_VERSION[17] = "20251101.1";
extern const char SW_TAG[8] = "RELEASE";

#include <stdint.h>             // define uint16_t, uint32_t
#include <SPI.h>                // SPI support
#include <EEPROM.h>             // eeprom library
#include "EnableInterrupt.h"    // use faster interrupt library
#include "board.h"              // LED macros and Arduino pin definitions
#include "config.h"             // config and eeprom
#include "tdc7200.h"            // TDC registers and structures
#include "timestamps.h"         // timestamp utility functions
#include "print.h"              // optimized 64-bit printing routines
#include "setup.h"              // initialization functions
#include "utils.h"              // utility functions
#include "config_menu_text.h"   // PROGMEM menu strings
#include "TICC.h"               // system-wide definitions

// Performance-critical variables: local copies of config values used in hot path
// These are copied from config at startup for faster access during timestamp processing
volatile int64_t PICcount;      // Coarse timer tick count (incremented by ISR)
int64_t PICTICK_PS;             // Picoseconds per coarse tick (used in calculate_timestamp)
int64_t CLOCK_PERIOD;           // Picoseconds per TDC clock tick (used in tdc7200 read)
int16_t CAL_PERIODS;            // TDC calibration periods (used in tdc7200 read)
MeasureMode MODE, lastMODE;     // Current and previous measurement mode (checked in main loop)

// Configuration and system control variables
config_t config;                        // Main configuration structure (stored in EEPROM)
config_t config_backup;                 // Backup of config before changes (for restart detection)
uint8_t config_changed = 0;             // Flag indicating config was modified during menu session
uint8_t config_requested = 0;           // Flag indicating config menu was requested
uint8_t skip_config_prompt_once = 0;    // Skip config prompt on next setup (used by some commands)
volatile uint8_t request_restart = 0;   // Request system restart (set when config changes require it)
uint8_t just_restarted = 1;             // Flag indicating system just restarted (skip first ref check)

// Serial input and control
char serial_char = 0;                   // Last character read from serial (for config and poll gating)

// struct that carries information for each TDC channel
static tdc7200Channel channels[] = {
  tdc7200Channel('0', ENABLE_0, INTB_0, CSB_0, STOP_0, LED_0),
  tdc7200Channel('1', ENABLE_1, INTB_1, CSB_1, STOP_1, LED_1),
};

/****************************************************************
 * Arduino IDE requires a setup() function but we don't use it.
 * Actual setup is done in ticc_setup() in setup.cpp
 ****************************************************************/
void setup() {}

/****************************************************************
 * Interrupt Service Routines
 ****************************************************************/
// ISR for timer. Capture PICcount on each channel's STOP 0->1 transition.
void coarseTimer() {
  PICcount++;
}

// ISRs to grab the coarse clock count on TDC STOP
void catch_stop0() {
  channels[0].PICstop = PICcount;
}

void catch_stop1() {
  channels[1].PICstop = PICcount;
}

/****************************************************************
 * main function
 ****************************************************************/
void loop() {
  ticc_setup();  // initialize and optionally go to config
  
  // Check if restart was requested after config processing
  if (request_restart) {
    request_restart = 0;  // Clear the flag
    just_restarted = 1;   // Set flag for next loop iteration
    configPrintlnProg(startup_restart_retry);
    return;  // Exit loop to trigger fresh ticc_setup() call
  }
  
  /****************************************************************
   * processing loop starts here!
   ****************************************************************/
  while (1) {

    // Single serial read for both config and poll
    bool output_allowed = (!config.POLL_CHAR);  // If no poll char set, always allow output
    
    if (Serial.available() > 0) {
      serial_char = Serial.read();
      
      // Check for config menu request
      if (serial_char == '#') {
        while (Serial.available()) (void)Serial.read(); // Clear serial buffer
        if (handle_config_request()) {
          return; // Restart needed
        }
      }
      
      // Check for poll character (if configured)
      if (config.POLL_CHAR && serial_char == config.POLL_CHAR) {
        output_allowed = true;  // Allow output for this iteration
      }
    }

    // Check reference clock and handle reference lost condition
    if (!check_reference_clock()) {
      return; // Reference lost, restart requested
    }

    // Check both channels simultaneously for better timestamp ordering
    bool ready_ch0 = (digitalRead(channels[0].INTB) == 0);
    bool ready_ch1 = (digitalRead(channels[1].INTB) == 0);
    
    // Both channels ready - process both simultaneously for better ordering
    if (ready_ch0 && ready_ch1) {
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
        
        CLR_LED_0; CLR_EXT_LED_0;
        CLR_LED_1; CLR_EXT_LED_1;
      }
      
    } else if (ready_ch0) {
      SET_LED_0; SET_EXT_LED_0;
      
      if (config.MODE == Binary) {
        if (process_binary_mode(&channels[0])) {
          CLR_LED_0; CLR_EXT_LED_0;
        }
      } else {
        calculate_timestamp(&channels[0], PICTICK_PS);
        CLR_LED_0; CLR_EXT_LED_0;
        
        // Check if channel 1 became ready during processing
        if (digitalRead(channels[1].INTB) == 0) {
          SET_LED_1; SET_EXT_LED_1;
          calculate_timestamp(&channels[1], PICTICK_PS);
          CLR_LED_1; CLR_EXT_LED_1;
        }
      }
      
    } else if (ready_ch1) {
      SET_LED_1; SET_EXT_LED_1;
      
      if (config.MODE == Binary) {
        if (process_binary_mode(&channels[1])) {
          CLR_LED_1; CLR_EXT_LED_1;
        }
      } else {
        calculate_timestamp(&channels[1], PICTICK_PS);
        CLR_LED_1; CLR_EXT_LED_1;
        
        // Check if channel 0 became ready during processing
        if (digitalRead(channels[0].INTB) == 0) {
          SET_LED_0; SET_EXT_LED_0;
          calculate_timestamp(&channels[0], PICTICK_PS);
          CLR_LED_0; CLR_EXT_LED_0;
        }
      }
    }

    // Skip output processing if no new timestamps are ready
    if (!channels[0].new_ts_ready && !channels[1].new_ts_ready) {
      continue; // skip output processing
    }
     
    // or if not output_allowed, but here consume flags anyway
     if (!output_allowed) {
      channels[0].new_ts_ready = 0; // consume flags
      channels[1].new_ts_ready = 0;
      continue; // skip output processing
    }

    // All modes: throw away first timestamp (totalize == 1) because it may be garbage
    // Non-Period modes: need totalize >= 2 (after throwing away first)
    // Period mode: need two good timestamps after first, so need totalize >= 3
    uint8_t min_totalize = (config.MODE == Period) ? 3 : 2;
    if (channels[0].new_ts_ready && channels[0].totalize < min_totalize) {
      channels[0].new_ts_ready = 0; // consume flag
    }
    if (channels[1].new_ts_ready && channels[1].totalize < min_totalize) {
      channels[1].new_ts_ready = 0; // consume flag
    }
    
    // Skip if no valid timestamps remain after consuming invalid ones
    if (!channels[0].new_ts_ready && !channels[1].new_ts_ready) {
      continue; // skip output processing
    }

    // Interval and Hat modes require both channels to be ready
    // If only one is ready, skip output (don't consume flags - wait for both)
    if (config.MODE == Interval || config.MODE == Hat) {
      if (!channels[0].new_ts_ready || !channels[1].new_ts_ready) {
        continue; // skip output processing (flags remain set for next iteration)
      }
    }

   
    // Call output function based on mode
    // output functions are in print.cpp
    switch (config.MODE) {
      case Timestamp:
        print_timestamp_mode(channels);
        break;
      case Period:
        print_period_mode(channels);
        break;
      case Debug:
        print_debug_mode(channels);
        break;
      case Paired_Timestamp:
        print_paired_timestamp_mode(channels);
        break;
      case Interval:
        print_interval_mode(channels);
        break;
      case Hat:
        print_hat_mode(channels);
        break;
      default:
        // Null or unknown mode - do nothing
        break;
      }

  }  // while (1) loop

}  // main loop()
