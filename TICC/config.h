#ifndef CONFIG_H
#define CONFIG_H

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <EEPROM.h>

#define PS_PER_SEC                (int64_t)  1000000000000   // ps/s

enum MeasureMode : unsigned char {Timestamp, Paired_Timestamp, Binary, Interval, Period, Hat, Debug, Null};

/*****************************************************************/
// system defines
#define BOARD_REVISION            'D'                   // production version is 'D'
#define EEPROM_VERSION            (byte)     13         // eeprom struct version (13: added NAME_3CH for 3-Corner Hat mode)
#define CONFIG_START              (byte)     0x00       // first byte of config in eeprom
#define SER_NUM_START             (int16_t)  0x0FF0     // first byte of serial number in eeprom
/*****************************************************************/
// default values for config struct
#define DEFAULT_MODE              (MeasureMode) 0       // Measurement mode -- 0 is Timestamp
#define DEFAULT_POLL_CHAR         (char)    0x00        // In poll mode, wait for this before output
#define DEFAULT_CLOCK_HZ          (int64_t) 10000000    // 10 MHz
#define DEFAULT_PICTICK_PS        (int64_t) 100000000   // 100us
#define DEFAULT_CAL_PERIODS       (int16_t) 20          // CAL_PERIODS (2, 10, 20, 40)
#define DEFAULT_TIMEOUT           (int16_t) 0x05        // measurement timeout
#define DEFAULT_WRAP              (int16_t) 0           // timestamp rollover in 100 us ticks; 0 = no wrap
#define DEFAULT_PLACES            (int16_t) 11          // decimal places for output (0-12, default 11)
#define DEFAULT_SYNC_MODE         (char)    'P'         // (P)rimary or (S)econdary
#define DEFAULT_NAME_0            (char)    'A'
#define DEFAULT_NAME_1            (char)    'B'
#define DEFAULT_NAME_3CH          (char)    'C'
#define DEFAULT_PROP_DELAY_0      (int64_t)  0
#define DEFAULT_PROP_DELAY_1      (int64_t)  0
#define DEFAULT_START_EDGE_0      (char)    'R'         // (R)ising or (F)alling
#define DEFAULT_START_EDGE_1      (char)    'R'         // (R)ising or (F)alling
#define DEFAULT_TIME_DILATION_0   (int64_t) 2500        // SWAG that seems to work
#define DEFAULT_TIME_DILATION_1   (int64_t) 2500        // SWAG that seems to work
#define DEFAULT_FIXED_TIME2_0     (int64_t) 0           // 0 to calculate, or fixed (~1135)
#define DEFAULT_FIXED_TIME2_1     (int64_t) 0           // 0 to calculate, or fixed (~1135)
#define DEFAULT_FUDGE0_0          (int64_t) 0           // Fudge channel 0 value (ps)
#define DEFAULT_FUDGE0_1          (int64_t) 0           // Fudge channel 1 value (ps)
#define DEFAULT_BAUD_RATE         (uint32_t) 115200     // Serial baud rate (default 115200)

/*****************************************************************/
// configuration structure type
struct config_t {
  byte       VERSION = EEPROM_VERSION;  // one byte   
  char       SW_VERSION[17];            // up to 16 bytes plus term
  char       BOARD_REV;                 // one byte   
  char       SER_NUM[17];              // up to 16 bytes plus term
  
  // global settings:
  MeasureMode MODE;                     // (T)imestamp, time (I)nterval
                                        // Time(L)ab, (P)eriod, (D)ebug, (B)inary (default 'T')
  char       POLL_CHAR;                 // In poll mode, wiat for this before output
  int64_t    CLOCK_HZ;                  // clock in Hz (default 10 000 000)
  int64_t    PICTICK_PS;                // coarse tick (default 100 000 000)
  int16_t    CAL_PERIODS;               // cal periods 2, 10, 20, 40 (default 20)
  int16_t    TIMEOUT;                   // timeout for measurement in hex (default 0x05)
  int16_t    WRAP;                      // wraparound value for PICcount
  int16_t    PLACES;                    // decimal places for output (0-12, default 11)
  char       SYNC_MODE;                 // one byte:  'P' for primary, 'S' for secondary
  uint32_t   BAUD_RATE;                 // serial baud rate (default 115200)
  
  // per-channel settings, arrays of 2 for channels 0 and 1:
  char       START_EDGE[2];            // (R)ising (default) or (F)alling edge 
  char       NAME[2];                  // user-set channel name
  char       NAME_3CH;                  // 3-Corner Hat synthesized channel name (default 'C')
  int64_t    PROP_DELAY[2];            // user-set offset value (ps)
  int64_t    TIME_DILATION[2];         // time dilation factor (default 2500)
  int64_t    FIXED_TIME2[2];           // if >0 use to replace time2 (default 0)
  int64_t    FUDGE0[2];                // fudge factor (ps) (default 0)
  
};

/*****************************************************************/
// External variables (defined in config_core.cpp)
extern char SER_NUM[17];
extern uint8_t config_changed;
extern config_t config;
extern config_t config_backup;

// New configuration system function prototypes
void show_config_menu();
bool process_config_command(const char* cmd, bool interactive = true);
void init_config_system();

// Command processing functions
bool process_mode_command(char cmd, const char* args, bool interactive);
bool process_baud_command(char cmd, const char* args, bool interactive);
bool process_advanced_command(char cmd, const char* args, bool interactive);
bool process_wrap_command(char cmd, const char* args, bool interactive);
bool process_places_command(char cmd, const char* args, bool interactive);
bool process_edge_command(char cmd, const char* args, bool interactive);
bool process_sync_command(char cmd, const char* args, bool interactive);
bool process_names_command(char cmd, const char* args, bool interactive);
bool process_poll_command(char cmd, const char* args, bool interactive);
bool process_menu_command();
bool process_info_command();
bool process_write_command(bool interactive = true);
bool process_eeprom_clear_command();
bool process_exit_command(char cmd);

// Menu display functions
void show_main_menu();
void show_mode_menu();
void show_baud_menu();
void show_advanced_menu();

// Serial I/O helper functions (declared in config_core.cpp)
void serialPrintImmediate(const char *s);
void configPrint(const char* msg);
void configPrintln(const char* msg);
void configPrintProg(const char* msg);
void configPrintlnProg(const char* msg);
void serialWriteImmediate(char c);
void serialDrain();
size_t readLine(char *buf, size_t cap);
char* trimInPlace(char *s);

// Utility functions
void print_MeasureMode(MeasureMode x);
void printHzAsMHz(int64_t x);
void get_serial_number();
void eeprom_clear();
void eeprom_write_config_default(uint16_t offset);
void eeprom_write_config();
void eeprom_read_config();
struct config_t defaultConfig();
void ticc_setup();

// Configuration management functions
void backup_config();
uint8_t config_change_requires_restart();
void apply_config_changes();
void handle_config_change_exit();

// Parsing helper functions (from config_core.cpp)
bool parseInt64Simple(const char *s, int64_t *out);
bool parseDecimalScaled(const char *s, int64_t scale, int64_t *out);
bool parseInt64Pair(const char *s, bool *set0, int64_t *v0, bool *set1, int64_t *v1);
bool parseDecimalScaledPair(const char *s, int64_t scale, bool *set0, int64_t *v0, bool *set1, int64_t *v1);
bool parseCharPair(const char *s, bool *set0, char *v0, bool *set1, char *v1, bool *set2 = nullptr, char *v2 = nullptr);
char* getInputOrPrompt(const char* args, const char* prompt, char* buffer, size_t bufferSize);

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
void print_mode_header();


#endif	/* CONFIG_H */
