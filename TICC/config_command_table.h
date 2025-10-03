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

// Command table entry structure
struct CommandEntry {
  char cmd_letter;                    // The command letter (A, B, C, etc.)
  CommandType type;                   // Type of command
  bool requires_submenu;              // True if this command shows a submenu
  bool (*handler_func)(char cmd, const char* args, bool interactive);  // Handler function
  void (*submenu_func)();             // Function to show submenu (if applicable)
};

// Command table - defined in config_command_table.cpp
extern const CommandEntry command_table[] PROGMEM;
extern const size_t COMMAND_TABLE_SIZE;

// Function to find a command in the table
const CommandEntry* find_command(char cmd_letter);

#endif // CONFIG_COMMAND_TABLE_H
