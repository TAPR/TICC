#ifndef CONFIG_TYPES_H
#define CONFIG_TYPES_H

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>
#include "config.h"  // Use existing config_t and MeasureMode definitions

// Configuration change tracking
extern uint8_t config_changed;
extern config_t config;
extern config_t config_backup;

// Function prototypes
void init_config_system();
void backup_config();
uint8_t config_change_requires_restart();
void apply_config_changes();
void handle_config_change_exit();

// EEPROM operations
void eeprom_write_config();
void eeprom_read_config();
void eeprom_clear();
struct config_t defaultConfig();

// Configuration display and interaction
void show_config_menu();
bool process_config_command(const char* cmd, bool interactive = true);

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
bool process_write_command();
bool process_eeprom_clear_command();
bool process_exit_command(char cmd);

// Menu display functions
void show_main_menu();
void show_mode_menu();
void show_baud_menu();
void show_advanced_menu();

#endif // CONFIG_TYPES_H
