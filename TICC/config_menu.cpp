// config_menu.cpp -- Menu display and command processing functions using command table

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>
#include <Arduino.h>
#include <string.h>
#include "config.h"
#include "config_menu_text.h"
#include "config_command_table.h"

// Forward declarations for functions in config_core.cpp
extern void configPrint(const char* msg);
extern void configPrintln(const char* msg);
extern size_t readLine(char *buf, size_t cap);
extern char* trimInPlace(char *s);
extern bool parseInt64Simple(const char *s, int64_t *out);
extern bool parseCharPair(const char *s, bool *set0, char *v0, bool *set1, char *v1);
extern char* getInputOrPrompt(const char* args, const char* prompt, char* buffer, size_t bufferSize);
extern void eeprom_write_config();
extern void eeprom_clear();
extern struct config_t defaultConfig();

// Single shared buffer to reduce memory usage
static char sharedBuffer[128];
static char confirmBuffer[32];


// Helper function to copy PROGMEM strings to buffer
static void copyProgStrToBuffer(const char* str, char* buffer, size_t bufferSize) {
  size_t i = 0;
  char c;
  while ((c = pgm_read_byte(str++)) != 0 && i < bufferSize - 1) {
    buffer[i++] = c;
  }
  buffer[i] = '\0';
}

// Helper function to get mode name as string
static const char* getModeName(MeasureMode mode) {
  switch (mode) {
    case Timestamp: return mode_timestamp;
    case Binary: return mode_binary;
    case Interval: return mode_interval;
    case Period: return mode_period;
    case timeLab: return mode_timelab;
    case Debug: return mode_debug;
    case Null: return mode_null;
    default: return mode_unknown;
  }
}

// Helper function to get wrap description as string
static const char* getWrapDescription(int16_t wrap) {
  static char desc[32];
  if (wrap <= 0) {
    strcpy_P(desc, wrap_no_wrap);
  } else if (wrap <= 9) {
    uint32_t wrap_seconds = 1;
    for (int i = 0; i < wrap; i++) wrap_seconds *= 10;
    sprintf_P(desc, wrap_format, (unsigned long)wrap_seconds);
  } else {
    sprintf_P(desc, wrap_scientific, wrap);
  }
  return desc;
}



// Format frequency as MHz string
static void formatHzAsMHz(int64_t hz, char* buffer, size_t bufferSize) {
  int64_t MHz = hz / 1000000LL;
  int64_t Hz = MHz * 1000000LL;
  int64_t fract = hz - Hz;
  snprintf(buffer, bufferSize, "%ld.%06ld MHz", (int32_t)MHz, (int32_t)fract);
}

// Format picoseconds as microseconds string
static void formatPsAsUs(int64_t ps, char* buffer, size_t bufferSize) {
  int64_t us = ps / 1000000LL;
  int64_t ps_remainder = ps - (us * 1000000LL);
  snprintf(buffer, bufferSize, "%ld.%06ld usec", (int32_t)us, (int32_t)ps_remainder);
}

// Parse scientific notation input (e.g., "1e7", "10e6", "1.5e7", "10000000")
// Returns true if successful, false if invalid
static bool parseScientificNotation(const char* input, int64_t* result) {
  if (!input || strlen(input) == 0) return false;
  
  char* endptr;
  double value = strtod(input, &endptr);
  
  // Check if entire string was consumed
  if (*endptr != '\0') return false;
  
  // Check for reasonable range (avoid overflow)
  if (value < 0 || value > 1e15) return false;
  
  *result = (int64_t)value;
  return true;
}

// Parse pair of values (scientific notation or integers) (e.g., "1e6/2e6", "1000000 2000000", "40/40")
// Returns true if successful, false if invalid
static bool parseScientificNotationPair(const char* input, int64_t* resultA, int64_t* resultB) {
  if (!input || strlen(input) == 0) return false;
  
  // Find the separator (either "/" or space)
  char* separator = strchr(input, '/');
  if (!separator) {
    separator = strchr(input, ' ');
  }
  if (!separator) return false;
  
  // Split the input at the separator
  char inputA[64], inputB[64];
  size_t lenA = separator - input;
  if (lenA >= sizeof(inputA)) return false;
  
  strncpy(inputA, input, lenA);
  inputA[lenA] = '\0';
  
  strcpy(inputB, separator + 1);
  
  // Parse both values using the same function that handles both scientific notation and integers
  return parseScientificNotation(inputA, resultA) && parseScientificNotation(inputB, resultB);
}

