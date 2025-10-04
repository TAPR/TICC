// config_menu.cpp -- Menu display and command processing functions using command table

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

/*
 * HOW TO ADD A NEW MENU ITEM (Table-Driven System)
 * ================================================
 * 
 * The configuration system now uses a table-driven approach for maximum efficiency
 * and maintainability. To add a new menu item, follow these simplified steps:
 * 
 * 1. ADD MENU TEXT (config_menu_text.h):
 *    - Add PROGMEM string constants for menu item text, prompts, and messages
 *    - Use descriptive names like: it_newitem, prompt_newitem, msg_newitem_ok
 *    - Example: const char it_newitem[] PROGMEM = "X - New Item (currently: value)";
 * 
 * 2. ADD CONFIGURATION PARAMETER (if needed):
 *    - Add field to config_t struct in config.h
 *    - Add default value constant
 *    - Update defaultConfig() function in config_core.cpp
 *    - Update print_config() function to display the parameter
 * 
 * 3. ADD COMMAND TABLE ENTRY (config_command_table.cpp):
 *    - Add entry to appropriate command table (main_menu_commands, advanced_commands, etc.)
 *    - Define ValueType, validation range, config pointer, and confirmation type
 *    - Example for simple integer:
 *      {'X', SIMPLE_INT, prompt_newitem, 0, 100, "", &config.NEW_PARAM, 0, "New Parameter", CONFIRM_HANDLE, 0}
 * 
 * 4. UPDATE MENU DISPLAY (config_menu.cpp):
 *    - Add menu item display to appropriate show_*_menu() function
 *    - Use copyProgStrToBuffer() and strcat_P() for PROGMEM strings
 *    - Show current value using appropriate formatting
 * 
 * 5. ADD EEPROM SUPPORT (if needed):
 *    - Add EEPROM read/write functions in config_eeprom.cpp
 *    - Update loadConfig() and saveConfig() functions
 *    - Add to configChanged() function
 * 
 * 6. TEST THE IMPLEMENTATION:
 *    - Compile and test the new menu item
 *    - Verify input validation works correctly
 *    - Test both interactive and batch command modes
 *    - Ensure EEPROM persistence works
 * 
 * COMMAND TABLE ENTRY FORMAT:
 * ===========================
 * 
 * CommandConfig entry: {cmd_letter, value_type, prompt_prog, min_val, max_val, 
 *                      allowed_chars, config_ptr, config_offset, description_prog, 
 *                      confirm_type, enum_value}
 * 
 * ValueType Options:
 * - SIMPLE_INT: Integer input with range validation
 * - SIMPLE_CHAR: Single character input  
 * - SCIENTIFIC: Scientific notation input (e.g., "1e7", "10e6")
 * - SCIENTIFIC_PAIR: Pair of scientific notation values (e.g., "1e6/2e6")
 * - CHAR_PAIR: Pair of characters (e.g., "AB")
 * - SUBMENU_SELECTION: Submenu selection (no input required)
 * 
 * ConfirmationType Options:
 * - CONFIRM_HANDLE: Use handleConfirmation() for single values
 * - CONFIRM_PAIR: Use handlePairConfirmation() for pair values
 * - CONFIRM_MANUAL: Use manual confirmation flow (for submenus)
 * 
 * EXAMPLES:
 * =========
 * 
 * Simple integer parameter (like Wrap Digits):
 * {'B', SIMPLE_INT, prompt_wrap, 0, 10, "", &config.WRAP, 0, "Wrap Digits", CONFIRM_HANDLE, 0}
 * 
 * Character parameter (like Sync Mode):
 * {'E', SIMPLE_CHAR, prompt_sync, 0, 0, "PS", &config.SYNC_MODE, 0, "Sync Mode", CONFIRM_HANDLE, 0}
 * 
 * Scientific notation (like Clock Speed):
 * {'1', SCIENTIFIC, prompt_clock, 1000000, 16000000, "", &config.CLOCK_HZ, 0, "Clock Speed", CONFIRM_MANUAL, 0}
 * 
 * Pair values (like Propagation Delay):
 * {'3', SCIENTIFIC_PAIR, prompt_prop, 0, 1000000000000, "", config.PROP_DELAY, 0, "PropDelay", CONFIRM_PAIR, 0}
 * 
 * Submenu selection (like Mode):
 * {'1', SUBMENU_SELECTION, NULL, 0, 0, "", &config.MODE, 0, "Mode", CONFIRM_MANUAL, Timestamp}
 * 
 * BENEFITS OF TABLE-DRIVEN SYSTEM:
 * ================================
 * - 31.6% reduction in code size (1,225 → 838 lines)
 * - Centralized command processing logic
 * - Consistent validation and confirmation patterns
 * - Easy to add new commands (just table entries)
 * - Reduced maintenance overhead
 * - All command tables in config_command_table.cpp/.h
 * 
 * HELPER FUNCTIONS AVAILABLE:
 * - getInputOrPrompt(): Get user input with prompt
 * - handlePairConfirmation(): Confirmation flow for pair values
 * - handleConfirmation(): General confirmation flow
 * - parseScientificNotation(): Parse scientific notation input
 * - parseScientificNotationPair(): Parse comma/space-separated pairs
 * - configPrintlnProg(): Print PROGMEM strings with "# " prefix
 * 
 * MEMORY CONSIDERATIONS:
 * - Use PROGMEM for all user-facing strings to save RAM
 * - Use strcat_P() and sprintf_P() for PROGMEM string operations
 * - Keep sharedBuffer[128] for temporary string operations
 * - Be mindful of Arduino's limited RAM (8KB total)
 */

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

