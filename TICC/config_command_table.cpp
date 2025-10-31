// config_command_table.cpp -- Command table implementation

#include <Arduino.h>
#include "config_command_table.h"
#include "config_menu_text.h"
#include "config.h"

// External functions from config_menu.cpp
extern char sharedBuffer[128];
extern char* getInputOrPrompt(const char* args, const char* prompt, char* buffer, size_t bufferSize);
extern bool parseInt64Simple(const char* input, int64_t* result);
extern bool parseScientificNotation(const char* input, int64_t* result);
extern bool parseScientificNotationPair(const char* input, int64_t* resultA, int64_t* resultB);
extern bool parseCharPair(const char* input, bool* set0, char* v0, bool* set1, char* v1, bool* set2 = nullptr, char* v2 = nullptr);
extern bool handleConfirmation(const char* message, bool interactive);
extern bool handlePairConfirmation(int64_t oldA, int64_t oldB, int64_t newA, int64_t newB, 
                                  int64_t* configA, int64_t* configB, const char* name);
extern void configPrintln(const char* message);
extern void configPrintlnProg(const char* message);
extern void configPrint(const char* message);
extern size_t readLine(char* buffer, size_t bufferSize);
extern char* trimInPlace(char* str);
extern void formatHzAsMHz(int64_t hz, char* buffer, size_t bufferSize);
extern void formatPsAsUs(int64_t ps, char* buffer, size_t bufferSize);
extern const char* getModeName(MeasureMode mode);
extern uint8_t config_changed;
extern config_t config;
extern const char msg_ok_mode[];
extern const char msg_ok_baud_was[];
extern const char msg_ok_baud_now[];
extern const char msg_ok_clock[];
extern const char ln_1_keep[];
extern const char ln_2_discard[];
extern const char ln_keep_changes[];
extern const char ln_discard_changes[];
extern const char ln_invalid[];
extern const char ln_falling_edge_warning[];

// Additional extern declarations for functions used in generic processor
extern char* app_init(char* buffer, size_t bufferSize);
extern bool app_p(char* &cur, size_t &rem, const char* prog);
extern bool app_i32(char* &cur, size_t &rem, int32_t v);

// Command table - stored in PROGMEM for memory efficiency
const CommandEntry command_table[] PROGMEM = {
  // Main menu commands
  {'A', CMD_SUBMENU, true,  process_mode_command,    show_mode_menu},
  {'B', CMD_DIRECT,  false, process_wrap_command,    nullptr},
  {'C', CMD_DIRECT,  false, process_places_command,  nullptr},
  {'D', CMD_DIRECT,  false, process_sync_command,    nullptr},
  {'E', CMD_DIRECT,  false, process_names_command,   nullptr},
  {'F', CMD_DIRECT,  false, process_poll_command,    nullptr},
  {'G', CMD_SUBMENU, true,  process_advanced_command, show_advanced_menu},
  {'H', CMD_SUBMENU, true,  process_baud_command,    show_baud_menu},
  {'M', CMD_MAIN_MENU, false, process_menu_command,  nullptr},
  {'S', CMD_DIRECT,  false, process_info_command,    nullptr},
  {'V', CMD_DIRECT,  false, process_version_command, nullptr},
  {'W', CMD_DIRECT,  false, process_write_wrapper, nullptr},
  {'X', CMD_DIRECT,  false, process_eeprom_clear_command, nullptr},
  
  // Undocumented commands (hidden from menu)
  {'T', CMD_DIRECT,  false, process_edge_command,    nullptr},  // 'T' for undocumented trigger edge
  
  // Exit commands
  {'1', CMD_EXIT, false, process_exit_command, nullptr},
  {'2', CMD_EXIT, false, process_exit_command, nullptr},
  {'3', CMD_EXIT, false, process_exit_command, nullptr},
  {'4', CMD_EXIT, false, process_exit_command, nullptr},
};

// Table size for iteration
const size_t COMMAND_TABLE_SIZE = sizeof(command_table) / sizeof(command_table[0]);

