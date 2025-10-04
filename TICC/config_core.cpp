// config_core.cpp -- Core configuration management functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <Arduino.h>
#include <EEPROM.h>
// config_types.h functionality now included in config.h
#include "config.h"

// External variables referenced by other files
char SER_NUM[17];          // set by get_ser_num();

// External variables defined in TICC.ino
extern uint8_t config_changed;
extern config_t config;
extern config_t config_backup;

// External variables from TICC.ino
extern const char SW_VERSION[17];
extern const char SW_TAG[6];

// External references to existing EEPROM functions (from original config system)
extern void eeprom_write_config();
extern void eeprom_read_config();
extern void eeprom_clear();
extern struct config_t defaultConfig();

// Serial I/O helper functions
void configPrint(const char* msg) {
  Serial.print("# ");
  // Check if this is a PROGMEM string (starts with PROGMEM marker)
  // For now, assume all strings passed to configPrint are regular strings
  Serial.print(msg);
  Serial.flush();
}

void configPrintln(const char* msg) {
  Serial.print("# ");
  Serial.println(msg);
  Serial.flush();
}

// PROGMEM-aware print functions
void configPrintProg(const char* msg) {
  Serial.print("# ");
  char c;
  while ((c = pgm_read_byte(msg++)) != 0) {
    Serial.write(c);
  }
  Serial.flush();
}

void configPrintlnProg(const char* msg) {
  Serial.print("# ");
  char c;
  while ((c = pgm_read_byte(msg++)) != 0) {
    Serial.write(c);
  }
  Serial.println();
  Serial.flush();
}

static void serialWriteImmediate(char c) {
  Serial.write(c);
  Serial.flush();
}

static void serialDrain() {
  Serial.flush();
  delay(5);
}

