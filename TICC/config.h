#ifndef CONFIG_H
#define CONFIG_H

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <EEPROM.h>

enum MeasureMode : unsigned char {Timestamp, Interval, Period, timeLab, Debug};

// system defines -- do not change!
#define BOARD_REVISION    'D'             // production version is 'D'
#define EEPROM_VERSION    (byte)     98              // eeprom struct version
#define CONFIG_START      (byte)     0x00            // first byte of config in eeprom
#define PS_PER_SEC        (int64_t)  1000000000000   // ps/s

// default values for config struct
#define MEASUREMENT_MODE    (MeasureMode) 0     // Measurement mode -- 0 is Timestamp
#define CLOCK_HERTZ         (int64_t) 10000000  // 10 MHz
#define PICTICK_PICOS       (int64_t) 100000000 // 100us
#define CAL_PER             (int16_t) 20        // CAL_PERIODS (2, 10, 20, 40)
#define TIMEOUT_HEX         (byte)    0x05      // measurement timeout
#define SYNC                (char)    'M'       // (M)aster or (S)lave
#define START_EDGE_0        (char)    'R'       // (R)ising or (F)alling
#define START_EDGE_1        (char)    'R'       // (R)ising or (F)alling
#define TIME_DILATION_0     (int64_t) 2500      // SWAG that seems to work
#define TIME_DILATION_1     (int64_t) 2500      // SWAG that seems to work
#define FIXED_TIME2_0       (int64_t) 0
#define FIXED_TIME2_1       (int64_t) 0
#define FUDGE0_0            (int64_t) 0
#define FUDGE0_1            (int64_t) 0

void print_MeasureMode(MeasureMode x);

// configuration structure type
struct config_t {
  byte       VERSION = EEPROM_VERSION; // one byte   
  char       SW_VERSION[17];           // up to 16 bytes plus term
  char       BOARD_REV;            // one byte   
  char       BOARD_ID[17];             // up to 16 bytes plus term
  
  // global settings:
  MeasureMode MODE;                 // (T)imestamp, time (I)nterval
                                    // Time(L)ab, (P)eriod, (D)ebug (default 'T')
  int64_t    CLOCK_HZ;              // clock in Hz (default 10 000 000)
  int64_t    PICTICK_PS;            // coarse tick (default 100 000 000)
  int16_t    CAL_PERIODS;           // cal periods 2, 10, 20, 40 (default 20)
  int16_t    TIMEOUT;              // timeout for measurement in hex (default 0x05)
  char       SYNC_MODE;                // one byte:  'M' for master,  'S' for slave
  
  // per-channel settings, arrays of 2 for channels A and B:
  char       START_EDGE[2];         // (R)ising (default) or (F)alling edge 
  int64_t    TIME_DILATION[2];      // time dilation factor (default 2500)
  int64_t    FIXED_TIME2[2];        // if >0 use to replace time2 (default 0)
  int64_t    FUDGE0[2];             // fudge factor (ps) (default 0)
  
};

void UserConfig(struct config_t *config);

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

void eeprom_write_config_default (uint16_t offset);
void print_config (config_t x);


#endif	/* CONFIG_H */