// Main menu commands table
const CommandConfig main_menu_commands[] PROGMEM = {
  // B - Wrap Digits
  {'B', SIMPLE_INT, prompt_wrap, 0, 10, "", &config.WRAP, 0, "Wrap Digits", CONFIRM_HANDLE, 0},
  
  // C - Decimal Places
  {'C', SIMPLE_INT, prompt_places, 0, 12, "", &config.PLACES, 0, "Decimal Places", CONFIRM_HANDLE, 0},
  
  // D - Sync Mode (was E)
  {'D', SIMPLE_CHAR, prompt_sync, 0, 0, "PS", &config.SYNC_MODE, 0, "Sync Mode", CONFIRM_HANDLE, 0},
  
  // E - Channel Names (was F)
  {'E', CHAR_PAIR, prompt_names, 0, 0, "", config.NAME, 0, "Channel Names", CONFIRM_HANDLE, 0},
  
  // F - Poll Character (was G)
  {'F', SIMPLE_CHAR, prompt_poll, 0, 0, "", &config.POLL_CHAR, 0, "Poll Character", CONFIRM_HANDLE, 0},
  
  // T - Start Edge (undocumented, was D)
  {'T', CHAR_PAIR, prompt_edge, 0, 0, "RF", config.START_EDGE, 0, "Start Edge", CONFIRM_HANDLE, 0}
};

const size_t MAIN_MENU_COMMANDS_SIZE = sizeof(main_menu_commands) / sizeof(main_menu_commands[0]);

// Advanced commands table (was H, now G)
const CommandConfig advanced_commands[] PROGMEM = {
  // G1 - Clock Speed (was H1)
  {'1', SCIENTIFIC, prompt_clock, 1000000, 16000000, "", &config.CLOCK_HZ, 0, "Clock Speed", CONFIRM_MANUAL, 0},
  
  // G2 - Coarse Tick (was H2)
  {'2', SCIENTIFIC, prompt_pictick, 100000000, 100000000000, "", &config.PICTICK_PS, 0, "Coarse Tick", CONFIRM_MANUAL, 0},
  
  // G3 - Propagation Delay (was H3)
  {'3', SCIENTIFIC_PAIR, prompt_prop, 0, 1000000000000, "", config.PROP_DELAY, 0, "PropDelay", CONFIRM_PAIR, 0},
  
  // G4 - Time Dilation (was H4)
  {'4', SCIENTIFIC_PAIR, prompt_dilation, 0, 1000000000000, "", config.TIME_DILATION, 0, "Time Dilation", CONFIRM_PAIR, 0},
  
  // G5 - Fixed Time2 (was H5)
  {'5', SCIENTIFIC_PAIR, prompt_fixed, 0, 1000000000000, "", config.FIXED_TIME2, 0, "fixedTime2", CONFIRM_PAIR, 0},
  
  // G6 - FUDGE0 (was H6)
  {'6', SCIENTIFIC_PAIR, prompt_fudge, 0, 1000000000000, "", config.FUDGE0, 0, "FUDGE0", CONFIRM_PAIR, 0}
};

const size_t ADVANCED_COMMANDS_SIZE = sizeof(advanced_commands) / sizeof(advanced_commands[0]);

// Mode submenu commands table
const CommandConfig mode_commands[] PROGMEM = {
   {'1', SUBMENU_SELECTION, NULL, 0, 0, "", &config.MODE, 0, "Mode", CONFIRM_MANUAL, Timestamp},
  {'2', SUBMENU_SELECTION, NULL, 0, 0, "", &config.MODE, 0, "Mode", CONFIRM_MANUAL, Paired_Timestamp},
  {'3', SUBMENU_SELECTION, NULL, 0, 0, "", &config.MODE, 0, "Mode", CONFIRM_MANUAL, Binary},
  {'4', SUBMENU_SELECTION, NULL, 0, 0, "", &config.MODE, 0, "Mode", CONFIRM_MANUAL, Interval},
  {'5', SUBMENU_SELECTION, NULL, 0, 0, "", &config.MODE, 0, "Mode", CONFIRM_MANUAL, Period},
  {'6', SUBMENU_SELECTION, NULL, 0, 0, "", &config.MODE, 0, "Mode", CONFIRM_MANUAL, Hat},
  {'7', SUBMENU_SELECTION, NULL, 0, 0, "", &config.MODE, 0, "Mode", CONFIRM_MANUAL, Debug},
  {'8', SUBMENU_SELECTION, NULL, 0, 0, "", &config.MODE, 0, "Mode", CONFIRM_MANUAL, Null}
};

