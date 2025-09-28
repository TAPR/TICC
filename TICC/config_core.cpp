// config_core.cpp -- core configuration management functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>
#include <ctype.h>
#include <EEPROM.h>
#include <SPI.h>
#include <string.h>

// misc.h removed - no longer needed
#include "config.h"
#include "board.h"
#include "tdc7200.h"
#include "print.h"

extern const char SW_VERSION[17]; // set in TICC.ino
extern const char SW_TAG[6];      // set in TICC.ino
char SER_NUM[17];          // set by get_ser_num();

// External variables for config change tracking
extern uint8_t config_changed;
extern config_t config;
extern tdc7200Channel channels[];

// Macro to mark config as changed
#define MARK_CONFIG_CHANGED() do { config_changed = 1; } while(0)

// --- Serial helpers ---
static void serialWriteRaw(const char *s, size_t len) {
  while (len) {
    // Wait until there is buffer space to avoid internal buffering delaying output
    while (Serial.availableForWrite() == 0) { /* yield */ }
    size_t n = Serial.write((const uint8_t*)s, len);
    s += n; len -= n;
  }
  Serial.flush();
}

static void serialPrintImmediate(const char *s) {
  Serial.print(s);
  Serial.flush();
}

// Helper function to print config output with "# " prefix for data file compatibility
static void configPrint(const char* msg) {
  serialPrintImmediate("# ");
  serialPrintImmediate(msg);
}

// Macro to replace serialPrintImmediate with configPrint for config output
#define CONFIG_PRINT(msg) configPrint(msg)

static void serialWriteImmediate(char c) {
  Serial.write(c);
  Serial.flush();
}

static void serialDrain() {
  Serial.flush();
  delay(5);
}

// Read a line into buf (cap includes terminator). Returns length (excludes terminator).
// Handles echo, backspace, CR/LF termination. Produces a NUL-terminated string without CR/LF.
static size_t readLine(char *buf, size_t cap) {
  if (cap == 0) return 0;
  size_t n = 0;
  for (;;) {
    while (!Serial.available()) { delay(1); }
    int ch = Serial.read();
    if (ch == '\r' || ch == '\n') {
      // Only echo newline if there was actual input
      if (n > 0) {
        serialWriteImmediate('\r'); serialWriteImmediate('\n');
      }
      buf[n] = '\0';
      return n;
    }
    if (ch == 0x08 || ch == 0x7F) { // backspace/delete
      if (n > 0) { n--; serialWriteImmediate('\b'); serialWriteImmediate(' '); serialWriteImmediate('\b'); }
      continue;
    }
    if (n + 1 < cap) {
      buf[n++] = (char)ch;
      serialWriteImmediate((char)ch);
    }
  }
}

// Trim leading/trailing spaces in place; returns start pointer inside buf.
static char* trimInPlace(char *s) {
  while (*s == ' ' || *s == '\t') s++;
  char *end = s + strlen(s);
  while (end > s && (end[-1] == ' ' || end[-1] == '\t')) { --end; }
  *end = '\0';
  return s;
}

// Simple int64 parser: accepts optional +/-, digits only; returns true on success
bool parseInt64Simple(const char *s, int64_t *out) {
  if (!s || !*s) return false;
  bool neg = false; if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
  if (!*s) return false;
  int64_t v = 0;
  while (*s) {
    if (*s < '0' || *s > '9') return false;
    int d = *s - '0';
    v = v * 10 + d;
    s++;
  }
  *out = neg ? -v : v;
  return true;
}

// Parse decimal like 10.5 into integer scaled by scale (e.g., 1e6). Returns true on success.
bool parseDecimalScaled(const char *s, int64_t scale, int64_t *out) {
  if (!s || !*s) return false;
  bool neg = false; if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
  if (!*s) return false;
  int64_t intPart = 0;
  while (*s && *s != '.') {
    if (*s < '0' || *s > '9') return false;
    intPart = intPart * 10 + (*s - '0');
    s++;
  }
  int64_t fracPart = 0; int64_t fracScale = 1;
  if (*s == '.') {
    s++;
    while (*s && fracScale < scale) {
      if (*s < '0' || *s > '9') return false;
      fracPart = fracPart * 10 + (*s - '0');
      fracScale *= 10;
      s++;
    }
    while (*s) { if (*s < '0' || *s > '9') return false; s++; }
  }
  // scale fractional to match target scale
  while (fracScale < scale) { fracPart *= 10; fracScale *= 10; }
  int64_t total = intPart * scale + fracPart;
  *out = neg ? -total : total;
  return true;
}