// Helper function to handle confirmation flow for pair values
static bool handlePairConfirmation(int64_t oldA, int64_t oldB, int64_t newA, int64_t newB, 
                                  int64_t* configA, int64_t* configB, const char* name) {
  sprintf_P(sharedBuffer, msg_ok_pair, name, (long)oldA, (long)oldB, (long)newA, (long)newB);
  configPrintln(sharedBuffer);
  
  // Show keep/discard options
  configPrintln("");
  configPrintlnProg(ln_1_keep);
  configPrintlnProg(ln_2_discard);
  configPrint("> ");
  
  readLine(sharedBuffer, sizeof(sharedBuffer));
  char *choice = trimInPlace(sharedBuffer);
  
  if (choice[0] == '2' && strlen(choice) == 1) {
    // Discard changes
    *configA = oldA;
    *configB = oldB;
    config_changed = 0;
    configPrintlnProg(ln_discard_changes);
    return false;
  } else {
    // Keep changes (default)
    *configA = newA;
    *configB = newB;
    configPrintlnProg(ln_keep_changes);
    return true;
  }
}


// Handle confirmation flow with keep/discard options
static bool handleConfirmation(const char* confirmationMsg, bool interactive = true) {
  configPrintln(confirmationMsg);
  configPrintlnProg(ln_save_eeprom);
  configPrintln("");
  
  // For non-interactive mode (batch commands), just keep changes
  if (!interactive) {
    configPrintlnProg(ln_keep_changes);
    return true;
  }
  
  configPrintlnProg(ln_1_keep);
  configPrintlnProg(ln_2_discard);
  configPrint("> ");
  
  // Clear Serial input buffer completely
  while (Serial.available() > 0) {
    Serial.read(); // Discard any buffered input
  }
  
  // Get user choice using separate buffer to avoid conflicts
  readLine(confirmBuffer, sizeof(confirmBuffer));
  char *choice = trimInPlace(confirmBuffer);
  
  if (strlen(choice) > 0 && choice[0] == '2') {
    // Discard changes
    configPrintlnProg(ln_discard_changes);
    return false;
  } else {
    // Keep changes (default)
    configPrintlnProg(ln_keep_changes);
    return true;
  }
}

// Show main menu
void show_main_menu() {
  char line[128];
  
  configPrintln("");
  
  // Title
  copyProgStrToBuffer(pg_main_title, line, sizeof(line));
  configPrintln(line);
  
  // A - Mode
  copyProgStrToBuffer(it_mode, line, sizeof(line));
  strcat(line, getModeName(config.MODE));
  strcat(line, ")");
  configPrintln(line);
  
  // B - Wrap
  copyProgStrToBuffer(it_wrap, line, sizeof(line));
  sprintf(line + strlen(line), "%d%s)", config.WRAP, getWrapDescription(config.WRAP));
  configPrintln(line);
  
  // C - Places
  copyProgStrToBuffer(it_places, line, sizeof(line));
  sprintf(line + strlen(line), "%d)", config.PLACES);
  configPrintln(line);
  
  // D - Trigger Edge
  copyProgStrToBuffer(it_edge, line, sizeof(line));
  sprintf(line + strlen(line), "%c/%c)", config.START_EDGE[0], config.START_EDGE[1]);
  configPrintln(line);
  
  // E - Sync Mode
  copyProgStrToBuffer(it_sync, line, sizeof(line));
  sprintf(line + strlen(line), "%c)", config.SYNC_MODE);
  configPrintln(line);
  
  // F - Channel Names
  copyProgStrToBuffer(it_names, line, sizeof(line));
  sprintf(line + strlen(line), "%c/%c)", config.NAME[0], config.NAME[1]);
  configPrintln(line);
  
  // G - Poll Character
  copyProgStrToBuffer(it_pollchar, line, sizeof(line));
  if (config.POLL_CHAR) {
    sprintf(line + strlen(line), "%c)", config.POLL_CHAR);
  } else {
    strcat_P(line, msg_poll_none);
  }
  configPrintln(line);
  
  // H - Advanced
  copyProgStrToBuffer(it_advanced, line, sizeof(line));
  configPrintln(line);
  
  // I - Baud Rate
  copyProgStrToBuffer(it_baud, line, sizeof(line));
  sprintf(line + strlen(line), "%lu)", (unsigned long)config.BAUD_RATE);
  configPrintln(line);
  
  configPrintln("");
  
  copyProgStrToBuffer(it_show_menu, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_printcfg, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_save, line, sizeof(line));
  configPrintln(line);
  
  configPrintln("");
  
  copyProgStrToBuffer(it_exit1, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_exit2, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_exit3, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_exit4, line, sizeof(line));
  configPrintln(line);
}

