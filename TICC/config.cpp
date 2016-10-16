// config.cpp -- set/read/write configuration
// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>           // define unint16_t, uint32_t
#include <EEPROM.h>           // read/write EEPROM
#include "ticc.h"             // general config
#include "misc.h"             // random functions
#include "board.h"            // Arduino pin definitions
#include "config.h"           // config and eeprom
#include "tdc7200.h"          // TDC registers and structures
#include "UserConfig.h"       // user configuration of TICC

extern const char SW_VERSION[17];
extern const char BOARD_SER_NUM[17];

// read and write config struct in eeprom
uint16_t eeprom_write_config(uint16_t offset, config_t x) {
  uint16_t num_bytes;
  num_bytes = EEPROM_writeAnything(offset, x);
  return num_bytes;
}

uint16_t eeprom_read_config(uint16_t offset, config_t x) {
  uint16_t num_bytes;
  num_bytes = EEPROM_readAnything(offset, x);
  return num_bytes;
}

uint16_t eeprom_write_default (uint16_t offset) {
  config_t x;
  strncpy(x.SW_VERSION,SW_VERSION,sizeof(SW_VERSION));
  strncpy(x.BOARD_SER_NUM,BOARD_SER_NUM,sizeof(BOARD_SER_NUM));
  x.MODE = 0; // MODE -- 0 is timestamp, 3 is TimeLab 
  x.CLOCK_HZ = 10000000; // 10 MHz
  x.PICTICK_PS = 100000000; // 100us
  x.CAL_PERIODS = 20; // CAL_PERIODS (2, 10, 20, 40)
  x.TIME_DILATION[0] = 2500;  // 2500 seems right for chA on C1
  x.TIME_DILATION[1] = 2500;
  x.FIXED_TIME2[0] = 0;
  x.FIXED_TIME2[1] = 0;
  x.FUDGE0[0] = 0;
  x.FUDGE0[1] = 0;
  eeprom_write_config(offset,x);
}

config_t copy_config_default () {
  config_t x;
  strncpy(x.SW_VERSION,SW_VERSION,sizeof(SW_VERSION));
  strncpy(x.BOARD_SER_NUM,BOARD_SER_NUM,sizeof(BOARD_SER_NUM));
  x.MODE = 0; // MODE -- 0 is timestamp, 3 is TimeLab 
  x.CLOCK_HZ = 10000000; // 10 MHz
  x.PICTICK_PS = 100000000; // 100us
  x.CAL_PERIODS = 20; // CAL_PERIODS (2, 10, 20, 40)
  x.TIME_DILATION[0] = 2500;  // 2500 seems right for chA on C1
  x.TIME_DILATION[1] = 2500;
  x.FIXED_TIME2[0] = 0;
  x.FIXED_TIME2[1] = 0;
  x.FUDGE0[0] = 0;
  x.FUDGE0[1] = 0;
  return x;
}