// External variables from TICC.ino
extern volatile uint8_t request_restart;
extern uint8_t config_changed;
extern config_t config;

// Forward declarations for version info
extern const char SW_VERSION[17];
extern const char SW_TAG[6];
extern char SER_NUM[17];

// Single shared buffer to reduce memory usage
char sharedBuffer[128];
static char confirmBuffer[32];

// Cursor-based safe appender helpers to avoid O(n^2) string operations
// and provide automatic bounds checking
char* app_init(char* buf, size_t cap) {
  if (cap) buf[0] = '\0';
  return buf;
}

size_t app_len(const char* buf) { 
  return strlen(buf); 
}

bool app_p(char* &cur, size_t &rem, const char* prog) {
  // copy PROGMEM string into buffer at 'cur'
  while (rem > 1) {
    char c = pgm_read_byte(prog++);
    if (!c) break;
    *cur++ = c; *cur = '\0'; --rem;
  }
  return rem > 1;
}

bool app_s(char* &cur, size_t &rem, const char* s) {
  while (*s && rem > 1) { *cur++ = *s++; *cur = '\0'; --rem; }
  return rem > 1;
}

bool app_c(char* &cur, size_t &rem, char c) {
  if (rem <= 1) return false;
  *cur++ = c; *cur = '\0'; --rem; 
  return true;
}

// Fast integer appenders to avoid pulling in full printf machinery
bool app_u32(char* &cur, size_t &rem, uint32_t v) {
  char tmp[11]; // max 4294967295
  char* p = &tmp[10]; *p = '\0';
  do { *--p = '0' + (v % 10); v /= 10; } while (v && p > tmp);
  return app_s(cur, rem, p);
}

bool app_i32(char* &cur, size_t &rem, int32_t v) {
  if (v < 0) { if (!app_c(cur, rem, '-')) return false; v = -v; }
  return app_u32(cur, rem, (uint32_t)v);
}

// Powers of ten lookup table to avoid repeated multiplication
static const uint32_t POW10[] PROGMEM = {
  1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
};


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
const char* getModeName(MeasureMode mode) {
  switch (mode) {
    case Timestamp: return mode_timestamp;
    case Binary: return mode_binary;
    case Interval: return mode_interval;
    case Period: return mode_period;
    case Hat: return mode_timelab;
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
    uint32_t wrap_seconds = pgm_read_dword(&POW10[wrap]);
    sprintf_P(desc, wrap_format, (unsigned long)wrap_seconds);
  } else {
    sprintf_P(desc, wrap_scientific, wrap);
  }
  return desc;
}



// Format frequency as MHz string
void formatHzAsMHz(int64_t hz, char* buffer, size_t bufferSize) {
  int64_t MHz = hz / 1000000LL;
  int64_t Hz = MHz * 1000000LL;
  int64_t fract = hz - Hz;
  snprintf(buffer, bufferSize, "%ld.%06ld MHz", (int32_t)MHz, (int32_t)fract);
}