// Parse pair syntax "A/B" where either side may be empty.
// Returns which sides are set, and their parsed int64 values.
bool parseInt64Pair(const char *s, bool *set0, int64_t *v0, bool *set1, int64_t *v1) {
  if (!s) return false;
  const char *slash = strchr(s, '/');
  char tmp[64];
  if (!slash) { // single value => apply to both
    size_t len = strlcpy(tmp, s, sizeof(tmp)); (void)len;
    char *t = trimInPlace(tmp);
    int64_t v; if (!parseInt64Simple(t, &v)) return false;
    *set0 = *set1 = true; *v0 = *v1 = v; return true;
  }
  bool ok;
  if (slash != s) {
    size_t l = (size_t)(slash - s); if (l >= sizeof(tmp)) l = sizeof(tmp) - 1;
    memcpy(tmp, s, l); tmp[l] = '\0';
    char *t = trimInPlace(tmp);
    ok = parseInt64Simple(t, v0); if (!ok) return false; *set0 = true;
  } else { *set0 = false; }
  if (*(slash+1)) {
    size_t l = strlcpy(tmp, slash+1, sizeof(tmp)); (void)l;
    char *t = trimInPlace(tmp);
    ok = parseInt64Simple(t, v1); if (!ok) return false; *set1 = true;
  } else { *set1 = false; }
  return true;
}

// Specialized pair parser for decimal scaled values (e.g., us->ps or MHz->Hz)
bool parseDecimalScaledPair(const char *s, int64_t scale, bool *set0, int64_t *v0, bool *set1, int64_t *v1) {
  const char *slash = strchr(s, '/');
  char tmp[64];
  if (!slash) {
    size_t l = strlcpy(tmp, s, sizeof(tmp)); (void)l;
    char *t = trimInPlace(tmp);
    int64_t v; if (!parseDecimalScaled(t, scale, &v)) return false; *set0 = *set1 = true; *v0 = *v1 = v; return true;
  }
  bool ok;
  if (slash != s) {
    size_t l = (size_t)(slash - s); if (l >= sizeof(tmp)) l = sizeof(tmp) - 1;
    memcpy(tmp, s, l); tmp[l] = '\0';
    char *t = trimInPlace(tmp);
    ok = parseDecimalScaled(t, scale, v0); if (!ok) return false; *set0 = true;
  } else { *set0 = false; }
  if (*(slash+1)) {
    size_t l = strlcpy(tmp, slash+1, sizeof(tmp)); (void)l;
    char *t = trimInPlace(tmp);
    ok = parseDecimalScaled(t, scale, v1); if (!ok) return false; *set1 = true;
  } else { *set1 = false; }
  return true;
}

// Get input either from direct parameter or interactive prompt
char* getInputOrPrompt(const char* args, const char* prompt, char* buffer, size_t bufferSize) {
  if (strlen(args) >= 1) {
    return (char*)args;  // Direct parameter provided
  } else {
    configPrint(prompt);
    readLine(buffer, bufferSize);
    return trimInPlace(buffer);
  }
}

void printHzAsMHz(int64_t x)
{
  char str[128];
  int64_t MHz = x / 1000000;
  int64_t Hz = MHz * 1000000;
  int64_t fract = x - Hz;
  sprintf(str, "%ld.", (int32_t)MHz), Serial.print(str);
  sprintf(str,"%06ld", (int32_t)fract), Serial.print(str);
}

char modeToChar(unsigned char mode)
{
		switch (mode)
		{
			case Timestamp: return 'T';
			case Interval:  return 'I';
			case Period:    return 'P';
			case timeLab:   return 'L';
			case Debug:     return 'D';
		}
   return '?';
}

