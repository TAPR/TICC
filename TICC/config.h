#ifndef CONFIG_H
#define CONFIG_H

// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.79 14 October 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <EEPROM.h>

#define CONFIG_START      (byte)     0 // first byte of config in eeprom

// configuration structure type
struct config_t {
  const byte EEPROM_VERSION = 0x01; // one byte   
  char       SW_VERSION[17];        // up to 16 bytes plus term
  char       BOARD_SER_NUM[17];     // up to 16 bytes plus term
  
  // global settings:
  char       MODE;                  // time(S)tamp, time (I)nterval
                                    // Time(L)ab, (P)eriod (default 'S')
  int64_t    CLOCK_HZ;              // clock in Hz (default 10 000 000)
  int64_t    PICTICK_PS;            // coarse tick (default 100 000 000)
  int16_t    CAL_PERIODS;           // cal periods 2, 10, 20, 40 (default 20)
  // per-channel settings, arrays of 2 for channels A and B:
  char       START_EDGE[2];         // (R)ising (default) or (F)alling edge
  int64_t    TIME_DILATION[2];      // time dilation factor (default TBD)
  int64_t    FIXED_TIME2[2];        // if >0 use to replace time2 (default 0)
  int64_t    FUDGE0[2];             // fudge factor (ps) (default 0)
};


// These allow us to read/write struct in eeprom
template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
    const byte* p = (const byte*)(const void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
    return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
    return i;
}

// read and write config struct in eeprom
uint16_t eeprom_write_config(uint16_t offset, config_t x);

uint16_t eeprom_read_config(uint16_t offset, config_t x);

uint16_t eeprom_write_default (uint16_t offset);

config_t copy_config_default ();

#endif	/* CONFIG_H */