const size_t MODE_COMMANDS_SIZE = sizeof(mode_commands) / sizeof(mode_commands[0]);

// Baud rate submenu commands table (was I, now H)
const CommandConfig baud_commands[] PROGMEM = {
  {'1', SUBMENU_SELECTION, NULL, 0, 0, "", &config.BAUD_RATE, 0, "Baud Rate", CONFIRM_MANUAL, 9600},
  {'2', SUBMENU_SELECTION, NULL, 0, 0, "", &config.BAUD_RATE, 0, "Baud Rate", CONFIRM_MANUAL, 19200},
  {'3', SUBMENU_SELECTION, NULL, 0, 0, "", &config.BAUD_RATE, 0, "Baud Rate", CONFIRM_MANUAL, 38400},
  {'4', SUBMENU_SELECTION, NULL, 0, 0, "", &config.BAUD_RATE, 0, "Baud Rate", CONFIRM_MANUAL, 57600},
  {'5', SUBMENU_SELECTION, NULL, 0, 0, "", &config.BAUD_RATE, 0, "Baud Rate", CONFIRM_MANUAL, 115200},
  {'6', SUBMENU_SELECTION, NULL, 0, 0, "", &config.BAUD_RATE, 0, "Baud Rate", CONFIRM_MANUAL, 230400}
};

const size_t BAUD_COMMANDS_SIZE = sizeof(baud_commands) / sizeof(baud_commands[0]);

// Function to find a command in the table
const CommandEntry* find_command(char cmd_letter) {
  for (size_t i = 0; i < COMMAND_TABLE_SIZE; i++) {
    // Read individual fields from PROGMEM instead of copying the whole struct
    char cmd = pgm_read_byte(&command_table[i].cmd_letter);
    
    if (cmd == cmd_letter) {
      return &command_table[i];
    }
  }
  return nullptr;  // Command not found
}

// Function to find a command in a configuration table
const CommandConfig* find_command_in_table(char cmd, const CommandConfig* table, size_t table_size) {
  for (size_t i = 0; i < table_size; i++) {
    char table_cmd = pgm_read_byte(&table[i].cmd_letter);
    if (table_cmd == cmd) {
      return &table[i];
    }
  }
  return nullptr;
}

