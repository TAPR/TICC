// utils.cpp -- TICC utility functions
//
// Copyright John Ackermann N8UR 2016-2025
// Licensed under BSD 2-clause license

#include <Arduino.h>
#include "board.h"
#include "config.h"
#include "tdc7200.h"
#include "utils.h"

// External global variables from TICC.ino
extern volatile int64_t PICcount;
extern config_t config;
extern tdc7200Channel channels[];
extern uint8_t skip_config_prompt_once;
extern uint8_t just_restarted;
extern uint8_t config_requested;
extern uint8_t config_changed;

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
// Note: Uses serial_char that was already read at top of main loop for efficiency
bool check_poll_gating() {
  // Fast path: if no poll character is configured, always allow output
  if (!config.POLL_CHAR) {
    return true;
  }
  
  // Poll character is configured - check if it was received
  // serial_char is set by main loop's single Serial.read()
  extern char serial_char;
  static bool poll_character_received = false;
  
  if (serial_char == config.POLL_CHAR) {
    poll_character_received = true;
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

// Handle config menu request
// Returns true if restart is needed, false otherwise
bool handle_config_request() {
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
  
  // Clear any remaining characters after config menu exits
  while (Serial.available()) (void)Serial.read();
  
  // Handle exit from config menu
  // Always handle config changes (whether written to EEPROM or not)
  handle_config_change_exit();
  
  // If restart required, return true to signal restart
  if (config_change_requires_restart()) {
    skip_config_prompt_once = 1;
    just_restarted = 1;  // Set flag for next loop iteration
    return true;  // Restart needed
  }
  
  // Restart measurements after config changes
  Serial.println("# Restarting measurements...");
  start_all_measurements();
  
  // Clear the config_changed flag for next time
  config_changed = 0;
  
  return false;  // No restart needed
}
