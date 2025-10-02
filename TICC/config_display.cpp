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

// Single shared buffer to reduce memory usage - replaces all local buffers
static char sharedBuffer[128];

// Fixed strings stored in PROGMEM to save RAM
const char str_mode[] PROGMEM = "Mode (currently: ";
const char str_wrap[] PROGMEM = "B - Timestamp Wrap digits (currently: ";
const char str_places[] PROGMEM = "C - Output Decimal Places (currently: ";
const char str_trigger[] PROGMEM = "D - Trigger Edge A/B (currently: ";
const char str_sync[] PROGMEM = "E - Primary/Secondary (currently: ";
const char str_names[] PROGMEM = "F - Channel Names (currently: ";
const char str_poll[] PROGMEM = "G - Poll Character (currently: ";
const char str_advanced[] PROGMEM = "H - Advanced settings";
const char str_baud[] PROGMEM = "I - Serial Baud Rate (currently: ";
const char str_menu[] PROGMEM = "M - Show this menu again";
const char str_info[] PROGMEM = "J - Show startup info";
const char str_write[] PROGMEM = "W - Write changes to EEPROM (persist across restarts)";
const char str_exit1[] PROGMEM = "1 - Discard changes and exit";
const char str_exit2[] PROGMEM = "2 - Apply changes and restart";
const char str_exit3[] PROGMEM = "3 - Apply changes and resume operation";
const char str_exit4[] PROGMEM = "4 - Reset all to defaults and restart";
const char str_show_menu[] PROGMEM = "Show this menu again\r\n";
const char str_startup_info[] PROGMEM = "Show startup info\r\n";
const char str_write_eeprom[] PROGMEM = "Write changes to EEPROM (persist across restarts)\r\n";

// Helper function to print PROGMEM strings to shared buffer
static void copyProgStrToBuffer(const char* str, char* buffer, size_t bufferSize) {
  size_t i = 0;
  char c;
  while ((c = pgm_read_byte(str++)) != 0 && i < bufferSize - 1) {
    buffer[i++] = c;
  }
  buffer[i] = '\0';
}

// Helper function to print confirmation messages without sprintf
static void printConfirmation(const char* prefix, const char* oldVal, const char* newVal) {
  Serial.print("# ");
  Serial.print(prefix);
  if (oldVal) {
    Serial.print(oldVal);
    Serial.print(" -> ");
  }
  Serial.print(newVal);
  Serial.println();
}

// Helper function to print confirmation messages with numbers
static void printConfirmation(const char* prefix, int32_t oldVal, int32_t newVal) {
  Serial.print("# ");
  Serial.print(prefix);
  Serial.print(oldVal);
  Serial.print(" -> ");
  Serial.print(newVal);
  Serial.println();
}