// Show mode submenu
void show_mode_menu() {
  char line[128];
  
  configPrintln("");
  
  copyProgStrToBuffer(pg_mode_title, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_mode_ts, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_mode_bin, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_mode_int, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_mode_period, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_mode_3ch, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_mode_debug, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_mode_null, line, sizeof(line));
  configPrintln(line);
  
  configPrintln("");
  
  copyProgStrToBuffer(ln_current_mode, line, sizeof(line));
  strcat(line, getModeName(config.MODE));
  configPrintln(line);
  
  configPrintln("");
}

// Show baud rate submenu
void show_baud_menu() {
  char line[128];
  
  configPrintln("");
  
  copyProgStrToBuffer(pg_serial_title, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_baud_9600, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_baud_19200, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_baud_38400, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_baud_57600, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_baud_115200, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_baud_230400, line, sizeof(line));
  configPrintln(line);
  
  configPrintln("");
}

// Show advanced submenu
void show_advanced_menu() {
  char line[128];
  
  configPrintln("");
  
  copyProgStrToBuffer(pg_advanced_title, line, sizeof(line));
  configPrintln(line);
  
  // H1 - Clock Speed
  copyProgStrToBuffer(it_adv_clock, line, sizeof(line));
  int64_t MHz = config.CLOCK_HZ / 1000000;
  int64_t Hz = MHz * 1000000;
  int64_t fract = config.CLOCK_HZ - Hz;
  sprintf(line + strlen(line), "%ld.%06ld MHz)", (int32_t)MHz, (int32_t)fract);
  configPrintln(line);
  
  // H2 - Coarse Tick
  copyProgStrToBuffer(it_adv_pictick, line, sizeof(line));
  int64_t us = config.PICTICK_PS / 1000000;
  int64_t ps = us * 1000000;
  int64_t ps_fract = config.PICTICK_PS - ps;
  sprintf(line + strlen(line), "%ld.%06ld usec)", (int32_t)us, (int32_t)ps_fract);
  configPrintln(line);
  
  // H3 - Propagation Delay
  copyProgStrToBuffer(it_adv_prop, line, sizeof(line));
  sprintf(line + strlen(line), "%ld/%ld)", (long)config.PROP_DELAY[0], (long)config.PROP_DELAY[1]);
  configPrintln(line);
  
  // H4 - Time Dilation
  copyProgStrToBuffer(it_adv_dilation, line, sizeof(line));
  sprintf(line + strlen(line), "%ld/%ld)", (long)config.TIME_DILATION[0], (long)config.TIME_DILATION[1]);
  configPrintln(line);
  
  // H5 - Fixed Time2
  copyProgStrToBuffer(it_adv_fixed, line, sizeof(line));
  sprintf(line + strlen(line), "%ld/%ld)", (long)config.FIXED_TIME2[0], (long)config.FIXED_TIME2[1]);
  configPrintln(line);
  
  // H6 - FUDGE0
  copyProgStrToBuffer(it_adv_fudge, line, sizeof(line));
  sprintf(line + strlen(line), "%ld/%ld)", (long)config.FUDGE0[0], (long)config.FUDGE0[1]);
  configPrintln(line);
  
  configPrintln("");
}

// Process mode submenu commands
bool process_mode_command(char cmd, const char* args, bool interactive) {
  MeasureMode oldMode = config.MODE;
  
  switch (cmd) {
    case '1': config.MODE = Timestamp; break;
    case '2': config.MODE = Binary; break;
    case '3': config.MODE = Interval; break;
    case '4': config.MODE = Period; break;
    case '5': config.MODE = timeLab; break;
    case '6': config.MODE = Debug; break;
    case '7': config.MODE = Null; break;
    default:
      configPrintlnProg(ln_invalid);
      configPrintln(" mode choice");
      return false;
  }
  
  if (oldMode != config.MODE) {
    config_changed = 1;
    strcpy_P(sharedBuffer, msg_ok_mode);
    strcat(sharedBuffer, getModeName(config.MODE));
    configPrintln(sharedBuffer);
    
    // Show keep/discard options
    configPrintln("");
    configPrintlnProg(ln_1_keep);
    configPrintlnProg(ln_2_discard);
    configPrint("> ");
    
    readLine(sharedBuffer, sizeof(sharedBuffer));
    char *choice = trimInPlace(sharedBuffer);
    
    if (choice[0] == '2' && strlen(choice) == 1) {
      // Discard changes
      config.MODE = oldMode;
      config_changed = 0;
      configPrintlnProg(ln_discard_changes);
    } else if (choice[0] == '1' && strlen(choice) == 1) {
      // Keep changes
      configPrintlnProg(ln_keep_changes);
    } else {
      // Invalid choice, keep changes by default
      configPrintlnProg(ln_keep_changes);
    }
  }
  
  return true;
}

