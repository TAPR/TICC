// TICC.h -- System-wide definitions and declarations for TICC
// Copyright John Ackermann N8UR 2016-2025
// Licensed under BSD 2-clause license

#ifndef TICC_H
#define TICC_H

#include <Arduino.h>
#include "config.h"

// System-wide constants
extern const char SW_VERSION[17];
extern const char SW_TAG[8];

// System-wide variables
extern uint8_t config_changed;
extern config_t config;
extern config_t config_backup;

// Performance-critical variables: local copies for hot path efficiency
// These are copied from config at startup to avoid struct access overhead in tight loops
extern volatile int64_t PICcount;      // Coarse timer tick count (incremented by ISR)
extern int64_t PICTICK_PS;             // Picoseconds per coarse tick (used in calculate_timestamp)
extern int64_t CLOCK_PERIOD;           // Picoseconds per TDC clock tick (used in tdc7200 read)
extern int16_t CAL_PERIODS;            // TDC calibration periods (used in tdc7200 read)
extern MeasureMode MODE;               // Current measurement mode (checked in main loop)
extern MeasureMode lastMODE;           // Previous measurement mode (for change detection)

// Setup and restart control variables
extern uint8_t skip_config_prompt_once;  // Skip config prompt on next setup (set by some commands)
extern volatile uint8_t request_restart; // Request system restart (set by config changes)
extern uint8_t just_restarted;           // Flag indicating system just restarted
extern uint8_t config_requested;         // Flag indicating config menu was requested

// System-wide utility tables
static const uint32_t POW10_TABLE[10] PROGMEM = {
  1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
};

// System-wide function declarations
extern void configPrint(const char* msg);
extern void configPrintln(const char* msg);
extern void configPrintProg(const char* msg);
extern void configPrintlnProg(const char* msg);
extern void eeprom_write_config();
extern void eeprom_read_config();
extern void eeprom_clear();
extern struct config_t defaultConfig();
extern bool check_reference_clock();
extern bool both_channels_ready();
extern bool poll_gating_ok();
extern void consume_both_flags();
extern bool check_poll_gating();

#endif // TICC_H
