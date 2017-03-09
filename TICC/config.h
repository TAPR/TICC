#ifndef CONFIG_H
#define CONFIG_H

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <EEPROM.h>
#define PS_PER_SEC                (int64_t)  1000000000000   // ps/s

enum MeasureMode : unsigned char {Timestamp, Interval, Period, timeLab, Debug, Null};

/*****************************************************************/
// system defines
#define BOARD_REVISION            'D'                   // production version is 'D'
#define EEPROM_VERSION            (byte)     8          // eeprom struct version
#define CONFIG_START              (byte)     0x00       // first byte of config in eeprom
#define SER_NUM_START             (int16_t)  0x0FF0     // first byte of serial number in eeprom
/*****************************************************************/
// default values for config struct
#define DEFAULT_MODE              (MeasureMode) 0       // Measurement mode -- 0 is Timestamp
#define DEFAULT_POLL_CHAR         (char)    0x00        // In poll mode, wait for this before output
#define DEFAULT_CLOCK_HZ          (int64_t) 10000000    // 10 MHz
#define DEFAULT_PICTICK_PS        (int64_t) 100000000   // 100us
#define DEFAULT_CAL_PERIODS       (int16_t) 20          // CAL_PERIODS (2, 10, 20, 40)
#define DEFAULT_TIMEOUT           (int16_t)  0x05       // measurement timeout
#define DEFAULT_SYNC_MODE         (char)    'M'         // (M)aster or (S)lave
#define DEFAULT_START_EDGE_0      (char)    'R'         // (R)ising or (F)alling
#define DEFAULT_START_EDGE_1      (char)    'R'         // (R)ising or (F)alling
#define DEFAULT_TIME_DILATION_0   (int64_t) 2500        // SWAG that seems to work
#define DEFAULT_TIME_DILATION_1   (int64_t) 2500        // SWAG that seems to work
#define DEFAULT_FIXED_TIME2_0     (int64_t) 0           // 0 to calculate, or fixed (~1135)
#define DEFAULT_FIXED_TIME2_1     (int64_t) 0           // 0 to calculate, or fixed (~1135)
#define DEFAULT_FUDGE0_0          (int64_t) 0           // Fudge channel A value (ps)
#define DEFAULT_FUDGE0_1          (int64_t) 0           // Fudge channel B value (ps)

/*****************************************************************/
// configuration structure type
struct config_t {
  byte       VERSION = EEPROM_VERSION;  // one byte   
  char       SW_VERSION[17];            // up to 16 bytes plus term
  char       BOARD_REV;                 // one byte   
  char       SER_NUM[17];              // up to 16 bytes plus term
  
  // global settings:
  MeasureMode MODE;                     // (T)imestamp, time (I)nterval
                                        // Time(L)ab, (P)eriod, (D)ebug (default 'T')
  char       POLL_CHAR;                 // In poll mode, wiat for this before output
  int64_t    CLOCK_HZ;                  // clock in Hz (default 10 000 000)
  int64_t    PICTICK_PS;                // coarse tick (default 100 000 000)
  int16_t    CAL_PERIODS;               // cal periods 2, 10, 20, 40 (default 20)
  int16_t    TIMEOUT;                   // timeout for measurement in hex (default 0x05)
  char       SYNC_MODE;                 // one byte:  'M' for master,  'S' for slave
  
  // per-channel settings, arrays of 2 for channels A and B:
  char       START_EDGE[2];            // (R)ising (default) or (F)alling edge 
  int64_t    TIME_DILATION[2];         // time dilation factor (default 2500)
  int64_t    FIXED_TIME2[2];           // if >0 use to replace time2 (default 0)
  int64_t    FUDGE0[2];                // fudge factor (ps) (default 0)
  
};

/*****************************************************************/
// exposed function prototypes
void UserConfig(struct config_t *config);
void print_MeasureMode(MeasureMode x);
void printHzAsMHz(int64_t x);
void get_serial_number();
void eeprom_clear();

/*****************************************************************/
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
void software_reset();


#endif	/* CONFIG_H */