// Process baud rate submenu commands
bool process_baud_command(char cmd, const char* args, bool interactive) {
  uint32_t oldRate = config.BAUD_RATE;
  uint32_t newRate;
  
  switch (cmd) {
    case '1': newRate = 9600; break;
    case '2': newRate = 19200; break;
    case '3': newRate = 38400; break;
    case '4': newRate = 57600; break;
    case '5': newRate = 115200; break;
    case '6': newRate = 230400; break;
    default:
      configPrintlnProg(ln_invalid);
      configPrintln(" baud rate choice");
      return false;
  }
  
  if (oldRate != newRate) {
    config.BAUD_RATE = newRate;
    config_changed = 1;
    strcpy_P(sharedBuffer, msg_ok_baud_was);
    sprintf(sharedBuffer + strlen(sharedBuffer), "%ld", (long)oldRate);
    strcat_P(sharedBuffer, msg_ok_baud_now);
    sprintf(sharedBuffer + strlen(sharedBuffer), "%ld", (long)newRate);
    configPrintln(sharedBuffer);
    
    // Show keep/discard options
    configPrintln("");
    configPrintlnProg(ln_1_keep);
    configPrintlnProg(ln_2_discard);
    configPrint("> ");
    
    readLine(sharedBuffer, sizeof(sharedBuffer));
    char *choice = trimInPlace(sharedBuffer);
    
    if (choice[0] == '2' && strlen(choice) == 1) {
      // Discard changes
      config.BAUD_RATE = oldRate;
      config_changed = 0;
      configPrintlnProg(ln_discard_changes);
    } else if (choice[0] == '1' && strlen(choice) == 1) {
      // Keep changes
      configPrintlnProg(ln_keep_changes);
    } else {
      // Invalid choice, keep changes by default
      configPrintlnProg(ln_keep_changes);
    }
  } else {
    // Baud rate is already set to this value
    strcpy_P(sharedBuffer, msg_ok_baud_already);
    sprintf(sharedBuffer + strlen(sharedBuffer), "%ld", (long)newRate);
    configPrintln(sharedBuffer);
  }
  
  return true;
}

