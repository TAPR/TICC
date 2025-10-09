#ifndef CONFIG_MENU_TEXT_H
#define CONFIG_MENU_TEXT_H

#include <Arduino.h>

// Menu titles
const char pg_main_title[]     PROGMEM = "== TICC Configuration ==";
const char pg_mode_title[]     PROGMEM = "-- Mode --";
const char pg_serial_title[]   PROGMEM = "-- Serial Baud Rate Settings --";
const char pg_advanced_title[] PROGMEM = "-- Advanced Settings --";

// Main menu items
const char it_mode[]           PROGMEM = "A - Mode (currently: ";
const char it_wrap[]           PROGMEM = "B - Timestamp Wrap digits (currently: ";
const char it_places[]         PROGMEM = "C - Output Decimal Places (currently: ";
const char it_edge[]           PROGMEM = "D - Trigger Edge A/B (currently: ";  // Hidden but kept for undocumented access
const char it_sync[]           PROGMEM = "D - Primary/Secondary (currently: ";
const char it_names[]          PROGMEM = "E - Channel Names (currently: ";
const char it_pollchar[]       PROGMEM = "F - Poll Character (currently: ";
const char it_advanced[]       PROGMEM = "G - Advanced settings";
const char it_baud[]           PROGMEM = "H - Serial Baud Rate (currently: ";
const char it_printcfg[]       PROGMEM = "S - Show startup info";
const char it_version[]        PROGMEM = "V - Show firmware version";
const char it_save[]           PROGMEM = "W - Write changes to EEPROM (persist across restarts)";

// Menu navigation
const char it_show_menu[]      PROGMEM = "M - Show this menu again";
const char it_exit1[]          PROGMEM = "1 - Discard changes and exit";
const char it_exit2[]          PROGMEM = "2 - Apply changes and resume operation";
const char it_exit3[]          PROGMEM = "3 - Reset all to defaults and restart";

// Mode submenu items
const char it_mode_ts[]        PROGMEM = "A1 - Timestamp";
const char it_mode_paired[]    PROGMEM = "A2 - Paired Timestamp";
const char it_mode_bin[]       PROGMEM = "A3 - Binary Timestamp";
const char it_mode_int[]       PROGMEM = "A4 - Time Interval A -> B";
const char it_mode_period[]    PROGMEM = "A5 - Period";
const char it_mode_3ch[]       PROGMEM = "A6 - 3-Cornered Hat";
const char it_mode_debug[]     PROGMEM = "A7 - Debug";
const char it_mode_null[]      PROGMEM = "A8 - Null Output";

// Mode descriptions
const char desc_timestamp[]    PROGMEM = "Timestamp";
const char desc_paired[]       PROGMEM = "Paired Timestamp";
const char desc_binary[]       PROGMEM = "Binary Timestamp";
const char desc_interval[]     PROGMEM = "Time Interval A->B";
const char desc_period[]       PROGMEM = "Period";
const char desc_timelab[]      PROGMEM = "3-Cornered Hat";
const char desc_debug[]        PROGMEM = "Debug";
const char desc_null[]         PROGMEM = "Null Output";

// Serial baud rate options
const char it_baud_9600[]      PROGMEM = "H1 - 9600 bps";
const char it_baud_19200[]     PROGMEM = "H2 - 19200 bps";
const char it_baud_38400[]     PROGMEM = "H3 - 38400 bps";
const char it_baud_57600[]     PROGMEM = "H4 - 57600 bps";
const char it_baud_115200[]    PROGMEM = "H5 - 115200 bps (default)";
const char it_baud_230400[]    PROGMEM = "H6 - 230400 bps";

// Advanced submenu items
const char it_adv_clock[]      PROGMEM = "G1 - Clock Speed MHz (currently: ";
const char it_adv_pictick[]    PROGMEM = "G2 - Coarse Tick us (currently: ";
const char it_adv_prop[]       PROGMEM = "G3 - Propagation Delay ps A/B (currently: ";
const char it_adv_dilation[]   PROGMEM = "G4 - Time Dilation A/B (currently: ";
const char it_adv_fixed[]      PROGMEM = "G5 - fixedTime2 ps A/B (currently: ";
const char it_adv_fudge[]      PROGMEM = "G6 - FUDGE0 ps A/B (currently: ";

// Prompts and status messages
const char ln_current_mode[]   PROGMEM = "Current mode: ";
const char ln_wrap_no[]        PROGMEM = " - no wrap)";
const char ln_wrap_seconds[]   PROGMEM = " seconds)";
const char ln_wrap_power[]     PROGMEM = " seconds)";
const char ln_current_places[] PROGMEM = ")";
const char ln_current_edge[]   PROGMEM = ")";
const char ln_current_sync[]   PROGMEM = ")";
const char ln_current_names[]  PROGMEM = ")";
const char ln_current_poll[]   PROGMEM = "none)";
const char ln_current_baudrate[] PROGMEM = ")";

// Input prompts
const char prompt_wrap[]       PROGMEM = "Wrap Digits (0..10): ";
const char prompt_places[]     PROGMEM = "Output Decimal Places (0..12): ";
const char prompt_edge[]       PROGMEM = "Enter Edges A/B (R or F): ";
const char prompt_sync[]       PROGMEM = "Enter P or S: ";
const char prompt_names[]      PROGMEM = "Enter Names A/B (any two chars): ";
const char prompt_poll[]       PROGMEM = "Enter Poll Character (- to clear): ";

