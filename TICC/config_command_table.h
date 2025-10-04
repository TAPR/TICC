#ifndef CONFIG_COMMAND_TABLE_H
#define CONFIG_COMMAND_TABLE_H

#include <Arduino.h>

// Forward declarations for command processing functions
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
bool process_version_command();
bool process_write_command();
bool process_eeprom_clear_command();
bool process_exit_command(char cmd);

// Forward declarations for menu display functions
void show_mode_menu();
void show_baud_menu();
void show_advanced_menu();

// Command type enumeration
enum CommandType {
  CMD_MAIN_MENU,      // Shows main menu
  CMD_SUBMENU,        // Shows a submenu
  CMD_DIRECT,         // Direct parameter command
  CMD_EXIT            // Exit command
};

// Value type enumeration for table-driven commands
enum ValueType {
  SIMPLE_INT,         // Integer input with range validation
  SIMPLE_CHAR,        // Single character input
  SCIENTIFIC,         // Scientific notation input
  SCIENTIFIC_PAIR,    // Pair of scientific notation values
  CHAR_PAIR,          // Pair of characters
  SUBMENU_SELECTION   // Submenu selection (no input)
};

// Confirmation type enumeration
enum ConfirmationType {
  CONFIRM_HANDLE,     // Use handleConfirmation()
  CONFIRM_PAIR,       // Use handlePairConfirmation()
  CONFIRM_MANUAL      // Use manual confirmation flow
};

// Command table entry structure
struct CommandEntry {
  char cmd_letter;                    // The command letter (A, B, C, etc.)
  CommandType type;                   // Type of command
  bool requires_submenu;              // True if this command shows a submenu
  bool (*handler_func)(char cmd, const char* args, bool interactive);  // Handler function
  void (*submenu_func)();             // Function to show submenu (if applicable)
};

// Command configuration structure for table-driven commands
struct CommandConfig {
  char cmd_letter;                    // The command letter
  ValueType value_type;               // Type of value/input
  const char* prompt_prog;            // PROGMEM prompt string (NULL for submenu selections)
  int64_t min_val, max_val;          // Validation range
  char allowed_chars[8];             // For char validation (e.g., "PS" for sync mode)
  void* config_ptr;                  // Pointer to config field
  size_t config_offset;              // Offset for arrays (0 for single values)
  const char* description_prog;      // PROGMEM description for messages
  ConfirmationType confirm_type;     // Which confirmation handler to use
  int64_t enum_value;               // For submenu selections (Mode enum, Baud rate)
};

// Command table - defined in config_command_table.cpp
extern const CommandEntry command_table[] PROGMEM;
extern const size_t COMMAND_TABLE_SIZE;

// Function to find a command in the table
const CommandEntry* find_command(char cmd_letter);

// Table-driven command processing
bool process_generic_command(char cmd, const char* args, bool interactive, const CommandConfig* table, size_t table_size);
const CommandConfig* find_command_in_table(char cmd, const CommandConfig* table, size_t table_size);

// Shared buffer (defined in config_menu.cpp)
extern char sharedBuffer[128];

// Cursor appender functions (defined in config_menu.cpp)
char* app_init(char* buf, size_t cap);
size_t app_len(const char* buf);
bool app_p(char* &cur, size_t &rem, const char* prog);
bool app_s(char* &cur, size_t &rem, const char* s);
bool app_c(char* &cur, size_t &rem, char c);
bool app_u32(char* &cur, size_t &rem, uint32_t v);
bool app_i32(char* &cur, size_t &rem, int32_t v);

// Command tables - defined in config_command_table.cpp
extern const CommandConfig main_menu_commands[];
extern const size_t MAIN_MENU_COMMANDS_SIZE;
extern const CommandConfig advanced_commands[];
extern const size_t ADVANCED_COMMANDS_SIZE;
extern const CommandConfig mode_commands[];
extern const size_t MODE_COMMANDS_SIZE;
extern const CommandConfig baud_commands[];
extern const size_t BAUD_COMMANDS_SIZE;

#endif // CONFIG_COMMAND_TABLE_H