// Generic command processor for table-driven commands
bool process_generic_command(char cmd, const char* args, bool interactive, const CommandConfig* table, size_t table_size) {
  const CommandConfig* cmd_config = find_command_in_table(cmd, table, table_size);
  if (!cmd_config) return false;
  
  // Read configuration from PROGMEM
  ValueType value_type = (ValueType)pgm_read_byte(&cmd_config->value_type);
  int64_t min_val = (int64_t)pgm_read_dword(&cmd_config->min_val);
  int64_t max_val = (int64_t)pgm_read_dword(&cmd_config->max_val);
  ConfirmationType confirm_type = (ConfirmationType)pgm_read_byte(&cmd_config->confirm_type);
  int64_t enum_value = (int64_t)pgm_read_dword(&cmd_config->enum_value);
  
  // Get prompt string from PROGMEM
  const char* prompt_prog = (const char*)pgm_read_word(&cmd_config->prompt_prog);
  const char* description_prog = (const char*)pgm_read_word(&cmd_config->description_prog);
  
  // Handle submenu selections (no input required)
  if (value_type == SUBMENU_SELECTION) {
    // Get config pointer and update value
    void* config_ptr = (void*)pgm_read_word(&cmd_config->config_ptr);
    if (config_ptr == &config.MODE) {
      MeasureMode oldMode = config.MODE;
      config.MODE = (MeasureMode)enum_value;
      if (oldMode != config.MODE) {
        config_changed = 1;
        // Build confirmation message
        char* p = app_init(sharedBuffer, sizeof(sharedBuffer));
        size_t r = sizeof(sharedBuffer);
        app_p(p, r, msg_ok_mode);
        app_p(p, r, getModeName(config.MODE));
        configPrintln(sharedBuffer);
        
        // Manual confirmation only in interactive mode
        if (interactive) {
          configPrintln("");
          configPrintlnProg(ln_1_keep);
          configPrintlnProg(ln_2_discard);
          configPrint("> ");
          
          readLine(sharedBuffer, sizeof(sharedBuffer));
          char *choice = trimInPlace(sharedBuffer);
          
          if (choice[0] == '2' && strlen(choice) == 1) {
            config.MODE = oldMode;
            config_changed = 0;
            configPrintlnProg(ln_discard_changes);
          } else {
            configPrintlnProg(ln_keep_changes);
          }
        } else {
          // Batch mode - automatically keep changes
          configPrintlnProg(ln_keep_changes);
        }
      }
    } else if (config_ptr == &config.BAUD_RATE) {
      uint32_t oldRate = config.BAUD_RATE;
      config.BAUD_RATE = (uint32_t)enum_value;
      if (oldRate != config.BAUD_RATE) {
        config_changed = 1;
        // Build confirmation message
        char* p = app_init(sharedBuffer, sizeof(sharedBuffer));
        size_t r = sizeof(sharedBuffer);
        app_p(p, r, msg_ok_baud_was);
        app_i32(p, r, (int32_t)oldRate);
        app_p(p, r, msg_ok_baud_now);
        app_i32(p, r, (int32_t)config.BAUD_RATE);
        configPrintln(sharedBuffer);
        
        // Manual confirmation only in interactive mode
        if (interactive) {
          configPrintln("");
          configPrintlnProg(ln_1_keep);
          configPrintlnProg(ln_2_discard);
          configPrint("> ");
          
          readLine(sharedBuffer, sizeof(sharedBuffer));
          char *choice = trimInPlace(sharedBuffer);
          
          if (choice[0] == '2' && strlen(choice) == 1) {
            config.BAUD_RATE = oldRate;
            config_changed = 0;
            configPrintlnProg(ln_discard_changes);
          } else {
            configPrintlnProg(ln_keep_changes);
          }
        } else {
          // Batch mode - automatically keep changes
          configPrintlnProg(ln_keep_changes);
        }
      }
    }
    return true;
  }
  
  // Handle input commands
  char* input = getInputOrPrompt(args, prompt_prog, sharedBuffer, sizeof(sharedBuffer));
  
  // Empty input (just Enter) means cancel and return to menu
  if (!input) return true;
  
  // Get config pointer early (needed for CHAR_PAIR parsing logic)
  void* config_ptr = (void*)pgm_read_word(&cmd_config->config_ptr);
  
  // Parse input based on type
  bool valid = false;
  int64_t value = 0, valueA = 0, valueB = 0;
  char charValue = 0, charA = 0, charB = 0;
  bool set0 = false, set1 = false;
  char char3CH = 0;
  bool set3CH = false;
  
  switch (value_type) {
    case SIMPLE_INT:
      valid = parseInt64Simple(input, &value) && value >= min_val && value <= max_val;
      break;
    case SIMPLE_CHAR:
      if (strlen(input) == 1) {
        charValue = toupper(input[0]);
        // Check if character is in allowed set
        char allowed[8];
        memcpy_P(allowed, cmd_config->allowed_chars, sizeof(allowed));
        valid = (strlen(allowed) == 0) || (strchr(allowed, charValue) != nullptr);
      }
      break;
    case SCIENTIFIC:
      valid = parseScientificNotation(input, &value) && value >= min_val && value <= max_val;
      break;
    case SCIENTIFIC_PAIR:
      valid = parseScientificNotationPair(input, &valueA, &valueB) && 
              valueA >= min_val && valueA <= max_val && 
              valueB >= min_val && valueB <= max_val;
      break;
    case CHAR_PAIR:
      // For channel names, parse optional third character for 3-Corner Hat mode
      if (config_ptr == config.NAME) {
        valid = parseCharPair(input, &set0, &charA, &set1, &charB, &set3CH, &char3CH);
      } else {
        valid = parseCharPair(input, &set0, &charA, &set1, &charB);
      }
      break;
    default:
      return false;
  }
  
  if (!valid) {
    configPrintlnProg(ln_invalid);
    configPrintln("");
    return true;
  }
  
  // Update configuration based on type
  
  switch (value_type) {
    case SIMPLE_INT:
      if (config_ptr == &config.WRAP) {
        int16_t old = config.WRAP;
        config.WRAP = (int16_t)value;
        config_changed = 1;
        sprintf_P(sharedBuffer, msg_ok_wrap, old, config.WRAP);
        if (!handleConfirmation(sharedBuffer, interactive)) {
          config.WRAP = old;
          config_changed = 0;
        }
      } else if (config_ptr == &config.PLACES) {
        int16_t old = config.PLACES;
        config.PLACES = (int16_t)value;
        config_changed = 1;
        sprintf_P(sharedBuffer, msg_ok_places, old, config.PLACES);
        if (!handleConfirmation(sharedBuffer, interactive)) {
          config.PLACES = old;
          config_changed = 0;
        }
      }
      break;
      
    case SIMPLE_CHAR:
      if (config_ptr == &config.SYNC_MODE) {
        char old = config.SYNC_MODE;
        config.SYNC_MODE = charValue;
        config_changed = 1;
        sprintf_P(sharedBuffer, msg_ok_sync, old, charValue);
        if (!handleConfirmation(sharedBuffer, interactive)) {
          config.SYNC_MODE = old;
          config_changed = 0;
        }
      } else if (config_ptr == &config.POLL_CHAR) {
        char old = config.POLL_CHAR;
        config.POLL_CHAR = (charValue == '-') ? 0x00 : charValue;
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
      }
      break;
      
    case SCIENTIFIC:
      if (config_ptr == &config.CLOCK_HZ) {
        int64_t old = config.CLOCK_HZ;
        config.CLOCK_HZ = value;
        config_changed = 1;
        
        char oldStr[32], newStr[32];
        formatHzAsMHz(old, oldStr, sizeof(oldStr));
        formatHzAsMHz(value, newStr, sizeof(newStr));
        sprintf_P(sharedBuffer, msg_ok_clock, oldStr, newStr);
        configPrintln(sharedBuffer);
        
        // Manual confirmation
        configPrintln("");
        configPrintlnProg(ln_1_keep);
        configPrintlnProg(ln_2_discard);
        configPrint("> ");
        
        readLine(sharedBuffer, sizeof(sharedBuffer));
        char *choice = trimInPlace(sharedBuffer);
        
        if (choice[0] == '2' && strlen(choice) == 1) {
          config.CLOCK_HZ = old;
          config_changed = 0;
          configPrintlnProg(ln_discard_changes);
        } else {
          configPrintlnProg(ln_keep_changes);
        }
      } else if (config_ptr == &config.PICTICK_PS) {
        int64_t old = config.PICTICK_PS;
        config.PICTICK_PS = value;
        config_changed = 1;
        
        char oldStr[32], newStr[32];
        formatPsAsUs(old, oldStr, sizeof(oldStr));
        formatPsAsUs(value, newStr, sizeof(newStr));
        sprintf_P(sharedBuffer, msg_ok_pictick, oldStr, newStr);
        configPrintln(sharedBuffer);
        
        // Manual confirmation
        configPrintln("");
        configPrintlnProg(ln_1_keep);
        configPrintlnProg(ln_2_discard);
        configPrint("> ");
        
        readLine(sharedBuffer, sizeof(sharedBuffer));
        char *choice = trimInPlace(sharedBuffer);
        
        if (choice[0] == '2' && strlen(choice) == 1) {
          config.PICTICK_PS = old;
          config_changed = 0;
          configPrintlnProg(ln_discard_changes);
        } else {
          configPrintlnProg(ln_keep_changes);
        }
      }
      break;
      
    case SCIENTIFIC_PAIR:
      if (config_ptr == config.PROP_DELAY) {
        int64_t oldA = config.PROP_DELAY[0], oldB = config.PROP_DELAY[1];
        config_changed = 1;
        handlePairConfirmation(oldA, oldB, valueA, valueB, &config.PROP_DELAY[0], &config.PROP_DELAY[1], "PropDelay");
      } else if (config_ptr == config.TIME_DILATION) {
        int64_t oldA = config.TIME_DILATION[0], oldB = config.TIME_DILATION[1];
        config_changed = 1;
        handlePairConfirmation(oldA, oldB, valueA, valueB, &config.TIME_DILATION[0], &config.TIME_DILATION[1], "Time Dilation");
      } else if (config_ptr == config.FIXED_TIME2) {
        int64_t oldA = config.FIXED_TIME2[0], oldB = config.FIXED_TIME2[1];
        config_changed = 1;
        handlePairConfirmation(oldA, oldB, valueA, valueB, &config.FIXED_TIME2[0], &config.FIXED_TIME2[1], "fixedTime2");
      } else if (config_ptr == config.FUDGE0) {
        int64_t oldA = config.FUDGE0[0], oldB = config.FUDGE0[1];
        config_changed = 1;
        handlePairConfirmation(oldA, oldB, valueA, valueB, &config.FUDGE0[0], &config.FUDGE0[1], "FUDGE0");
      }
      break;
      
    case CHAR_PAIR:
      if (config_ptr == config.START_EDGE) {
        char old0 = config.START_EDGE[0], old1 = config.START_EDGE[1];
        if (set0) config.START_EDGE[0] = charA;
        if (set1) config.START_EDGE[1] = charB;
        config_changed = 1;
        
        // Check if falling edge is being set and display warning
        if (config.START_EDGE[0] == 'F' || config.START_EDGE[1] == 'F') {
          configPrintlnProg(ln_falling_edge_warning);
          configPrintln("");
        }
        
        sprintf_P(sharedBuffer, msg_ok_edge, old0, old1, config.START_EDGE[0], config.START_EDGE[1]);
        if (!handleConfirmation(sharedBuffer, interactive)) {
          config.START_EDGE[0] = old0;
          config.START_EDGE[1] = old1;
          config_changed = 0;
        }
      } else if (config_ptr == config.NAME) {
        char old0 = config.NAME[0], old1 = config.NAME[1];
        char old3CH = config.NAME_3CH;
        if (set0) config.NAME[0] = charA;
        if (set1) config.NAME[1] = charB;
        if (set3CH) config.NAME_3CH = char3CH;
        config_changed = 1;
        // Always show third channel in parentheses if it's set (non-zero), even if default
        bool show_old_3ch = (old3CH != 0);
        bool show_new_3ch = (config.NAME_3CH != 0);
        
        if (set3CH) {
          // Setting third channel explicitly - show as "A/B -> C/D/E" (not in parentheses when being changed)
          if (show_old_3ch && old3CH != DEFAULT_NAME_3CH) {
            sprintf_P(sharedBuffer, msg_ok_names_3ch, old0, old1, old3CH, config.NAME[0], config.NAME[1], config.NAME_3CH);
          } else {
            sprintf_P(sharedBuffer, msg_ok_names_3ch_new, old0, old1, config.NAME[0], config.NAME[1], config.NAME_3CH);
          }
        } else {
          // Not setting third channel - always show in parentheses if it's set (even if default)
          if (show_new_3ch) {
            sprintf_P(sharedBuffer, msg_ok_names_with_3ch, old0, old1, old3CH, config.NAME[0], config.NAME[1], config.NAME_3CH);
          } else {
            sprintf_P(sharedBuffer, msg_ok_names, old0, old1, config.NAME[0], config.NAME[1]);
          }
        }
        if (!handleConfirmation(sharedBuffer, interactive)) {
          config.NAME[0] = old0;
          config.NAME[1] = old1;
          config.NAME_3CH = old3CH;
          config_changed = 0;
        }
      }
      break;
  }
  
  return true;
}

// Alternative implementation using binary search for better performance with larger tables
// (uncomment if you have many commands and want O(log n) lookup)
/*
#include <algorithm>

struct CommandEntryComparator {
  bool operator()(const CommandEntry& a, const CommandEntry& b) const {
    return a.cmd_letter < b.cmd_letter;
  }
};

const CommandEntry* find_command_binary(char cmd_letter) {
  // Note: This would require the table to be sorted by cmd_letter
  CommandEntry target = {cmd_letter, CMD_DIRECT, false, nullptr, nullptr};
  
  const CommandEntry* result = std::lower_bound(
    command_table, 
    command_table + COMMAND_TABLE_SIZE, 
    target, 
    CommandEntryComparator()
  );
  
  if (result != command_table + COMMAND_TABLE_SIZE && result->cmd_letter == cmd_letter) {
    return result;
  }
  return nullptr;
}
*/