// Read a line into buf (cap includes terminator). Returns length (excludes terminator).
static size_t readLine(char *buf, size_t cap) {
  if (cap == 0) return 0;
  size_t n = 0;
  for (;;) {
    while (!Serial.available()) { delay(1); }
    int ch = Serial.read();
    if (ch == '\r' || ch == '\n') {
      // Only echo newline if there was actual input
      if (n > 0) {
        serialWriteImmediate('\r'); 
        serialWriteImmediate('\n');
      }
      buf[n] = '\0';
      return n;
    }
    if (ch == 0x08 || ch == 0x7F) { // backspace/delete
      if (n > 0) { 
        n--; 
        serialWriteImmediate('\b'); 
        serialWriteImmediate(' '); 
        serialWriteImmediate('\b'); 
      }
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
  bool neg = false; 
  if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
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
  bool neg = false; 
  if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
  if (!*s) return false;
  int64_t intPart = 0;
  while (*s && *s != '.') {
    if (*s < '0' || *s > '9') return false;
    intPart = intPart * 10 + (*s - '0');
    s++;
  }
  int64_t fracPart = 0; 
  int64_t fracScale = 1;
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
bool parseInt64Pair(const char *s, bool *set0, int64_t *v0, bool *set1, int64_t *v1) {
  if (!s) return false;
  const char *slash = strchr(s, '/');
  char tmp[64];
  if (!slash) { 
    // single value => apply to both
    size_t len = strlcpy(tmp, s, sizeof(tmp)); (void)len;
    char *t = trimInPlace(tmp);
    int64_t v; 
    if (!parseInt64Simple(t, &v)) return false;
    *set0 = *set1 = true; 
    *v0 = *v1 = v; 
    return true;
  }
  bool ok;
  if (slash != s) {
    size_t l = (size_t)(slash - s); 
    if (l >= sizeof(tmp)) l = sizeof(tmp) - 1;
    memcpy(tmp, s, l); 
    tmp[l] = '\0';
    char *t = trimInPlace(tmp);
    ok = parseInt64Simple(t, v0); 
    if (!ok) return false; 
    *set0 = true;
  } else { 
    *set0 = false; 
  }
  if (*(slash+1)) {
    size_t l = strlcpy(tmp, slash+1, sizeof(tmp)); (void)l;
    char *t = trimInPlace(tmp);
    ok = parseInt64Simple(t, v1); 
    if (!ok) return false; 
    *set1 = true;
  } else { 
    *set1 = false; 
  }
  return true;
}

// Parse pair syntax for non-numeric values (like edges "R/F" or names "A/B")
bool parseCharPair(const char *s, bool *set0, char *v0, bool *set1, char *v1) {
  if (!s) return false;
  
  // Handle run-together format (e.g., "RR", "RF")
  if (strlen(s) == 2 && s[0] != '/' && s[1] != ' ') {
    *set0 = *set1 = true;
    *v0 = s[0];
    *v1 = s[1];
    return true;
  }
  
  // Handle separated format (e.g., "R/F", "R F")
  const char *separator = strchr(s, '/');
  if (!separator) {
    // Try space separator
    separator = strchr(s, ' ');
  }
  
  char tmp[16];
  if (!separator) { 
    // single value => apply to both
    if (strlen(s) == 1) {
      *set0 = *set1 = true; 
      *v0 = *v1 = s[0]; 
      return true;
    }
    return false;
  }
  
  if (separator != s) {
    size_t l = (size_t)(separator - s); 
    if (l >= sizeof(tmp)) l = sizeof(tmp) - 1;
    memcpy(tmp, s, l); 
    tmp[l] = '\0';
    char *t = trimInPlace(tmp);
    if (strlen(t) == 1) {
      *v0 = t[0];
      *set0 = true;
    } else {
      *set0 = false;
    }
  } else { 
    *set0 = false; 
  }
  
  if (*(separator+1)) {
    size_t l = strlcpy(tmp, separator+1, sizeof(tmp)); (void)l;
    char *t = trimInPlace(tmp);
    if (strlen(t) == 1) {
      *v1 = t[0];
      *set1 = true;
    } else {
      *set1 = false;
    }
  } else { 
    *set1 = false; 
  }
  return true;
}

// Get input either from direct parameter or interactive prompt
char* getInputOrPrompt(const char* args, const char* prompt, char* buffer, size_t bufferSize) {
  // Check if args is empty or null
  if (args == NULL || args[0] == '\0') {
    configPrintProg(prompt);  // Use PROGMEM-aware print function
    readLine(buffer, bufferSize);
    char* trimmed = trimInPlace(buffer);
    if (!trimmed[0]) {
      return NULL;  // Empty input = escape/cancel
    }
    return trimmed;
  } else {
    return (char*)args;  // Direct parameter provided
  }
}

// Configuration change management functions
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
  return 0;
}

// Apply config changes that don't require restart
void apply_config_changes() {
  // This would integrate with the main program's config application logic
  // For now, just mark as applied
  config_changed = 0;
}

// Handle restart vs. resume decision after config changes
void handle_config_change_exit() {
  // Check if restart was requested from config menu
  extern volatile uint8_t request_restart;
  
  if (request_restart) {
    // Restart was requested from config menu
    if (config_changed) {
      // Option 3 - reset to defaults (config_changed still set)
      configPrintln("Restarting with default settings.");
    } else {
      // W command - changes written to EEPROM (config_changed cleared)
      configPrintln("Restarting with new settings.");
    }
  } else if (config_change_requires_restart()) {
    // Full restart required due to config changes
    configPrintln("Configuration changes require restart. Restarting...");
    delay(1000);
    // This would trigger a restart in the main program
  } else {
    // Can resume with flush
    if (config_changed) {
      configPrintln("Applying configuration changes...");
      apply_config_changes();
      configPrintln("Resuming operation with new settings.");
      configPrintln("(Changes are temporary - will revert on restart)");
    } else {
      // Changes were written to EEPROM, just resume
      configPrintln("Resuming operation with new settings.");
    }
  }
}

// Print current configuration (replaces old print_config function)
void print_config(config_t x) {
  char tmpbuf[64];
  
  // Software Version
  strcpy(tmpbuf, "Software Version: ");
  strcat(tmpbuf, SW_VERSION);
  if (strlen(SW_TAG) > 0) {
    strcat(tmpbuf, " (");
    strcat(tmpbuf, SW_TAG);
    strcat(tmpbuf, ")");
  }
  configPrintln(tmpbuf);
  
  // EEPROM Version and Board Version
  sprintf(tmpbuf, "EEPROM Version: %d, Board Version: %c", EEPROM.read(CONFIG_START), x.BOARD_REV);
  configPrintln(tmpbuf);
  
  // Board Serial Number
  strcpy(tmpbuf, "Board Serial Number: ");
  strcat(tmpbuf, x.SER_NUM);
  configPrintln(tmpbuf);
  
  // Measurement Mode (most important param)
  strcpy(tmpbuf, "Measurement Mode: ");
  switch (x.MODE) {
    case Timestamp: strcat(tmpbuf, "Timestamp"); break;
    case Binary: strcat(tmpbuf, "Binary Timestamp"); break;
    case Period: strcat(tmpbuf, "Period"); break;
    case Interval: strcat(tmpbuf, "Time Interval A->B"); break;
    case Hat: strcat(tmpbuf, "3-Cornered Hat"); break;
    case Debug: strcat(tmpbuf, "Debug"); break;
    case Null: strcat(tmpbuf, "Null Output"); break;
  }
  configPrintln(tmpbuf);
  
  // Timestamp Wrap
  strcpy(tmpbuf, "Timestamp Wrap: ");
  if (x.WRAP <= 0) {
    sprintf(tmpbuf + strlen(tmpbuf), "%d (no wrap)", x.WRAP);
  } else if (x.WRAP <= 9) {
    uint32_t wrap_seconds = 1;
    for (int i = 0; i < x.WRAP; i++) wrap_seconds *= 10;
    sprintf(tmpbuf + strlen(tmpbuf), "%d (wraps at %lu seconds)", x.WRAP, (unsigned long)wrap_seconds);
  } else {
    sprintf(tmpbuf + strlen(tmpbuf), "%d (wraps at 1e%d seconds)", x.WRAP, x.WRAP);
  }
  configPrintln(tmpbuf);
  
  // Output Decimal Places
  sprintf(tmpbuf, "Output Decimal Places: %d", x.PLACES);
  configPrintln(tmpbuf);
  
  // Trigger Edge
  sprintf(tmpbuf, "Trigger Edge: %c (ch0), %c (ch1)", x.START_EDGE[0], x.START_EDGE[1]);
  configPrintln(tmpbuf);
  
  // SyncMode
  sprintf(tmpbuf, "SyncMode: %c", x.SYNC_MODE);
  configPrintln(tmpbuf);
  
  // Serial Baud Rate
  sprintf(tmpbuf, "Serial Baud Rate: %lu", (unsigned long)x.BAUD_RATE);
  configPrintln(tmpbuf);
  
  // Channel Names
  sprintf(tmpbuf, "Channel Names: %c/%c", x.NAME[0], x.NAME[1]);
  configPrintln(tmpbuf);
  
  // Poll Character
  if (x.POLL_CHAR) {
    sprintf(tmpbuf, "Poll Character: %c", x.POLL_CHAR);
  } else {
    strcpy(tmpbuf, "Poll Character: none");
  }
  configPrintln(tmpbuf);
  
  // Clock Speed
  int64_t MHz = x.CLOCK_HZ / 1000000;
  int64_t Hz = MHz * 1000000;
  int64_t fract = x.CLOCK_HZ - Hz;
  sprintf(tmpbuf, "Clock Speed: %ld.%06ld MHz", (int32_t)MHz, (int32_t)fract);
  configPrintln(tmpbuf);
  
  // Coarse tick
  int64_t us = x.PICTICK_PS / 1000000;
  int64_t ps = us * 1000000;
  int64_t ps_fract = x.PICTICK_PS - ps;
  sprintf(tmpbuf, "Coarse tick: %ld.%06ld usec", (int32_t)us, (int32_t)ps_fract);
  configPrintln(tmpbuf);
  
  // Cal Periods
  sprintf(tmpbuf, "Cal Periods: %d", x.CAL_PERIODS);
  configPrintln(tmpbuf);
  
  // PropDelay
  sprintf(tmpbuf, "PropDelay: %ld (ch0), %ld (ch1)", (long)x.PROP_DELAY[0], (long)x.PROP_DELAY[1]);
  configPrintln(tmpbuf);
  
  // Timeout
  sprintf(tmpbuf, "Timeout: 0x%.2X", x.TIMEOUT);
  configPrintln(tmpbuf);
  
  // Time Dilation
  sprintf(tmpbuf, "Time Dilation: %ld (ch0), %ld (ch1)", (long)x.TIME_DILATION[0], (long)x.TIME_DILATION[1]);
  configPrintln(tmpbuf);
  
  // FIXED_TIME2
  sprintf(tmpbuf, "FIXED_TIME2: %ld (ch0), %ld (ch1)", (long)x.FIXED_TIME2[0], (long)x.FIXED_TIME2[1]);
  configPrintln(tmpbuf);
  
  // FUDGE0
  sprintf(tmpbuf, "FUDGE0: %ld (ch0), %ld (ch1)", (long)x.FUDGE0[0], (long)x.FUDGE0[1]);
  configPrintln(tmpbuf);
}

// Create default configuration struct
struct config_t defaultConfig() {
  struct config_t x;
  x.VERSION = EEPROM_VERSION;
  strncpy(x.SW_VERSION, SW_VERSION, sizeof(x.SW_VERSION));
  x.BOARD_REV = BOARD_REVISION;
  strncpy(x.SER_NUM, SER_NUM, sizeof(x.SER_NUM));
  x.MODE = DEFAULT_MODE;
  x.POLL_CHAR = DEFAULT_POLL_CHAR;
  x.CLOCK_HZ = DEFAULT_CLOCK_HZ;
  x.PICTICK_PS = DEFAULT_PICTICK_PS;
  x.CAL_PERIODS = DEFAULT_CAL_PERIODS;
  x.TIMEOUT = DEFAULT_TIMEOUT;
  x.WRAP = DEFAULT_WRAP;
  x.PLACES = DEFAULT_PLACES;
  x.SYNC_MODE = DEFAULT_SYNC_MODE;
  x.BAUD_RATE = DEFAULT_BAUD_RATE;
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

// Initialize configuration system
void init_config_system() {
  // Load config from EEPROM using existing function
  eeprom_read_config();
  config_changed = 0;
}