// Process advanced submenu commands
bool process_advanced_command(char cmd, const char* args, bool interactive) {
  switch (cmd) {
    case '1': {
      // H1 - Clock Speed (Hz, accepts scientific notation like 1e7 for 10 MHz)
      char* input = getInputOrPrompt(args, prompt_clock, sharedBuffer, sizeof(sharedBuffer));
      if (!input) return false;
      
      int64_t hz;
      if (parseScientificNotation(input, &hz) && hz >= 1000000LL && hz <= 16000000LL) {
        int64_t old = config.CLOCK_HZ;
        config.CLOCK_HZ = hz;
        config_changed = 1;
        
        // Build confirmation message using string formatting
        char oldStr[32], newStr[32];
        formatHzAsMHz(old, oldStr, sizeof(oldStr));
        formatHzAsMHz(hz, newStr, sizeof(newStr));
        sprintf_P(sharedBuffer, msg_ok_clock, oldStr, newStr);
        configPrintln(sharedBuffer);
        
        // Show keep/discard options
        configPrintln("");
        configPrintlnProg(ln_1_keep);
        configPrintlnProg(ln_2_discard);
        configPrint("> ");
        
        readLine(sharedBuffer, sizeof(sharedBuffer));
        char *choice = trimInPlace(sharedBuffer);
        
        if (choice[0] == '2' && strlen(choice) == 1) {
          // Discard changes
          config.CLOCK_HZ = old;
          config_changed = 0;
          configPrintlnProg(ln_discard_changes);
        } else if (choice[0] == '1' && strlen(choice) == 1) {
          // Keep changes
          configPrintlnProg(ln_keep_changes);
        } else {
          // Invalid choice, keep changes by default
          configPrintlnProg(ln_keep_changes);
        }
      } else {
        configPrintlnProg(ln_invalid);
        configPrintln(" (expected Hz, e.g., 1e7 for 10 MHz)");
      }
      break;
    }
    case '2': {
      // H2 - Coarse Tick (picoseconds, accepts scientific notation like 1e8 for 100 us)
      char* input = getInputOrPrompt(args, prompt_pictick, sharedBuffer, sizeof(sharedBuffer));
      if (!input) return false;
      
      int64_t ps;
      if (parseScientificNotation(input, &ps) && ps >= 100000000LL && ps <= 100000000000LL) {
        int64_t old = config.PICTICK_PS;
        config.PICTICK_PS = ps;
        config_changed = 1;
        
        // Build confirmation message using string formatting
        char oldStr[32], newStr[32];
        formatPsAsUs(old, oldStr, sizeof(oldStr));
        formatPsAsUs(ps, newStr, sizeof(newStr));
        sprintf(sharedBuffer, "OK -- Coarse Tick %s -> %s", oldStr, newStr);
        configPrintln(sharedBuffer);
        
        // Show keep/discard options
        configPrintln("");
        configPrintlnProg(ln_1_keep);
        configPrintlnProg(ln_2_discard);
        configPrint("> ");
        
        readLine(sharedBuffer, sizeof(sharedBuffer));
        char *choice = trimInPlace(sharedBuffer);
        
        if (choice[0] == '2' && strlen(choice) == 1) {
          // Discard changes
          config.PICTICK_PS = old;
          config_changed = 0;
          configPrintlnProg(ln_discard_changes);
        } else if (choice[0] == '1' && strlen(choice) == 1) {
          // Keep changes
          configPrintlnProg(ln_keep_changes);
        } else {
          // Invalid choice, keep changes by default
          configPrintlnProg(ln_keep_changes);
        }
      } else {
        configPrintlnProg(ln_invalid);
        configPrintln(" (expected picoseconds, e.g., 1e8 for 100 us)");
      }
      break;
    }
    case '3': {
      // H3 - Propagation Delay A/B (picoseconds, accepts scientific notation like 1e6,2e6)
      char* input = getInputOrPrompt(args, prompt_prop, sharedBuffer, sizeof(sharedBuffer));
      if (!input) return false;
      
      int64_t psA, psB;
      if (parseScientificNotationPair(input, &psA, &psB) && psA >= 0 && psA <= 1000000000000LL && psB >= 0 && psB <= 1000000000000LL) {
        int64_t oldA = config.PROP_DELAY[0], oldB = config.PROP_DELAY[1];
        config_changed = 1;
        handlePairConfirmation(oldA, oldB, psA, psB, &config.PROP_DELAY[0], &config.PROP_DELAY[1], "PropDelay");
      } else {
        configPrintlnProg(ln_invalid);
        configPrintln(" (expected ps values like 40/40 or 1e6/2e6)");
      }
      break;
    }
    case '4': {
      // H4 - Time Dilation A/B (picoseconds, accepts scientific notation like 1e6,2e6)
      char* input = getInputOrPrompt(args, prompt_dilation, sharedBuffer, sizeof(sharedBuffer));
      if (!input) return false;
      
      int64_t psA, psB;
      if (parseScientificNotationPair(input, &psA, &psB) && psA >= 0 && psA <= 1000000000000LL && psB >= 0 && psB <= 1000000000000LL) {
        int64_t oldA = config.TIME_DILATION[0], oldB = config.TIME_DILATION[1];
        config_changed = 1;
        handlePairConfirmation(oldA, oldB, psA, psB, &config.TIME_DILATION[0], &config.TIME_DILATION[1], "Time Dilation");
      } else {
        configPrintlnProg(ln_invalid);
        configPrintln(" (expected ps values like 40/40 or 1e6/2e6)");
      }
      break;
    }
    case '5': {
      // H5 - Fixed Time2 A/B (picoseconds, accepts scientific notation like 1e6,2e6)
      char* input = getInputOrPrompt(args, prompt_fixed, sharedBuffer, sizeof(sharedBuffer));
      if (!input) return false;
      
      int64_t psA, psB;
      if (parseScientificNotationPair(input, &psA, &psB) && psA >= 0 && psA <= 1000000000000LL && psB >= 0 && psB <= 1000000000000LL) {
        int64_t oldA = config.FIXED_TIME2[0], oldB = config.FIXED_TIME2[1];
        config_changed = 1;
        handlePairConfirmation(oldA, oldB, psA, psB, &config.FIXED_TIME2[0], &config.FIXED_TIME2[1], "fixedTime2");
      } else {
        configPrintlnProg(ln_invalid);
        configPrintln(" (expected ps values like 40/40 or 1e6/2e6)");
      }
      break;
    }
    case '6': {
      // H6 - FUDGE0 A/B (picoseconds, accepts scientific notation like 1e6,2e6)
      char* input = getInputOrPrompt(args, prompt_fudge, sharedBuffer, sizeof(sharedBuffer));
      if (!input) return false;
      
      int64_t psA, psB;
      if (parseScientificNotationPair(input, &psA, &psB) && psA >= 0 && psA <= 1000000000000LL && psB >= 0 && psB <= 1000000000000LL) {
        int64_t oldA = config.FUDGE0[0], oldB = config.FUDGE0[1];
        config_changed = 1;
        handlePairConfirmation(oldA, oldB, psA, psB, &config.FUDGE0[0], &config.FUDGE0[1], "FUDGE0");
      } else {
        configPrintlnProg(ln_invalid);
        configPrintln(" (expected ps values like 40/40 or 1e6/2e6)");
      }
      break;
    }
    default:
      configPrintlnProg(ln_invalid);
      configPrintln(" Advanced Choice");
      return false;
  }
  
  return true;
}

// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================

// Process wrap command
bool process_wrap_command(char cmd, const char* args, bool interactive) {
  char* input = getInputOrPrompt(args, prompt_wrap, sharedBuffer, sizeof(sharedBuffer));
  if (!input) return true;
  
  int64_t wrap;
  if (parseInt64Simple(input, &wrap) && wrap >= 0 && wrap <= 10) {
    int16_t old = config.WRAP;
    config.WRAP = (int16_t)wrap;
    config_changed = 1;
    sprintf(sharedBuffer, "OK -- Wrap Digits %d -> %d", old, config.WRAP);
    if (!handleConfirmation(sharedBuffer, interactive)) {
      config.WRAP = old;
      config_changed = 0;
    }
  } else {
    configPrintlnProg(ln_invalid);
    configPrintln("");
  }
  return true;
}

// Process places command
bool process_places_command(char cmd, const char* args, bool interactive) {
  char* input = getInputOrPrompt(args, prompt_places, sharedBuffer, sizeof(sharedBuffer));
  if (!input) return true;
  
  int64_t places;
  if (parseInt64Simple(input, &places) && places >= 0 && places <= 12) {
    int16_t old = config.PLACES;
    config.PLACES = (int16_t)places;
    config_changed = 1;
    sprintf(sharedBuffer, "OK -- Decimal Places %d -> %d", old, config.PLACES);
    if (!handleConfirmation(sharedBuffer, interactive)) {
      config.PLACES = old;
      config_changed = 0;
    }
  } else {
    configPrintlnProg(ln_invalid);
    configPrintln("");
  }
  return true;
}

// Process edge command
bool process_edge_command(char cmd, const char* args, bool interactive) {
  char* input = getInputOrPrompt(args, prompt_edge, sharedBuffer, sizeof(sharedBuffer));
  if (!input) return true;
  
  bool set0, set1;
  char v0, v1;
  if (parseCharPair(input, &set0, &v0, &set1, &v1)) {
    v0 = toupper(v0);
    v1 = toupper(v1);
    if ((v0 == 'R' || v0 == 'F') && (v1 == 'R' || v1 == 'F')) {
      char old0 = config.START_EDGE[0], old1 = config.START_EDGE[1];
      if (set0) config.START_EDGE[0] = v0;
      if (set1) config.START_EDGE[1] = v1;
      config_changed = 1;
      sprintf(sharedBuffer, "OK -- Edges %c/%c -> %c/%c", old0, old1, config.START_EDGE[0], config.START_EDGE[1]);
      if (!handleConfirmation(sharedBuffer, interactive)) {
        config.START_EDGE[0] = old0;
        config.START_EDGE[1] = old1;
        config_changed = 0;
      }
    } else {
      configPrintlnProg(ln_invalid);
      configPrintln("");
    }
  } else {
    configPrintlnProg(ln_invalid);
    configPrintln("");
  }
  return true;
}

// Process sync command
bool process_sync_command(char cmd, const char* args, bool interactive) {
  char* input = getInputOrPrompt(args, prompt_sync, sharedBuffer, sizeof(sharedBuffer));
  if (!input) return true;
  
  char c = toupper(input[0]);
  if (c == 'P' || c == 'S') {
    char old = config.SYNC_MODE;
    config.SYNC_MODE = c;
    config_changed = 1;
    sprintf(sharedBuffer, "OK -- Sync Mode %c -> %c", old, c);
    if (!handleConfirmation(sharedBuffer, interactive)) {
      config.SYNC_MODE = old;
      config_changed = 0;
    }
  } else {
    configPrintlnProg(ln_invalid);
    configPrintln("");
  }
  return true;
}