// Main configuration menu display and interaction
void doSetupMenu(struct config_t *pConfigInfo) {
  bool showMenu = true;
  for (;;) {
    if (showMenu) {
      configPrint("\r\n");
      configPrint("== TICC Configuration ==\r\n");
      // A) Mode
      strcpy(sharedBuffer, "A - ");
      copyProgStrToBuffer(str_mode, sharedBuffer + 4, sizeof(sharedBuffer) - 4);
      strcat(sharedBuffer, "Timestamp");  // Default, will be overwritten
      switch (pConfigInfo->MODE) {
        case Timestamp: strcpy(sharedBuffer + strlen(sharedBuffer) - 9, "Timestamp"); break;
        case Binary:    strcpy(sharedBuffer + strlen(sharedBuffer) - 9, "Binary Timestamp"); break;
        case Period:    strcpy(sharedBuffer + strlen(sharedBuffer) - 9, "Period"); break;
        case Interval:  strcpy(sharedBuffer + strlen(sharedBuffer) - 9, "Interval A->B"); break;
        case timeLab:   strcpy(sharedBuffer + strlen(sharedBuffer) - 9, "TimeLab 3-ch"); break;
        case Debug:     strcpy(sharedBuffer + strlen(sharedBuffer) - 9, "Debug"); break;
        case Null:      strcpy(sharedBuffer + strlen(sharedBuffer) - 9, "Null"); break;
      }
      strcat(sharedBuffer, ")\r\n");
      configPrint(sharedBuffer);
      // B) Wrap digits
      copyProgStrToBuffer(str_wrap, sharedBuffer, sizeof(sharedBuffer));
      char numStr[12];
      sprintf(numStr, "%d", pConfigInfo->WRAP);
      strcat(sharedBuffer, numStr);
      if (pConfigInfo->WRAP <= 0) {
        strcat(sharedBuffer, " - no wrap)\r\n");
      } else if (pConfigInfo->WRAP <= 9) {
        uint32_t wrap_seconds = 1;
        for (int i = 0; i < pConfigInfo->WRAP; i++) wrap_seconds *= 10;
        strcat(sharedBuffer, " - wraps at ");
        sprintf(numStr, "%lu", (unsigned long)wrap_seconds);
        strcat(sharedBuffer, numStr);
        strcat(sharedBuffer, " seconds)\r\n");
      } else {
        strcat(sharedBuffer, " - wraps at 1e");
        sprintf(numStr, "%d", pConfigInfo->WRAP);
        strcat(sharedBuffer, numStr);
        strcat(sharedBuffer, " seconds)\r\n");
      }
      configPrint(sharedBuffer);
      // C) Output decimal places
      copyProgStrToBuffer(str_places, sharedBuffer, sizeof(sharedBuffer));
      sprintf(numStr, "%d", pConfigInfo->PLACES);
      strcat(sharedBuffer, numStr);
      strcat(sharedBuffer, ")\r\n");
      configPrint(sharedBuffer);
      
      // D) Trigger edges
      copyProgStrToBuffer(str_trigger, sharedBuffer, sizeof(sharedBuffer));
      size_t len = strlen(sharedBuffer);
      sharedBuffer[len] = pConfigInfo->START_EDGE[0];
      sharedBuffer[len+1] = '/';
      sharedBuffer[len+2] = pConfigInfo->START_EDGE[1];
      sharedBuffer[len+3] = '\0';
      strcat(sharedBuffer, ")\r\n");
      configPrint(sharedBuffer);
      
      // E) Sync mode
      copyProgStrToBuffer(str_sync, sharedBuffer, sizeof(sharedBuffer));
      len = strlen(sharedBuffer);
      sharedBuffer[len] = pConfigInfo->SYNC_MODE;
      sharedBuffer[len+1] = '\0';
      strcat(sharedBuffer, ")\r\n");
      configPrint(sharedBuffer);
      
      // F) Channel names
      copyProgStrToBuffer(str_names, sharedBuffer, sizeof(sharedBuffer));
      len = strlen(sharedBuffer);
      sharedBuffer[len] = pConfigInfo->NAME[0];
      sharedBuffer[len+1] = '/';
      sharedBuffer[len+2] = pConfigInfo->NAME[1];
      sharedBuffer[len+3] = '\0';
      strcat(sharedBuffer, ")\r\n");
      configPrint(sharedBuffer);
      
      // G) Poll char
      copyProgStrToBuffer(str_poll, sharedBuffer, sizeof(sharedBuffer));
      if (pConfigInfo->POLL_CHAR) {
        len = strlen(sharedBuffer);
        sharedBuffer[len] = pConfigInfo->POLL_CHAR;
        sharedBuffer[len+1] = '\0';
      } else {
        strcat(sharedBuffer, "none");
      }
      strcat(sharedBuffer, ")\r\n");
      configPrint(sharedBuffer);
      
      // H) Advanced settings
      copyProgStrToBuffer(str_advanced, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      
      // I) Serial baud rate
      copyProgStrToBuffer(str_baud, sharedBuffer, sizeof(sharedBuffer));
      sprintf(numStr, "%ld", (long)pConfigInfo->BAUD_RATE);
      strcat(sharedBuffer, numStr);
      strcat(sharedBuffer, ")\r\n");
      configPrint(sharedBuffer);
      configPrint("\r\n");
      strcpy(sharedBuffer, "M - Show this menu again\r\n");
      configPrint(sharedBuffer);
      strcpy(sharedBuffer, "J - Show startup info\r\n");
      configPrint(sharedBuffer);
      strcpy(sharedBuffer, "W - Write changes to EEPROM (persist across restarts)\r\n");
      configPrint(sharedBuffer);
      configPrint("\r\n");
      copyProgStrToBuffer(str_exit1, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_exit2, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_exit3, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_exit4, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      showMenu = false;
    }
    configPrint("> ");
    Serial.flush();
    size_t n = readLine(sharedBuffer, sizeof(sharedBuffer));
    char *line = trimInPlace(sharedBuffer);
    if (n == 0) {
      // Empty input - just continue to next iteration without showing menu again
      continue;
    }
    serialDrain();

    // Check if original input had semicolons (determines interactive mode)
    bool has_semicolons = (strchr(line, ';') != NULL);
    
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
        // Use interactive mode only if original input had no semicolons
        if (!processCommand(pConfigInfo, cmd_line, &showMenu, !has_semicolons)) {
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