// Format picoseconds as microseconds string
void formatPsAsUs(int64_t ps, char* buffer, size_t bufferSize) {
  int64_t us = ps / 1000000LL;
  int64_t ps_remainder = ps - (us * 1000000LL);
  snprintf(buffer, bufferSize, "%ld.%06ld usec", (int32_t)us, (int32_t)ps_remainder);
}

// Parse scientific notation input (e.g., "1e7", "10e6", "1.5e7", "10000000")
// Returns true if successful, false if invalid
bool parseScientificNotation(const char* input, int64_t* result) {
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
bool parseScientificNotationPair(const char* input, int64_t* resultA, int64_t* resultB) {
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
bool handlePairConfirmation(int64_t oldA, int64_t oldB, int64_t newA, int64_t newB, 
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
bool handleConfirmation(const char* confirmationMsg, bool interactive = true) {
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
  strcat_P(line, getModeName(config.MODE));
  strcat(line, ")");
  configPrintln(line);
  
  // B - Wrap
  copyProgStrToBuffer(it_wrap, line, sizeof(line));
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%d%s)", config.WRAP, getWrapDescription(config.WRAP));
  configPrintln(line);
  
  // C - Places
  copyProgStrToBuffer(it_places, line, sizeof(line));
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%d)", config.PLACES);
  configPrintln(line);
  
  // D - Trigger Edge
  copyProgStrToBuffer(it_edge, line, sizeof(line));
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%c/%c)", config.START_EDGE[0], config.START_EDGE[1]);
  configPrintln(line);
  
  // E - Sync Mode
  copyProgStrToBuffer(it_sync, line, sizeof(line));
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%c)", config.SYNC_MODE);
  configPrintln(line);
  
  // F - Channel Names
  copyProgStrToBuffer(it_names, line, sizeof(line));
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%c/%c)", config.NAME[0], config.NAME[1]);
  configPrintln(line);
  
  // G - Poll Character
  copyProgStrToBuffer(it_pollchar, line, sizeof(line));
  if (config.POLL_CHAR) {
    snprintf(line + strlen(line), sizeof(line) - strlen(line), "%c)", config.POLL_CHAR);
  } else {
    strcat_P(line, msg_poll_none);
  }
  configPrintln(line);
  
  // H - Advanced
  copyProgStrToBuffer(it_advanced, line, sizeof(line));
  configPrintln(line);
  
  // I - Baud Rate
  copyProgStrToBuffer(it_baud, line, sizeof(line));
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%lu)", (unsigned long)config.BAUD_RATE);
  configPrintln(line);
  
  configPrintln("");
  
  copyProgStrToBuffer(it_show_menu, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_printcfg, line, sizeof(line));
  configPrintln(line);
  
  copyProgStrToBuffer(it_version, line, sizeof(line));
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
  strcat_P(line, getModeName(config.MODE));
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
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%ld.%06ld MHz)", (int32_t)MHz, (int32_t)fract);
  configPrintln(line);
  
  // H2 - Coarse Tick
  copyProgStrToBuffer(it_adv_pictick, line, sizeof(line));
  int64_t us = config.PICTICK_PS / 1000000;
  int64_t ps = us * 1000000;
  int64_t ps_fract = config.PICTICK_PS - ps;
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%ld.%06ld usec)", (int32_t)us, (int32_t)ps_fract);
  configPrintln(line);
  
  // H3 - Propagation Delay
  copyProgStrToBuffer(it_adv_prop, line, sizeof(line));
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%ld/%ld)", (long)config.PROP_DELAY[0], (long)config.PROP_DELAY[1]);
  configPrintln(line);
  
  // H4 - Time Dilation
  copyProgStrToBuffer(it_adv_dilation, line, sizeof(line));
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%ld/%ld)", (long)config.TIME_DILATION[0], (long)config.TIME_DILATION[1]);
  configPrintln(line);
  
  // H5 - Fixed Time2
  copyProgStrToBuffer(it_adv_fixed, line, sizeof(line));
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%ld/%ld)", (long)config.FIXED_TIME2[0], (long)config.FIXED_TIME2[1]);
  configPrintln(line);
  
  // H6 - FUDGE0
  copyProgStrToBuffer(it_adv_fudge, line, sizeof(line));
  snprintf(line + strlen(line), sizeof(line) - strlen(line), "%ld/%ld)", (long)config.FUDGE0[0], (long)config.FUDGE0[1]);
  configPrintln(line);
  
  configPrintln("");
}