struct config_t defaultConfig() {
  struct config_t x;
  x.VERSION = EEPROM_VERSION;
  strncpy(x.SW_VERSION,SW_VERSION,sizeof(SW_VERSION));
  x.BOARD_REV = BOARD_REVISION;
  strncpy(x.SER_NUM,SER_NUM,sizeof(SER_NUM));
  x.MODE = DEFAULT_MODE;
  x.POLL_CHAR = DEFAULT_POLL_CHAR;
  x.CLOCK_HZ = DEFAULT_CLOCK_HZ;
  x.PICTICK_PS = DEFAULT_PICTICK_PS;
  x.CAL_PERIODS = DEFAULT_CAL_PERIODS;
  x.TIMEOUT = DEFAULT_TIMEOUT;
  x.WRAP = DEFAULT_WRAP;
  x.PLACES = DEFAULT_PLACES;
  x.SYNC_MODE = DEFAULT_SYNC_MODE;
  x.NAME[0] = DEFAULT_NAME_0;
  x.NAME[1] = DEFAULT_NAME_1;
  x.PROP_DELAY[0] = DEFAULT_PROP_DELAY_0;
  x.PROP_DELAY[1] = DEFAULT_PROP_DELAY_1;
  x.START_EDGE[0] = DEFAULT_START_EDGE_0;
  x.START_EDGE[1] = DEFAULT_START_EDGE_1;
  x.TIME_DILATION[0] = DEFAULT_TIME_DILATION_0;
  x.TIME_DILATION[1] = DEFAULT_TIME_DILATION_1;
  x.FIXED_TIME2[0] = DEFAULT_FIXED_TIME2_0;
  x.FIXED_TIME2[1] = DEFAULT_FIXED_TIME2_1;
  x.FUDGE0[0] = DEFAULT_FUDGE0_0;
  x.FUDGE0[1] = DEFAULT_FUDGE0_1;
  return x;
}

// eeprom_write_config_default moved to config_eeprom.cpp

void print_config (config_t x) {
  char tmpbuf[8];
  
  // Software Version
  Serial.print("# Software Version: ");Serial.print(SW_VERSION);
  if (strlen(SW_TAG) > 0) {
    Serial.print(" (");Serial.print(SW_TAG);Serial.print(")");
  }
  Serial.println();
  
  // EEPROM Version and Board Version
  Serial.print("# EEPROM Version: ");Serial.print(EEPROM.read(CONFIG_START)); 
  Serial.print(", Board Version: ");Serial.println(x.BOARD_REV);
  
  // Board Serial Number
  Serial.print("# Board Serial Number: ");Serial.println(x.SER_NUM);
  
  // Measurement Mode (most important param)
  Serial.print("# Measurement Mode: ");print_MeasureMode(MeasureMode(x.MODE));
  
  // Timestamp Wrap
  Serial.print("# Timestamp Wrap: ");
  if (x.WRAP <= 0) {
    Serial.print(x.WRAP);
    Serial.println(" (no wrap)");
  } else if (x.WRAP <= 9) {
    uint32_t wrap_seconds = 1;
    for (int i = 0; i < x.WRAP; i++) wrap_seconds *= 10;
    Serial.print(x.WRAP);
    Serial.print(" (wraps at ");
    Serial.print((unsigned long)wrap_seconds);
    Serial.println(" seconds)");
  } else {
    Serial.print(x.WRAP);
    Serial.print(" (wraps at 1e");
    Serial.print(x.WRAP);
    Serial.println(" seconds)");
  }
  
  // Output Decimal Places
  Serial.print("# Output Decimal Places: ");Serial.println(x.PLACES);
  
  // Trigger Edge
  Serial.print("# Trigger Edge: ");Serial.print(x.START_EDGE[0]);Serial.print(" (ch0), ");  
  Serial.print(x.START_EDGE[1]);Serial.println(" (ch1)");
  
  // SyncMode
  Serial.print("# SyncMode: ");Serial.println(x.SYNC_MODE);
  
  // Channel Names
  Serial.print("# Channel Names: ");Serial.print(x.NAME[0]);Serial.print("/");Serial.println(x.NAME[1]);
  
  // Poll Character (moved to follow Channel Names)
  Serial.print("# Poll Character: ");
  if (x.POLL_CHAR) {
    Serial.println(x.POLL_CHAR);
  } else {
    Serial.println("none");
  }
  
  // Clock Speed
  Serial.print("# Clock Speed: ");printHzAsMHz(x.CLOCK_HZ);Serial.println(" MHz");
  
  // Coarse tick
  Serial.print("# Coarse tick: ");printHzAsMHz(x.PICTICK_PS);Serial.println(" usec");
  
  // Cal Periods
  Serial.print("# Cal Periods: ");Serial.println(x.CAL_PERIODS);
  
  // PropDelay
  Serial.print("# PropDelay: ");Serial.print((int32_t)x.PROP_DELAY[0]);
  Serial.print(" (ch0), ");Serial.print((int32_t)x.PROP_DELAY[1]);Serial.println(" (ch1)");
  
  // Timeout
  Serial.print("# Timeout: ");
  sprintf(tmpbuf,"0x%.2X",x.TIMEOUT);Serial.println(tmpbuf);
  
  // Time Dilation
  Serial.print("# Time Dilation: ");Serial.print((int32_t)x.TIME_DILATION[0]);
  Serial.print(" (ch0), ");Serial.print((int32_t)x.TIME_DILATION[1]);Serial.println(" (ch1)");
  
  // FIXED_TIME2
  Serial.print("# FIXED_TIME2: ");Serial.print((int32_t)x.FIXED_TIME2[0]);
  Serial.print(" (ch0), ");Serial.print((int32_t)x.FIXED_TIME2[1]);Serial.println(" (ch1)");
  
  // FUDGE0
  Serial.print("# FUDGE0: ");Serial.print((int32_t)x.FUDGE0[0]);
  Serial.print(" (ch0), ");Serial.print((int32_t)x.FUDGE0[1]);Serial.println(" (ch1)");
}