// Process names command
bool process_names_command(char cmd, const char* args, bool interactive) {
  char* input = getInputOrPrompt(args, prompt_names, sharedBuffer, sizeof(sharedBuffer));
  if (!input) return true;
  
  bool set0, set1;
  char v0, v1;
  if (parseCharPair(input, &set0, &v0, &set1, &v1)) {
    char old0 = config.NAME[0], old1 = config.NAME[1];
    if (set0) config.NAME[0] = v0;
    if (set1) config.NAME[1] = v1;
    config_changed = 1;
    sprintf(sharedBuffer, "OK -- Names %c/%c -> %c/%c", old0, old1, config.NAME[0], config.NAME[1]);
    if (!handleConfirmation(sharedBuffer, interactive)) {
      config.NAME[0] = old0;
      config.NAME[1] = old1;
      config_changed = 0;
    }
  } else {
    configPrintlnProg(ln_invalid);
    configPrintln("");
  }
  return true;
}

// Process poll command
bool process_poll_command(char cmd, const char* args, bool interactive) {
  char* input = getInputOrPrompt(args, prompt_poll, sharedBuffer, sizeof(sharedBuffer));
  if (!input) return true;
  
  char old = config.POLL_CHAR;
  config.POLL_CHAR = (input[0] == '\0' || input[0] == ' ') ? 0x00 : input[0];
  config_changed = 1;
  strcpy(sharedBuffer, "OK -- Poll Character ");
  if (old) {
    sharedBuffer[strlen(sharedBuffer)] = old;
    sharedBuffer[strlen(sharedBuffer)+1] = '\0';
  } else {
    strcat(sharedBuffer, "none");
  }
  strcat(sharedBuffer, " -> ");
  if (config.POLL_CHAR) {
    sharedBuffer[strlen(sharedBuffer)] = config.POLL_CHAR;
    sharedBuffer[strlen(sharedBuffer)+1] = '\0';
  } else {
    strcat(sharedBuffer, "none");
  }
  if (!handleConfirmation(sharedBuffer, interactive)) {
    config.POLL_CHAR = old;
    config_changed = 0;
  }
  return true;
}

// Process menu command
bool process_menu_command() {
  show_main_menu();
  return true;
}

// Process info command
bool process_info_command() {
  configPrintln("");
  // print_config(config); // Call existing function
  configPrintln("");
  return true;
}

// Process write command
bool process_write_command() {
  eeprom_write_config();
  config_changed = 0;
  configPrintln("Changes written to EEPROM (will persist across restarts)");
  return true;
}

// Process EEPROM clear command
bool process_eeprom_clear_command() {
  configPrintlnProg(ln_eeprom_warning);
  configPrintln("");
  configPrintlnProg(ln_eeprom_confirm);
  readLine(sharedBuffer, sizeof(sharedBuffer));
  char *input = trimInPlace(sharedBuffer);
  if (strcmp(input, "YES") == 0) {
    configPrintlnProg(ln_eeprom_clearing);
    eeprom_clear();
    configPrintlnProg(ln_eeprom_cleared);
    return false; // Exit config system
  } else {
    configPrintlnProg(ln_eeprom_cancelled);
  }
  return true;
}

// Process exit command
bool process_exit_command(char cmd) {
  switch (cmd) {
    case '1':
      configPrintln("Discarded changes.");
      config_changed = 0;
      return false; // Exit config system
    case '2':
      configPrintln("Applying changes and restarting...");
      return false; // Exit config system with restart
    case '3':
      configPrintln("Applying changes and resuming operation...");
      config_changed = 0;
      return false; // Exit config system with resume
    case '4':
      config = defaultConfig();
      eeprom_write_config();
      configPrintln("Defaults written. Restarting...");
      return false; // Exit config system with restart
    default:
      return true;
  }
}

// ============================================================================
// MAIN COMMAND PROCESSOR USING COMMAND TABLE
// ============================================================================

