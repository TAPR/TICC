// config.cpp -- set/read/write configuration

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2020
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Portions Copyright David McQuate WA8YWQ 2016
// Licensed under BSD 2-clause license

#include <stdint.h>           // define unint16_t, uint32_t
#include <ctype.h>
#include <EEPROM.h>           // read/write EEPROM
#include <SPI.h>

#include "misc.h"             // random functions
#include "config.h"           // config and eeprom
#include "board.h"            // Arduino pin definitions
#include "tdc7200.h"          // TDC registers and structures

extern const char SW_VERSION[17]; // set in TICC.ino
extern const char SW_TAG[6];      // set in TICC.ino
// SER_NUM moved to config_core.cpp

// External variables for config change tracking
extern uint8_t config_changed;

// Macro to mark config as changed
#define MARK_CONFIG_CHANGED() do { config_changed = 1; } while(0)

// Serial I/O helper functions moved to config_core.cpp
// Parsing functions moved to config_core.cpp
// Legacy input functions removed - replaced by readLine() in unified menu system
// Legacy getInt64 function removed - replaced by parseInt64Simple() and parseDecimalScaled() in unified menu system

/******************************************************************************
 * Simple helper functions to reduce repetition in menu code
 *******************************************************************************/

// getInputOrPrompt moved to config_core.cpp
// printHzAsMHz moved to config_core.cpp

/******************************************************************************
 * Legacy menu functions removed - replaced by unified menu system
*******************************************************************************/

// modeToChar moved to config_core.cpp
// defaultConfig moved to config_core.cpp
// initializeConfig function removed - unused

// processCommand moved to config_menu.cpp

// doSetupMenu moved to config_display.cpp

// UserConfig moved to config_display.cpp

// Pretty-print mode
// print_MeasureMode moved to config_core.cpp

// eeprom_write_config_default moved to config_core.cpp

// print_config moved to config_core.cpp

// get_serial_number moved to config_core.cpp

// eeprom_clear moved to config_core.cpp
