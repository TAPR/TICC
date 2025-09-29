// config_display.cpp -- Display and menu interface functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2020
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Portions Copyright David McQuate WA8YWQ 2016
// Licensed under BSD 2-clause license

#include <stdint.h>
#include <Arduino.h>
#include "config.h"

// External variables
extern uint8_t config_changed;

// Macro to mark config as changed
#define MARK_CONFIG_CHANGED() do { config_changed = 1; } while(0)

// Serial I/O helper functions are in config_core.cpp

// Main configuration menu display and interaction
void doSetupMenu(struct config_t *pConfigInfo) {
  char buf[96];
  bool showMenu = true;
  for (;;) {
    if (showMenu) {
      configPrint("\r\n");
      configPrint("== TICC Configuration ==\r\n");
      // A) Mode
      configPrint("A - Mode (currently: ");
      switch (pConfigInfo->MODE) {
        case Timestamp: serialPrintImmediate("Timestamp"); break;
        case Binary:    serialPrintImmediate("Binary Timestamp"); break;
        case Period:    serialPrintImmediate("Period"); break;
        case Interval:  serialPrintImmediate("Interval A->B"); break;
        case timeLab:   serialPrintImmediate("TimeLab 3-ch"); break;
        case Debug:     serialPrintImmediate("Debug"); break;
        case Null:      serialPrintImmediate("Null"); break;
      }
      serialPrintImmediate(")\r\n");
      // B) Wrap digits
      {
        char tmp[64]; 
        if (pConfigInfo->WRAP <= 0) {
          sprintf(tmp, "B - Timestamp Wrap digits (currently: %d - no wrap)\r\n", (int)pConfigInfo->WRAP);
        } else if (pConfigInfo->WRAP <= 9) {
          uint32_t wrap_seconds = 1;
          for (int i = 0; i < pConfigInfo->WRAP; i++) wrap_seconds *= 10;
          sprintf(tmp, "B - Timestamp Wrap digits (currently: %d - wraps at %lu seconds)\r\n", (int)pConfigInfo->WRAP, (unsigned long)wrap_seconds);
        } else {
          sprintf(tmp, "B - Timestamp Wrap digits (currently: %d - wraps at 1e%d seconds)\r\n", (int)pConfigInfo->WRAP, (int)pConfigInfo->WRAP);
        }
        configPrint(tmp);
      }
      // C) Output decimal places
      {
        char tmp[48]; sprintf(tmp, "C - Output Decimal Places (currently: %d)\r\n", (int)pConfigInfo->PLACES);
        configPrint(tmp);
      }
      // D) Trigger edges
      {
        char tmp[64]; sprintf(tmp, "D - Trigger Edge A/B (currently: %c/%c)\r\n", pConfigInfo->START_EDGE[0], pConfigInfo->START_EDGE[1]);
        configPrint(tmp);
      }
      // E) Sync mode
      {
      char tmp[48]; sprintf(tmp, "E - Primary/Secondary (currently: %c)\r\n", pConfigInfo->SYNC_MODE);
        configPrint(tmp);
      }
      // F) Channel names
      {
        char tmp[48]; sprintf(tmp, "F - Channel Names (currently: %c/%c)\r\n", pConfigInfo->NAME[0], pConfigInfo->NAME[1]);
        configPrint(tmp);
      }
      // G) Poll char
      configPrint("G - Poll Character (currently: ");
      if (pConfigInfo->POLL_CHAR) {
        char ch[8]; ch[0] = pConfigInfo->POLL_CHAR; ch[1] = 0; serialPrintImmediate(ch);
      } else {
        serialPrintImmediate("none");
      }
      serialPrintImmediate(")\r\n");
      // H) Advanced settings
      configPrint("H - Advanced settings\r\n");
      // I) Serial baud rate
      {
        char tmp[64]; sprintf(tmp, "I - Serial Baud Rate (currently: %lu)\r\n", (unsigned long)pConfigInfo->BAUD_RATE);
        configPrint(tmp);
      }
      configPrint("\r\n");
      configPrint("M - Show this menu again\r\n");
      configPrint("J - Show startup info\r\n");
      configPrint("W - Write changes to EEPROM (persist across restarts)\r\n");
      configPrint("\r\n");
      configPrint("1 - Discard changes and exit\r\n");
      configPrint("2 - Apply changes and restart\r\n");
      configPrint("3 - Apply changes and resume operation\r\n");
      configPrint("4 - Reset all to defaults and restart\r\n");
      showMenu = false;
    }
    configPrint("> ");
    Serial.flush();
    size_t n = readLine(buf, sizeof(buf));
    char *line = trimInPlace(buf);
    if (n == 0) {
      // Empty input - just continue to next iteration without showing menu again
      continue;
    }
    serialDrain();

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
        if (!processCommand(pConfigInfo, cmd_line, &showMenu)) {
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
  }
}

// User configuration request and startup display
void UserConfig(struct config_t *pConfigInfo) {
    char c;
    // Do not block on Serial readiness; setup() already delays after begin

    Serial.println("# Type any character for config menu");
    Serial.print("# ");
    bool configRequested = 0;
    for (int i = 6; i >= 0; --i)  // wait ~6 sec so user can type something
    { 
      delay(250);   Serial.print('.'); if (Serial.available()) { configRequested = 1; break; }
      delay(250);   Serial.print('.'); if (Serial.available()) { configRequested = 1; break; }
      delay(250);   Serial.print('.'); if (Serial.available()) { configRequested = 1; break; }
      delay(250);   Serial.print('.'); if (Serial.available()) { configRequested = 1; break; }
    }
    Serial.println();
    while (Serial.available()) c = Serial.read();   // eat any characters entered before we start  doSetupMenu()
    if (configRequested) doSetupMenu(pConfigInfo); 
}
