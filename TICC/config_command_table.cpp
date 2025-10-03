// config_command_table.cpp -- Command table implementation

#include <Arduino.h>
#include "config_command_table.h"

// Command table - stored in PROGMEM for memory efficiency
const CommandEntry command_table[] PROGMEM = {
  // Main menu commands
  {'A', CMD_SUBMENU, true,  process_mode_command,    show_mode_menu},
  {'B', CMD_DIRECT,  false, process_wrap_command,    nullptr},
  {'C', CMD_DIRECT,  false, process_places_command,  nullptr},
  {'D', CMD_DIRECT,  false, process_edge_command,    nullptr},
  {'E', CMD_DIRECT,  false, process_sync_command,    nullptr},
  {'F', CMD_DIRECT,  false, process_names_command,   nullptr},
  {'G', CMD_DIRECT,  false, process_poll_command,    nullptr},
  {'H', CMD_SUBMENU, true,  process_advanced_command, show_advanced_menu},
  {'I', CMD_SUBMENU, true,  process_baud_command,    show_baud_menu},
  {'M', CMD_MAIN_MENU, false, process_menu_command,  nullptr},
  {'S', CMD_DIRECT,  false, process_info_command,    nullptr},
  {'V', CMD_DIRECT,  false, process_version_command, nullptr},
  {'W', CMD_DIRECT,  false, process_write_command,   nullptr},
  {'X', CMD_DIRECT,  false, process_eeprom_clear_command, nullptr},
  
  // Exit commands
  {'1', CMD_EXIT, false, process_exit_command, nullptr},
  {'2', CMD_EXIT, false, process_exit_command, nullptr},
  {'3', CMD_EXIT, false, process_exit_command, nullptr},
  {'4', CMD_EXIT, false, process_exit_command, nullptr},
};

// Table size for iteration
const size_t COMMAND_TABLE_SIZE = sizeof(command_table) / sizeof(command_table[0]);

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