// Process mode submenu commands - now handled by table-driven system
bool process_mode_command(char cmd, const char* args, bool interactive) {
  return process_generic_command(cmd, args, interactive, mode_commands, MODE_COMMANDS_SIZE);
}

// Process baud rate submenu commands - now handled by table-driven system
bool process_baud_command(char cmd, const char* args, bool interactive) {
  return process_generic_command(cmd, args, interactive, baud_commands, BAUD_COMMANDS_SIZE);
}

// Process advanced submenu commands - now handled by table-driven system
bool process_advanced_command(char cmd, const char* args, bool interactive) {
  return process_generic_command(cmd, args, interactive, advanced_commands, ADVANCED_COMMANDS_SIZE);
}

// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================

// Process wrap command - now handled by table-driven system
bool process_wrap_command(char cmd, const char* args, bool interactive) {
  return process_generic_command(cmd, args, interactive, main_menu_commands, MAIN_MENU_COMMANDS_SIZE);
}

// Process places command - now handled by table-driven system
bool process_places_command(char cmd, const char* args, bool interactive) {
  return process_generic_command(cmd, args, interactive, main_menu_commands, MAIN_MENU_COMMANDS_SIZE);
}

// Process edge command - now handled by table-driven system
bool process_edge_command(char cmd, const char* args, bool interactive) {
  return process_generic_command(cmd, args, interactive, main_menu_commands, MAIN_MENU_COMMANDS_SIZE);
}

// Process sync command - now handled by table-driven system
bool process_sync_command(char cmd, const char* args, bool interactive) {
  return process_generic_command(cmd, args, interactive, main_menu_commands, MAIN_MENU_COMMANDS_SIZE);
}

// Process names command - now handled by table-driven system
bool process_names_command(char cmd, const char* args, bool interactive) {
  return process_generic_command(cmd, args, interactive, main_menu_commands, MAIN_MENU_COMMANDS_SIZE);
}

// Process poll command - now handled by table-driven system
bool process_poll_command(char cmd, const char* args, bool interactive) {
  return process_generic_command(cmd, args, interactive, main_menu_commands, MAIN_MENU_COMMANDS_SIZE);
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

// Process version command
bool process_version_command() {
  // Build complete version string using cursor appender
  char versionStr[128];
  char* p = app_init(versionStr, sizeof(versionStr));
  size_t r = sizeof(versionStr);
  app_s(p, r, "Firmware version: ");
  app_s(p, r, SW_VERSION);
  if (strlen(SW_TAG) > 0) {
    app_s(p, r, " (");
    app_s(p, r, SW_TAG);
    app_c(p, r, ')');
  }
  app_s(p, r, ", Board serial: ");
  app_s(p, r, SER_NUM);
  
  // Single print call - configPrintln adds # prefix
  configPrintln(versionStr);
  return true;
}

// Process write command
bool process_write_command() {
  eeprom_write_config();
  config_changed = 0;
  configPrintln("Changes written to EEPROM (will persist across restarts)");
  
  // Ask if user wants to restart to apply changes
  configPrintln("");
  configPrintln("Restart now to apply changes?");
  configPrintln("1 - Yes, restart now");
  configPrintln("2 - No, continue with current settings");
  configPrintln("");
  configPrint("> ");
  
  char input[8];
  readLine(input, sizeof(input));
  char *trimmed = trimInPlace(input);
  
  if (strcmp(trimmed, "1") == 0) {
    configPrintln("Restarting with new settings...");
    request_restart = 1;  // Set restart flag
    return false; // Exit config system with restart
  } else {
    configPrintln("Continuing with current settings.");
    configPrintln("(Changes will take effect after restart)");
    return true; // Stay in config system
  }
}

// Process EEPROM clear command
bool process_eeprom_clear_command() {
  configPrintlnProg(ln_eeprom_warning);
  configPrintln("");
  configPrintlnProg(ln_eeprom_confirm);
  configPrint("> ");
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
      configPrintln("Applying changes and resuming operation...");
      config_changed = 0;
      return false; // Exit config system with resume
    case '3':
      config = defaultConfig();
      eeprom_write_config();
      configPrintln("Defaults written. Restarting...");
      request_restart = 1;  // Set restart flag
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
