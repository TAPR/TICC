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

extern const char SW_VERSION[17] = "20251031.1";
extern const char SW_TAG[8] = "RELEASE";

#include <stdint.h>             // define unint16_t, uint32_t
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
    Serial.println("# Restart requested, reinitializing system...");
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
    
    // or if both channels have invalid first timestamps (totalize <= 1)
    if ((channels[0].totalize <= 1) && (channels[1].totalize <= 1)) {
      channels[0].new_ts_ready = 0; // consume flags
      channels[1].new_ts_ready = 0; // consume flags
      continue; // skip output processing
    }

    // Timestamp mode: print each timestamp in timestamp order (earlier timestamp first)
    if ((config.MODE == Timestamp) && output_allowed) {
      // Both ready: print earlier timestamp first, then later one
      if (channels[0].new_ts_ready && channels[1].new_ts_ready) {
        // Check if channel 0 timestamp < channel 1 timestamp (channel 0 is earlier)
        bool ch0_earlier = (channels[0].timestamp.seconds < channels[1].timestamp.seconds) ||
                        ((channels[0].timestamp.seconds == channels[1].timestamp.seconds) && 
                         (channels[0].timestamp.picos < channels[1].timestamp.picos));
        
        int first_ch = ch0_earlier ? 0 : 1;
        int second_ch = ch0_earlier ? 1 : 0;
        
        char line[64];
        print_timestamp(line, sizeof(line), &channels[first_ch].timestamp, (char)channels[first_ch].name);
        print_timestamp(line, sizeof(line), &channels[second_ch].timestamp, (char)channels[second_ch].name);
        
        channels[0].new_ts_ready = 0;  // consume both
        channels[1].new_ts_ready = 0;
        } else {
        // Only one ready: print that one
        for (int ci = 0; ci < 2; ++ci) {
          if (channels[ci].new_ts_ready) {
            char line[64];
            print_timestamp(line, sizeof(line), &channels[ci].timestamp, (char)channels[ci].name);
            channels[ci].new_ts_ready = 0;  // consume
          }
        }
      }
    }

    // Period mode: print period (timestamp - previous_timestamp) for each channel
    if ((config.MODE == Period) && output_allowed) {
      // Static buffer to store previous timestamps for each channel
      static Timestamp64 prev_timestamp[2] = {{0, 0}, {0, 0}};
      
      for (int ci = 0; ci < 2; ++ci) {
        if (channels[ci].new_ts_ready && (channels[ci].totalize > 2)) {
          // Calculate period: current timestamp - previous timestamp from buffer
          Timestamp64 period = timestamp_difference(&channels[ci].timestamp, 
              &prev_timestamp[ci]);
          char line[64];
          print_timestamp(line, sizeof(line), &period, (char)channels[ci].name, false);  // No wrap
          
          channels[ci].new_ts_ready = 0;  // consume
          
          // Update buffer with current timestamp for next calculation
          prev_timestamp[ci] = channels[ci].timestamp;
        }
      }
    }

    // Debug mode: output raw TDC7200 register values plus calculated timestamp
    if ((config.MODE == Debug) && output_allowed) {
      for (int ci = 0; ci < 2; ++ci) {
        if (channels[ci].new_ts_ready && (channels[ci].totalize > 2)) {
          char line[128];
          size_t n = 0;
          
          // Raw TDC7200 values (6 digits each)
          n += sprintf(line + n, "%06lu ", (unsigned long)channels[ci].time1Result);
          n += sprintf(line + n, "%06lu ", (unsigned long)channels[ci].time2Result);
          n += sprintf(line + n, "%06lu ", (unsigned long)channels[ci].clock1Result);
          n += sprintf(line + n, "%06lu ", (unsigned long)channels[ci].cal1Result);
          n += sprintf(line + n, "%06lu ", (unsigned long)channels[ci].cal2Result);
          
          // Write the line so far
          Serial.write((const uint8_t*)line, n);
          
          // PICstop (int64_t - use new print_int64 function)
          print_int64(channels[ci].PICstop, false);
          Serial.write(' ');
          
          // tof (int64_t - use new print_int64 function) 
          print_int64(channels[ci].tof, false);
          Serial.write(' ');
          
          // timestamp (no WRAP, PLACES=12) with channel name
          print_timestamp(line, sizeof(line), &channels[ci].timestamp, (char)channels[ci].name, false);
          
          channels[ci].new_ts_ready = 0;  // consume
        }
      }
    }

    // Paired_Timestamp mode: assemble and print pairs without timeout
    if (config.MODE == Paired_Timestamp) {
      // Two-slot buffer; accumulate two successive samples (across either channel)
      // then emit exactly two lines per pair in fixed order: if both channels are
      // present print channel 0 then channel 1; if both are the same channel, print that
      // channel twice.
      struct PairSlot {
        Timestamp64 t;
        uint8_t ch;
      };
      static PairSlot ts_pair[2];
      static uint8_t ts_pair_count = 0;

      // Ingest any fresh samples into the pair buffer
      for (int ci = 0; ci < 2; ++ci) {
        if (channels[ci].new_ts_ready) {
          if (ts_pair_count < 2) {
            ts_pair[ts_pair_count].t = channels[ci].timestamp;
            ts_pair[ts_pair_count].ch = (uint8_t)ci;
            ts_pair_count++;
          }
          channels[ci].new_ts_ready = 0;  // consume
        }
      }

      // If we have a complete pair, emit in fixed order with poll gating
      if ((ts_pair_count == 2) && output_allowed) {
          // Determine composition and enforce channel 0 then channel 1 order when both present
          if ((ts_pair[0].ch == 0 && ts_pair[1].ch == 1) || (ts_pair[0].ch == 1 && ts_pair[1].ch == 0)) {
            // Mixed channels: find channel 0 then channel 1
            const PairSlot *ch0 = (ts_pair[0].ch == 0) ? &ts_pair[0] : &ts_pair[1];
            const PairSlot *ch1 = (ts_pair[0].ch == 1) ? &ts_pair[0] : &ts_pair[1];
            // Print channel 0 timestamp
            {
              char line[64];
              print_timestamp(line, sizeof(line), &ch0->t, (char)channels[0].name);
            }
            // Print channel 1 timestamp
            {
              char line[64];
              print_timestamp(line, sizeof(line), &ch1->t, (char)channels[1].name);
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

    // Shared pairing logic for Interval and 3-Cornered Hat modes
    if (both_channels_ready() && output_allowed) {
        switch (config.MODE) {
          case Interval:
            {
              // Calculate time interval from channel 0 to channel 1
              Timestamp64 interval = timestamp_difference(&channels[1].timestamp, &channels[0].timestamp);
              char line[64];
              print_timestamp(line, sizeof(line), &interval, '\0', false);  // No wrap, no channel name
              consume_both_flags();
              break;
            }
          case Hat:
            {
              // 3-Cornered Hat mode: channel 0, channel 1, and chC (synthesized)
              // chC = int(channel 1) + (channel 1 - channel 0) - properly handle negative differences
              
              // Print channel 0 and channel 1 timestamps
              char line[64];
              print_timestamp(line, sizeof(line), &channels[0].timestamp, (char)channels[0].name);
              print_timestamp(line, sizeof(line), &channels[1].timestamp, (char)channels[1].name);
              
              // Calculate chC = int(channel 1) + (channel 1 - channel 0)
              Timestamp64 interval = timestamp_difference(&channels[1].timestamp, &channels[0].timestamp);
              Timestamp64 chC;
              
              // chC uses the integer seconds from channel 1, plus the fractional difference
              chC.seconds = channels[1].timestamp.seconds;  // int(channel 1) - integer seconds from channel 1
              chC.picos = interval.picos;              // (channel 1 - channel 0) fractional part
              
              // Handle negative fractional differences (interval.seconds < 0 means negative difference)
              if (interval.seconds < 0) {
                // The fractional part is in complement representation, convert to normal
                if (interval.picos != 0) {
                  chC.picos = PS_PER_SEC - interval.picos;
                  // Since we're subtracting from the integer seconds, borrow if needed
                  chC.seconds -= 1;
                }
              }
              
              // Print synthesized third channel using configured name (default 'C')
              char third_ch_name = config.NAME_3CH;
              if (third_ch_name == 0) third_ch_name = 'C';  // Fallback to default
              print_timestamp(line, sizeof(line), &chC, third_ch_name);
              
              consume_both_flags();
              break;
            }
          default: break;
        }
      }

  }  // while (1) loop

}  // main loop()
