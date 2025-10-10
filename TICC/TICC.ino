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

// 9 October 2025 - version 20251009.1
extern const char SW_VERSION[17] = "20251009.2";
extern const char SW_TAG[6] = "BETA";

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

volatile int64_t PICcount;
int64_t CLOCK_HZ;
int64_t PICTICK_PS;
int64_t CLOCK_PERIOD;
int16_t CAL_PERIODS;
int16_t WRAP;
int64_t ticksPerSecond;        // number of coarse ticks per second

config_t config;
MeasureMode MODE, lastMODE;
uint8_t skip_config_prompt_once = 0;  
volatile uint8_t request_restart = 0;
uint8_t just_restarted = 1; 

// Configuration change tracking
static config_t config_backup;  // Backup of config before changes
uint8_t config_changed = 0;     // Flag indicating config was modified (global for config.cpp access)
uint8_t config_requested = 0;  

// Serial input handling
char serial_char = 0;  // Last character read from serial (for config and poll gating)

static tdc7200Channel channels[] = {
  tdc7200Channel('0', ENABLE_0, INTB_0, CSB_0, STOP_0, LED_0),
  tdc7200Channel('1', ENABLE_1, INTB_1, CSB_1, STOP_1, LED_1),
};

void setup() {} // we don't use default setup(), but ticc_setup() in setup.cpp

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

    // Single serial read for both config and poll gating (optimization)
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

    // Early exit optimization: skip output processing if no new timestamps are ready
    // or if both channels have invalid first timestamps (totalize <= 1)
    if (!channels[0].new_ts_ready && !channels[1].new_ts_ready) {
      continue; // Skip all output processing
    }
    
    // Skip invalid first timestamp(s) - at least one channel must have totalize > 1
    if ((channels[0].totalize <= 1) && (channels[1].totalize <= 1)) {
      // Both channels still on first (invalid) timestamp, consume flags and skip
      channels[0].new_ts_ready = 0;
      channels[1].new_ts_ready = 0;
      continue;
    }

    /*
    // TEMPORARY: Speed test - minimal output to measure max processing rate (ch0 only)
    if (channels[0].new_ts_ready) {
      Serial.println("@");  // 3 bytes: '@' '\r' '\n'
      channels[0].new_ts_ready = 0;
      continue; // Skip all normal print routines
    }
    continue; // Skip if ch1 had data but we're only testing ch0
    */
    
    // Timestamp mode: print each timestamp in timestamp order (earlier timestamp first)
    if ((config.MODE == Timestamp) && output_allowed) {
      if (channels[0].new_ts_ready && channels[1].new_ts_ready) {
        // Both ready: print earlier timestamp first, then later one
        // Inline comparison for performance (avoids function call overhead)
        // Check if chA timestamp < chB timestamp (chA is earlier)
        bool A_earlier = (channels[0].timestamp.seconds < channels[1].timestamp.seconds) ||
                        ((channels[0].timestamp.seconds == channels[1].timestamp.seconds) && 
                         (channels[0].timestamp.picos < channels[1].timestamp.picos));
        
        int first_ch = A_earlier ? 0 : 1;
        int second_ch = A_earlier ? 1 : 0;
        
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
          // Determine composition and enforce chA then chB order when both present
          if ((ts_pair[0].ch == 0 && ts_pair[1].ch == 1) || (ts_pair[0].ch == 1 && ts_pair[1].ch == 0)) {
            // Mixed channels: find A then B
            const PairSlot *A = (ts_pair[0].ch == 0) ? &ts_pair[0] : &ts_pair[1];
            const PairSlot *B = (ts_pair[0].ch == 1) ? &ts_pair[0] : &ts_pair[1];
          // Print chA timestamp
            {
              char line[64];
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

    // Shared pairing logic for Interval and 3-Cornered Hat modes
    if (both_channels_ready() && output_allowed) {
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

  }  // while (1) loop

}  // main loop()