// Main command processor using the command table
bool process_config_command(const char* cmdLine, bool interactive) {
  char *line = trimInPlace((char*)cmdLine);
  if (strlen(line) == 0) return true; // Empty command, continue
  
  char cmd = toupper(line[0]);
  static char args_buffer[64];  // Static buffer for arguments
  char *args;
  
  
  if (strlen(line) <= 1) {
    // No arguments - create empty string
    args_buffer[0] = '\0';
    args = args_buffer;
  } else {
    char *src = line + 1;
    while (*src == ' ') src++; // Skip leading spaces

    // Copy arguments to buffer and null-terminate
    int i = 0;
    while (*src && *src != ' ' && i < 63) {
      args_buffer[i++] = *src++;
    }
    args_buffer[i] = '\0';
    args = args_buffer;
  }
  
  // Handle direct submenu commands (A1-A7, I1-I6, H1-H6)
  if (cmd == 'A' && strlen(line) >= 2 && isdigit(line[1])) {
    return process_mode_command(line[1], args, interactive);
  }
  
  if (cmd == 'I' && strlen(line) >= 2 && isdigit(line[1])) {
    return process_baud_command(line[1], args, interactive);
  }
  
  if (cmd == 'H' && strlen(line) >= 2 && isdigit(line[1])) {
    return process_advanced_command(line[1], args, interactive);
  }
  
  // Look up command in table
  const CommandEntry* entry = find_command(cmd);
  if (!entry) {
    char line[64];
    copyProgStrToBuffer(ln_unknown, line, sizeof(line));
    configPrintln(line);
    return true;
  }
  
  // Read fields directly from PROGMEM
  CommandType type = (CommandType)pgm_read_byte(&entry->type);
  bool requires_submenu = pgm_read_byte(&entry->requires_submenu);
  
  // Execute command based on type
  if (type == CMD_MAIN_MENU) {
    bool (*handler_func)(char, const char*, bool) = (bool (*)(char, const char*, bool))pgm_read_word(&entry->handler_func);
    return handler_func(cmd, args, interactive);
  }
  else if (type == CMD_SUBMENU) {
    // Read function pointer from PROGMEM
    void (*submenu_func)() = (void (*)())pgm_read_word(&entry->submenu_func);
    if (submenu_func) {
      submenu_func();
      configPrint("> ");
      readLine(sharedBuffer, sizeof(sharedBuffer));
      char *input = trimInPlace(sharedBuffer);
      if (strlen(input) > 0) {
        // Handle submenu selection (1-7 for mode, 1-6 for baud/advanced)
        if (isdigit(input[0])) {
          bool (*handler_func)(char, const char*, bool) = (bool (*)(char, const char*, bool))pgm_read_word(&entry->handler_func);
          return handler_func(input[0], input + 1, interactive);
        }
        // Handle direct submenu commands (A1-A7, H1-H6, I1-I6)
        else if (strlen(input) >= 2 && toupper(input[0]) == cmd && isdigit(input[1])) {
          bool (*handler_func)(char, const char*, bool) = (bool (*)(char, const char*, bool))pgm_read_word(&entry->handler_func);
          return handler_func(input[1], input + 2, interactive);
        }
      }
    }
    return true;
  }
  else if (type == CMD_DIRECT) {
    bool (*handler_func)(char, const char*, bool) = (bool (*)(char, const char*, bool))pgm_read_word(&entry->handler_func);
    return handler_func(cmd, args, interactive);
  }
  else if (type == CMD_EXIT) {
    bool (*handler_func)(char, const char*, bool) = (bool (*)(char, const char*, bool))pgm_read_word(&entry->handler_func);
    return handler_func(cmd, args, interactive);
  }
  else {
    char line[64];
    copyProgStrToBuffer(ln_unknown, line, sizeof(line));
    configPrintln(line);
    return true;
  }
}

// Main configuration menu system entry point
void show_config_menu() {
  bool showMenu = true;
  bool interactive = true;
  
  // Check if we're in batch mode (semicolons in input)
  // This would be determined by the caller
  
  while (true) {
    if (showMenu) {
      show_main_menu();
      showMenu = false;
    }
    
    configPrint("> ");
    readLine(sharedBuffer, sizeof(sharedBuffer));
    char *line = trimInPlace(sharedBuffer);
    
    if (strlen(line) == 0) {
      continue; // Empty input, show menu again
    }
    
    // Check for batch mode (semicolons)
    bool has_semicolons = (strchr(line, ';') != NULL);
    interactive = !has_semicolons;
    
    // Process semicolon-separated commands
    char *cmd_start = line;
    char *cmd_end;
    bool should_exit = false;
    
    while (cmd_start && *cmd_start && !should_exit) {
      // Find the next semicolon or end of string
      cmd_end = strchr(cmd_start, ';');
      bool had_semicolon = (cmd_end != NULL);
      if (cmd_end) {
        *cmd_end = '\0';  // Temporarily null-terminate
      } else {
        cmd_end = cmd_start + strlen(cmd_start);  // Point to end of string
      }
      
      // Process this command
      char *cmd_line = trimInPlace(cmd_start);
      if (strlen(cmd_line) > 0) {
        if (!process_config_command(cmd_line, interactive)) {
          should_exit = true;
          break;
        }
      }
      
      // Restore semicolon if we had one
      if (had_semicolon) {
        *cmd_end = ';';
      }
      
      // Move to next command
      if (had_semicolon) {
        cmd_start = cmd_end + 1;
        while (*cmd_start == ' ') cmd_start++;  // Skip leading spaces
        if (*cmd_start == '\0') break;  // No more commands
      } else {
        break;  // No more commands
      }
    }
    
    if (should_exit) break;
    
    // Show menu again after processing commands
    showMenu = true;
  }
}
