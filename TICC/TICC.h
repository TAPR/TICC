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
extern char SER_NUM[17];
extern uint8_t config_changed;
extern config_t config;
extern config_t config_backup;

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