// Advanced prompts
const char prompt_clock[]      PROGMEM = "Clock Hz (e.g., 1e7 for 10 MHz): ";
const char prompt_pictick[]    PROGMEM = "Coarse Tick ps (e.g., 1e8 for 100 us): ";
const char prompt_prop[]       PROGMEM = "Propagation Delay A/B ps (e.g., 40/40 or 1e6/2e6): ";
const char prompt_dilation[]   PROGMEM = "Time Dilation A/B ps (e.g., 40/40 or 1e6/2e6): ";
const char prompt_fixed[]      PROGMEM = "Fixed Time2 A/B ps (e.g., 40/40 or 1e6/2e6): ";
const char prompt_fudge[]      PROGMEM = "FUDGE0 A/B ps (e.g., 40/40 or 1e6/2e6): ";

// Confirmation messages
const char ln_save_eeprom[]    PROGMEM = "Save to EEPROM with 'W' and restart for change to take effect.";
const char ln_keep_changes[]   PROGMEM = "Changes kept.";
const char ln_discard_changes[] PROGMEM = "Changes discarded.";
const char ln_1_keep[]         PROGMEM = "1 - Keep changes";
const char ln_2_discard[]      PROGMEM = "2 - Discard changes";

// Mode change messages
const char ln_mode_discarded[] PROGMEM = "Mode changes discarded.";
const char ln_mode_kept[]      PROGMEM = "Mode changes kept.";

// Mode names
const char mode_timestamp[]    PROGMEM = "Paired Timestamp";
const char mode_strict[]       PROGMEM = "Strict Timestamp";
const char mode_immediate[]    PROGMEM = "Immediate Timestamp";
const char mode_binary[]       PROGMEM = "Binary Timestamp";
const char mode_interval[]     PROGMEM = "Time Interval A->B";
const char mode_period[]       PROGMEM = "Period";
const char mode_timelab[]      PROGMEM = "3-Cornered Hat";
const char mode_debug[]        PROGMEM = "Debug";
const char mode_null[]         PROGMEM = "Null Output";
const char mode_unknown[]      PROGMEM = "Unknown";

// Wrap descriptions
const char wrap_no_wrap[]      PROGMEM = " - no wrap";
const char wrap_format[]       PROGMEM = " - wraps at %lu seconds";
const char wrap_scientific[]   PROGMEM = " - wraps at 1e%d seconds";

// Confirmation message formats
const char msg_ok_pair[]       PROGMEM = "OK -- %s %ld/%ld -> %ld/%ld ps";
const char msg_ok_mode[]       PROGMEM = "OK -- Mode set to ";
const char msg_ok_baud_was[]   PROGMEM = "OK -- Baud rate was ";
const char msg_ok_baud_now[]   PROGMEM = "; now ";
const char msg_ok_baud_already[] PROGMEM = "OK -- Baud rate is already ";
const char msg_ok_clock[]      PROGMEM = "OK -- Clock %s -> %s";
const char msg_poll_none[]     PROGMEM = "none)";

// Error messages
const char ln_invalid[]        PROGMEM = "Invalid";
const char ln_unknown[]        PROGMEM = "Unknown command";

// Warning messages
const char ln_falling_edge_warning[] PROGMEM = "WARNING: Falling edge trigger is unreliable; use at your own risk!";

// EEPROM clear messages
const char ln_eeprom_warning[] PROGMEM = "WARNING: This will completely erase the entire EEPROM including serial number!";
const char ln_eeprom_confirm[] PROGMEM = "Type 'YES' to confirm: ";
const char ln_eeprom_clearing[] PROGMEM = "Clearing entire EEPROM...";
const char ln_eeprom_cleared[] PROGMEM = "EEPROM cleared. Restarting...";
const char ln_eeprom_cancelled[] PROGMEM = "EEPROM clear cancelled.";

// Config change messages
const char ln_restart_default[] PROGMEM = "Restarting with default settings.";
const char ln_restart_new[] PROGMEM = "Restarting with new settings.";
const char ln_restart_required[] PROGMEM = "Configuration changes require restart. Restarting...";
const char ln_applying_changes[] PROGMEM = "Applying configuration changes...";
const char ln_resuming_operation[] PROGMEM = "Resuming operation with new settings.";
const char ln_changes_temporary[] PROGMEM = "(Changes are temporary - will revert on restart)";

// Write command messages
const char ln_changes_written[] PROGMEM = "Changes written to EEPROM (will persist across restarts)";
const char ln_restart_now[] PROGMEM = "Restart now to apply changes?";
const char ln_1_yes_restart[] PROGMEM = "1 - Yes, restart now";
const char ln_2_no_continue[] PROGMEM = "2 - No, continue with current settings";
const char ln_continuing_settings[] PROGMEM = "Continuing with current settings.";
const char ln_changes_after_restart[] PROGMEM = "(Changes will take effect after restart)";

// Exit command messages
const char ln_discarded_changes[] PROGMEM = "Discarded changes.";
const char ln_applying_resuming[] PROGMEM = "Applying changes and resuming operation...";
const char ln_defaults_written[] PROGMEM = "Defaults written. Restarting...";

#endif