// get_serial_number moved to config_eeprom.cpp

// eeprom_clear moved to config_eeprom.cpp

void print_MeasureMode(MeasureMode x) {
  switch (x) {
    case Timestamp:
      Serial.println("Timestamp");
      break;
    case Binary:
      Serial.println("Binary Timestamp");
      break;
    case Period:
      Serial.println("Period");
      break;
    case Interval:
      Serial.println("Time Interval A->B");
      break;
    case timeLab:
      Serial.println("TimeLab 3-Cornered Hat");
      break;
    case Debug:
      Serial.println("Debug");
      break;
    case Null:
      Serial.println("Null Output");
      break;
  }  
}

// Configuration change management functions (moved from TICC.ino)

// Static backup for config change tracking
static config_t config_backup;

// Backup current config before making changes
void backup_config() {
  config_backup = config;
  config_changed = 0;
}

// Check if a config change requires a full restart vs. just a flush
uint8_t config_change_requires_restart() {
  // These parameters require full restart (hardware reinitialization)
  if (config.CLOCK_HZ != config_backup.CLOCK_HZ) return 1;
  if (config.PICTICK_PS != config_backup.PICTICK_PS) return 1;
  if (config.CAL_PERIODS != config_backup.CAL_PERIODS) return 1;
  if (config.START_EDGE[0] != config_backup.START_EDGE[0]) return 1;
  if (config.START_EDGE[1] != config_backup.START_EDGE[1]) return 1;
  if (config.SYNC_MODE != config_backup.SYNC_MODE) return 1;
  
  // These parameters can be changed with just a flush
  // MODE, POLL_CHAR, WRAP, PLACES, NAME, PROP_DELAY, TIME_DILATION, FIXED_TIME2, FUDGE0, TIMEOUT
  return 0;
}

// Apply config changes that don't require restart
void apply_config_changes() {
  extern int64_t CLOCK_HZ, PICTICK_PS, CLOCK_PERIOD, ticksPerSecond;
  extern int16_t CAL_PERIODS, WRAP;
  extern MeasureMode MODE;
  
  // Update global variables from config
  MODE = config.MODE;
  CLOCK_HZ = config.CLOCK_HZ;
  CLOCK_PERIOD = (PS_PER_SEC / CLOCK_HZ);
  PICTICK_PS = config.PICTICK_PS;
  CAL_PERIODS = config.CAL_PERIODS;
  WRAP = config.WRAP;
  ticksPerSecond = PS_PER_SEC / PICTICK_PS;
  
  // Update cached print parameters for maximum performance
  update_cached_config();

  // Update channel-specific settings (2 channels: A and B)
  for (size_t i = 0; i < 2; ++i) {
    channels[i].name = config.NAME[i];
    channels[i].prop_delay = config.PROP_DELAY[i];
    channels[i].time_dilation = config.TIME_DILATION[i];
    channels[i].fixed_time2 = config.FIXED_TIME2[i];
    channels[i].fudge = config.PROP_DELAY[i] + config.FUDGE0[i];
  }
}

// Handle restart vs. resume decision after config changes
void handle_config_change_exit() {
  if (!config_changed) {
    // No changes made, just resume
    return;
  }
  
  if (config_change_requires_restart()) {
    // Full restart required
    Serial.println("# Configuration changes require restart. Restarting...");
    delay(1000);
    return; // This will cause ticc_setup() to be called again
  } else {
    // Can resume with flush
    Serial.println("# Applying configuration changes...");
    apply_config_changes();
    // Note: flush_all_channels() will be called from TICC.ino
    Serial.println("# Resuming operation with new settings.");
    Serial.println("# (Changes are temporary - will revert on restart)");
  }
}
