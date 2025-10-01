// config_menu.cpp -- Menu command processing functions

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

// Single shared buffer to reduce memory usage - replaces all local buffers
static char sharedBuffer[128];

// PROGMEM strings for submenus and messages
const char str_mode_menu[] PROGMEM = "-- Mode --";
const char str_mode1[] PROGMEM = "A1 - Timestamp";
const char str_mode2[] PROGMEM = "A2 - Binary Timestamp";
const char str_mode3[] PROGMEM = "A3 - Time Interval A -> B";
const char str_mode4[] PROGMEM = "A4 - Period";
const char str_mode5[] PROGMEM = "A5 - TimeLab 3-Cornered Hat";
const char str_mode6[] PROGMEM = "A6 - Debug";
const char str_mode7[] PROGMEM = "A7 - Null Output";
const char str_current_mode[] PROGMEM = "Current mode: ";
const char str_discard[] PROGMEM = "1 - Discard changes and return to main menu";
const char str_keep[] PROGMEM = "2 - Keep changes and return to main menu";
const char str_mode_changes_discarded[] PROGMEM = "Mode changes discarded.";
const char str_mode_changes_kept[] PROGMEM = "Mode changes kept.";
const char str_mode_was[] PROGMEM = "Mode was ";
const char str_mode_now[] PROGMEM = "; now ";

const char str_baud_menu[] PROGMEM = "-- Serial Baud Rate Settings --";
const char str_baud1[] PROGMEM = "I1 - 9600 bps";
const char str_baud2[] PROGMEM = "I2 - 19200 bps";
const char str_baud3[] PROGMEM = "I3 - 38400 bps";
const char str_baud4[] PROGMEM = "I4 - 57600 bps";
const char str_baud5[] PROGMEM = "I5 - 115200 bps (default)";
const char str_baud6[] PROGMEM = "I6 - 230400 bps";
const char str_baud_changes_discarded[] PROGMEM = "Baud rate changes discarded.";
const char str_baud_changes_kept[] PROGMEM = "Baud rate changes kept.";
const char str_baud_was[] PROGMEM = "Baud rate was ";
const char str_baud_now[] PROGMEM = "; now ";

// Advanced Settings submenu PROGMEM strings
const char str_advanced_menu[] PROGMEM = "-- Advanced Settings --";
const char str_h1_clock[] PROGMEM = "H1 - Clock Speed MHz (currently: ";
const char str_h2_coarse[] PROGMEM = "H2 - Coarse Tick us (currently: ";
const char str_h3_prop[] PROGMEM = "H3 - Propagation Delay ps A/B (currently: ";
const char str_h4_dilation[] PROGMEM = "H4 - Time Dilation A/B (currently: ";
const char str_h5_fixed[] PROGMEM = "H5 - fixedTime2 ps A/B (currently: ";
const char str_h6_fudge[] PROGMEM = "H6 - FUDGE0 ps A/B (currently: ";

// Confirmation message strings
const char str_save_eeprom[] PROGMEM = "Save to EEPROM with 'W' and restart for change to take effect.";
const char str_keep_changes[] PROGMEM = "1 - Keep changes";
const char str_discard_changes[] PROGMEM = "2 - Discard changes";

// Helper function to print PROGMEM strings
static void printProgStr(const char* str) {
  char c;
  while ((c = pgm_read_byte(str++)) != 0) {
    Serial.write(c);
  }
}

// Helper function to copy PROGMEM strings to buffer
static void copyProgStrToBuffer(const char* str, char* buffer, size_t bufferSize) {
  size_t i = 0;
  char c;
  while ((c = pgm_read_byte(str++)) != 0 && i < bufferSize - 1) {
    buffer[i++] = c;
  }
  buffer[i] = '\0';
}

// Helper function to handle confirmation flow with keep/discard options
// Returns true if changes should be kept, false if discarded
static bool handleConfirmation(const char* confirmationMsg, const char* itemSpecificMsg, 
                              bool preserveCase = false) {
  // Show confirmation message
  strcpy(sharedBuffer, confirmationMsg);
  strcat(sharedBuffer, "\r\n");
  configPrint(sharedBuffer);
  
  // Show item-specific message if provided
  if (itemSpecificMsg) {
    copyProgStrToBuffer(itemSpecificMsg, sharedBuffer, sizeof(sharedBuffer));
    strcat(sharedBuffer, "\r\n");
    configPrint(sharedBuffer);
  }
  
  configPrint("\r\n");
  
  // Show keep/discard options
  copyProgStrToBuffer(str_keep_changes, sharedBuffer, sizeof(sharedBuffer));
  strcat(sharedBuffer, "\r\n");
  configPrint(sharedBuffer);
  copyProgStrToBuffer(str_discard_changes, sharedBuffer, sizeof(sharedBuffer));
  strcat(sharedBuffer, "\r\n");
  configPrint(sharedBuffer);
  configPrint("> ");
  
  // Get user choice
  size_t n = readLine(sharedBuffer, sizeof(sharedBuffer));
  char *choice = trimInPlace(sharedBuffer);
  
  // Sanitize input: uppercase all alphas unless preserveCase is true
  if (!preserveCase) {
    for (size_t i = 0; i < n; i++) {
      if (isalpha(choice[i])) {
        choice[i] = toupper(choice[i]);
      }
    }
  }
  
  if (n && choice[0] == '2') {
    // Discard changes
    strcpy(sharedBuffer, "Changes discarded.\r\n");
    configPrint(sharedBuffer);
    return false;
  } else {
    // Keep changes (default)
    strcpy(sharedBuffer, "Changes kept.\r\n");
    configPrint(sharedBuffer);
    return true;
  }
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

// Helper function to print confirmation messages with characters
static void printConfirmation(const char* prefix, char oldVal, char newVal) {
  Serial.print("# ");
  Serial.print(prefix);
  Serial.write(oldVal);
  Serial.print(" -> ");
  Serial.write(newVal);
  Serial.println();
}

// Process a single command and return true if the command was processed successfully
bool processCommand(struct config_t *pConfigInfo, char *cmdLine, bool *showMenu) {
  char *line = trimInPlace(cmdLine);
  if (strlen(line) == 0) return true; // Empty command, continue
  
  char cmd = toupper(line[0]);
  // Check for direct parameters (no spaces required between command and parameters)
  char *args = line + 1;
  // Skip leading spaces if present, but also handle no-space case
  while (*args == ' ') args++;
  
  // Direct submenu commands (A1-A6, G1-G6)
  if (cmd == 'A' && strlen(line) >= 2 && isdigit(line[1])) {
    // Mode submenu commands
    char choice = line[1];
    MeasureMode old = pConfigInfo->MODE;
    if (choice == '1') pConfigInfo->MODE = Timestamp;
    else if (choice == '2') pConfigInfo->MODE = Binary;
    else if (choice == '3') pConfigInfo->MODE = Interval;
    else if (choice == '4') pConfigInfo->MODE = Period;
    else if (choice == '5') pConfigInfo->MODE = timeLab;
    else if (choice == '6') pConfigInfo->MODE = Debug;
    else if (choice == '7') pConfigInfo->MODE = Null;
    else {
      configPrint("Invalid mode choice\r\n");
      return true;
    }
    MARK_CONFIG_CHANGED();
    char msg[64]; 
    const char* modeName = "Unknown";
    switch (pConfigInfo->MODE) {
      case Timestamp: modeName = "Timestamp"; break;
      case Interval: modeName = "Interval"; break;
      case Period: modeName = "Period"; break;
      case timeLab: modeName = "TimeLab"; break;
      case Debug: modeName = "Debug"; break;
      case Binary: modeName = "Binary Timestamp"; break;
      case Null: modeName = "Null"; break;
    }
    sprintf(msg, "OK -- Mode set to %s\r\n", modeName); configPrint(msg);
    return true;
  }
  
  if (cmd == 'G' && strlen(line) >= 2 && isdigit(line[1])) {
    // Advanced submenu commands
    char choice = line[1];
    if (choice == '1') {
      // G1) Clock speed MHz
      char buf[96];
      char *input = getInputOrPrompt(args, "Clock MHz: ", buf, sizeof(buf));
      int64_t hz; 
      if (parseDecimalScaled(input, 1000000LL, &hz) && hz > 0) { 
        int64_t old = pConfigInfo->CLOCK_HZ; 
        pConfigInfo->CLOCK_HZ = hz; 
        MARK_CONFIG_CHANGED();
        char m[64]; 
        sprintf(m, "OK -- Clock %ld.%06ld -> %ld.%06ld\r\n", 
                (int32_t)(old/1000000LL), (int32_t)(old%1000000LL),
                (int32_t)(hz/1000000LL), (int32_t)(hz%1000000LL)); 
        configPrint(m); 
      } else {
        configPrint("Invalid\r\n");
      }
      Serial.flush();
    }
    else if (choice == '2') {
      // G2) Coarse tick us
      char buf[96];
      char *input = getInputOrPrompt(args, "Coarse Tick (us): ", buf, sizeof(buf));
      int64_t ps; 
      if (parseDecimalScaled(input, 1000000LL, &ps) && ps > 0) { 
        int64_t old = pConfigInfo->PICTICK_PS; 
        pConfigInfo->PICTICK_PS = ps; 
        MARK_CONFIG_CHANGED();
        char m[64]; 
        sprintf(m, "OK -- Coarse Tick %ld.%06ld -> %ld.%06ld\r\n", 
                (int32_t)(old/1000000LL), (int32_t)(old%1000000LL),
                (int32_t)(ps/1000000LL), (int32_t)(ps%1000000LL)); 
        configPrint(m); 
      } else {
        configPrint("Invalid\r\n");
      }
      Serial.flush();
    }
    else if (choice == '3') {
      // G3) Prop delays
      char buf[96];
      char *input = getInputOrPrompt(args, "Enter Pair A/B: ", buf, sizeof(buf));
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(input, &s0, &v0, &s1, &v1)) { 
        configPrint("Invalid\r\n"); 
      } else { 
        int32_t o0=pConfigInfo->PROP_DELAY[0], o1=pConfigInfo->PROP_DELAY[1]; 
        if (s0) pConfigInfo->PROP_DELAY[0]=v0; 
        if (s1) pConfigInfo->PROP_DELAY[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; 
        sprintf(m, "OK -- PropDelay %ld/%ld -> %ld/%ld\r\n", 
                (long)o0, (long)o1, (long)pConfigInfo->PROP_DELAY[0], (long)pConfigInfo->PROP_DELAY[1]); 
        configPrint(m); 
      }
      Serial.flush();
    }
    else if (choice == '4') {
      // G4) Time dilation
      char buf[96];
      char *input = getInputOrPrompt(args, "Enter Pair A/B: ", buf, sizeof(buf));
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(input, &s0, &v0, &s1, &v1)) { 
        configPrint("Invalid\r\n"); 
      } else { 
        int32_t o0=pConfigInfo->TIME_DILATION[0], o1=pConfigInfo->TIME_DILATION[1]; 
        if (s0) pConfigInfo->TIME_DILATION[0]=v0; 
        if (s1) pConfigInfo->TIME_DILATION[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; 
        sprintf(m, "OK -- Time Dilation %ld/%ld -> %ld/%ld\r\n", 
                (long)o0, (long)o1, (long)pConfigInfo->TIME_DILATION[0], (long)pConfigInfo->TIME_DILATION[1]); 
        configPrint(m); 
      }
      Serial.flush();
    }
    else if (choice == '5') {
      // G5) fixedTime2
      char buf[96];
      char *input = getInputOrPrompt(args, "Enter Pair A/B: ", buf, sizeof(buf));
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(input, &s0, &v0, &s1, &v1)) { 
        configPrint("Invalid\r\n"); 
      } else { 
        int32_t o0=pConfigInfo->FIXED_TIME2[0], o1=pConfigInfo->FIXED_TIME2[1]; 
        if (s0) pConfigInfo->FIXED_TIME2[0]=v0; 
        if (s1) pConfigInfo->FIXED_TIME2[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; 
        sprintf(m, "OK -- fixedTime2 %ld/%ld -> %ld/%ld\r\n", 
                (long)o0, (long)o1, (long)pConfigInfo->FIXED_TIME2[0], (long)pConfigInfo->FIXED_TIME2[1]); 
        configPrint(m); 
      }
      Serial.flush();
    }
    else if (choice == '6') {
      // G6) FUDGE0
      char buf[96];
      char *input = getInputOrPrompt(args, "Enter Pair A/B: ", buf, sizeof(buf));
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(input, &s0, &v0, &s1, &v1)) { 
        configPrint("Invalid\r\n"); 
      } else { 
        int32_t o0=pConfigInfo->FUDGE0[0], o1=pConfigInfo->FUDGE0[1]; 
        if (s0) pConfigInfo->FUDGE0[0]=v0; 
        if (s1) pConfigInfo->FUDGE0[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; 
        sprintf(m, "OK -- FUDGE0 %ld/%ld -> %ld/%ld\r\n", 
                (long)o0, (long)o1, (long)pConfigInfo->FUDGE0[0], (long)pConfigInfo->FUDGE0[1]); 
        configPrint(m); 
      }
      Serial.flush();
    }
    else {
      configPrint("Invalid Advanced Choice\r\n");
    }
    return true;
  }

  // Main menu commands
  if (cmd == 'A') {
    // Interactive Mode submenu
    for (;;) {
      configPrint("\r\n");
      copyProgStrToBuffer(str_mode_menu, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      
      copyProgStrToBuffer(str_mode1, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_mode2, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_mode3, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_mode4, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_mode5, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_mode6, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_mode7, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      
      configPrint("\r\n");
      copyProgStrToBuffer(str_current_mode, sharedBuffer, sizeof(sharedBuffer));
      switch (pConfigInfo->MODE) {
        case Timestamp: strcat(sharedBuffer, "Timestamp"); break;
        case Binary:    strcat(sharedBuffer, "Binary Timestamp"); break;
        case Period:    strcat(sharedBuffer, "Period"); break;
        case Interval:  strcat(sharedBuffer, "Time Interval A->B"); break;
        case timeLab:   strcat(sharedBuffer, "TimeLab 3-Cornered Hat"); break;
        case Debug:     strcat(sharedBuffer, "Debug"); break;
        case Null:      strcat(sharedBuffer, "Null Output"); break;
      }
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      
      configPrint("\r\n");
      copyProgStrToBuffer(str_discard, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_keep, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      configPrint("> ");
      size_t mn = readLine(sharedBuffer, sizeof(sharedBuffer)); 
      char *mline = trimInPlace(sharedBuffer);
      if (mn) {
        if (mline[0] == '1' || mline[0] == '2') {
          // Return options
          if (mline[0] == '1') {
            strcpy(sharedBuffer, "Mode changes discarded.\r\n");
            configPrint(sharedBuffer);
          } else {
            strcpy(sharedBuffer, "Mode changes kept.\r\n");
            configPrint(sharedBuffer);
          }
          *showMenu = true;
          break; // Exit the submenu loop
        } else {
          // Mode setting options
          char m = toupper(mline[0]); MeasureMode old = pConfigInfo->MODE;
          if (m == 'A' && mline[1] == '1') pConfigInfo->MODE = Timestamp;
          else if (m == 'A' && mline[1] == '2') pConfigInfo->MODE = Binary;
          else if (m == 'A' && mline[1] == '3') pConfigInfo->MODE = Interval;
          else if (m == 'A' && mline[1] == '4') pConfigInfo->MODE = Period;
          else if (m == 'A' && mline[1] == '5') pConfigInfo->MODE = timeLab;
          else if (m == 'A' && mline[1] == '6') pConfigInfo->MODE = Debug;
          else if (m == 'A' && mline[1] == '7') pConfigInfo->MODE = Null;
          
          // Show mode change confirmation and mark config as changed
          if (old != pConfigInfo->MODE) {
            strcpy(sharedBuffer, "Mode was ");
            switch (old) {
              case Timestamp: strcat(sharedBuffer, "Timestamp"); break;
              case Binary: strcat(sharedBuffer, "Binary Timestamp"); break;
              case Interval: strcat(sharedBuffer, "Time Interval A->B"); break;
              case Period: strcat(sharedBuffer, "Period"); break;
              case timeLab: strcat(sharedBuffer, "TimeLab 3-Cornered Hat"); break;
              case Debug: strcat(sharedBuffer, "Debug"); break;
              case Null: strcat(sharedBuffer, "Null Output"); break;
            }
            strcat(sharedBuffer, "; now ");
            switch (pConfigInfo->MODE) {
              case Timestamp: strcat(sharedBuffer, "Timestamp"); break;
              case Binary: strcat(sharedBuffer, "Binary Timestamp"); break;
              case Interval: strcat(sharedBuffer, "Time Interval A->B"); break;
              case Period: strcat(sharedBuffer, "Period"); break;
              case timeLab: strcat(sharedBuffer, "TimeLab 3-Cornered Hat"); break;
              case Debug: strcat(sharedBuffer, "Debug"); break;
              case Null: strcat(sharedBuffer, "Null Output"); break;
            }
            strcat(sharedBuffer, "\r\n");
            configPrint(sharedBuffer);
            MARK_CONFIG_CHANGED();
          }
        }
      }
    }
    return true;
  }

  if (cmd == 'M') { *showMenu = true; configPrint("\r\n"); return true; }

  // B) Wrap digits
  if (cmd == 'B') {
    char *input = getInputOrPrompt(args, "Wrap Digits (0..10): ", sharedBuffer, sizeof(sharedBuffer));
    int64_t wrap; 
    if (parseInt64Simple(input, &wrap) && wrap >= 0 && wrap <= 10) { 
      int16_t old = pConfigInfo->WRAP; 
      pConfigInfo->WRAP = (int16_t)wrap; 
      MARK_CONFIG_CHANGED();
      
      // Show confirmation and get user choice
      strcpy(sharedBuffer, "OK -- Wrap Digits ");
      sprintf(sharedBuffer + strlen(sharedBuffer), "%d", old);
      strcat(sharedBuffer, " -> ");
      sprintf(sharedBuffer + strlen(sharedBuffer), "%d", pConfigInfo->WRAP);
      
      if (!handleConfirmation(sharedBuffer, str_save_eeprom)) {
        // Discard changes
        pConfigInfo->WRAP = old;
        config_changed = 0; // Clear the changed flag
      }
    } else {
      configPrint("Invalid\r\n");
    }
    Serial.flush();
    return true;
  }
  
  // C) Output decimal places
  if (cmd == 'C') {
    char *input = getInputOrPrompt(args, "Output Decimal Places (0..12): ", sharedBuffer, sizeof(sharedBuffer));
    int64_t places; 
    if (parseInt64Simple(input, &places) && places >= 0 && places <= 12) { 
      int16_t old = pConfigInfo->PLACES; 
      pConfigInfo->PLACES = (int16_t)places; 
      MARK_CONFIG_CHANGED();
      
      // Show confirmation and get user choice
      strcpy(sharedBuffer, "OK -- Decimal Places ");
      sprintf(sharedBuffer + strlen(sharedBuffer), "%d", old);
      strcat(sharedBuffer, " -> ");
      sprintf(sharedBuffer + strlen(sharedBuffer), "%d", pConfigInfo->PLACES);
      
      if (!handleConfirmation(sharedBuffer, str_save_eeprom)) {
        // Discard changes
        pConfigInfo->PLACES = old;
        config_changed = 0; // Clear the changed flag
      }
    } else {
      configPrint("Invalid\r\n");
    }
    Serial.flush();
    return true;
  }
  
  // D) Trigger edges
  if (cmd == 'D') {
    char *input = getInputOrPrompt(args, "Enter Edges A/B (R/F): ", sharedBuffer, sizeof(sharedBuffer));
    if (input[0] && input[1] == '/' && input[2]) {
      char e0 = toupper(input[0]), e1 = toupper(input[2]);
      if ((e0 == 'R' || e0 == 'F') && (e1 == 'R' || e1 == 'F')) {
        char o0 = pConfigInfo->START_EDGE[0], o1 = pConfigInfo->START_EDGE[1];
        pConfigInfo->START_EDGE[0] = e0; 
        pConfigInfo->START_EDGE[1] = e1;
        MARK_CONFIG_CHANGED();
        
        // Show confirmation and get user choice
        strcpy(sharedBuffer, "OK -- Edges ");
        sharedBuffer[strlen(sharedBuffer)] = o0;
        sharedBuffer[strlen(sharedBuffer)+1] = '/';
        sharedBuffer[strlen(sharedBuffer)+2] = o1;
        sharedBuffer[strlen(sharedBuffer)+3] = '\0';
        strcat(sharedBuffer, " -> ");
        sharedBuffer[strlen(sharedBuffer)] = e0;
        sharedBuffer[strlen(sharedBuffer)+1] = '/';
        sharedBuffer[strlen(sharedBuffer)+2] = e1;
        sharedBuffer[strlen(sharedBuffer)+3] = '\0';
        
        if (!handleConfirmation(sharedBuffer, str_save_eeprom)) {
          // Discard changes
          pConfigInfo->START_EDGE[0] = o0;
          pConfigInfo->START_EDGE[1] = o1;
          config_changed = 0; // Clear the changed flag
        }
      } else {
        configPrint("Invalid\r\n");
      }
    } else {
      configPrint("Invalid\r\n");
    }
    Serial.flush();
    return true;
  }
  
  // E) Sync mode
  if (cmd == 'E') {
    char *input = getInputOrPrompt(args, "Enter P or S: ", sharedBuffer, sizeof(sharedBuffer));
    char c = toupper(input[0]); 
    if (c == 'P' || c == 'S') { 
      char old = pConfigInfo->SYNC_MODE; 
      pConfigInfo->SYNC_MODE = c; 
      MARK_CONFIG_CHANGED();
      
      // Show confirmation and get user choice
      strcpy(sharedBuffer, "OK -- Sync Mode ");
      sharedBuffer[strlen(sharedBuffer)] = old;
      sharedBuffer[strlen(sharedBuffer)+1] = '\0';
      strcat(sharedBuffer, " -> ");
      sharedBuffer[strlen(sharedBuffer)] = c;
      sharedBuffer[strlen(sharedBuffer)+1] = '\0';
      
      if (!handleConfirmation(sharedBuffer, str_save_eeprom)) {
        // Discard changes
        pConfigInfo->SYNC_MODE = old;
        config_changed = 0; // Clear the changed flag
      }
    } else {
      configPrint("Invalid\r\n");
    }
    Serial.flush();
    return true;
  }
  
  // F) Channel names (preserve case - no uppercasing)
  if (cmd == 'F') {
    char *input = getInputOrPrompt(args, "Enter Names A/B: ", sharedBuffer, sizeof(sharedBuffer));
    if (input[0] && input[1] == '/' && input[2]) {
      char o0 = pConfigInfo->NAME[0], o1 = pConfigInfo->NAME[1]; 
      pConfigInfo->NAME[0] = input[0];  // No toupper() - preserve case
      pConfigInfo->NAME[1] = input[2];  // No toupper() - preserve case
      MARK_CONFIG_CHANGED();
      
      // Show confirmation and get user choice
      strcpy(sharedBuffer, "OK -- Names ");
      sharedBuffer[strlen(sharedBuffer)] = o0;
      sharedBuffer[strlen(sharedBuffer)+1] = '/';
      sharedBuffer[strlen(sharedBuffer)+2] = o1;
      sharedBuffer[strlen(sharedBuffer)+3] = '\0';
      strcat(sharedBuffer, " -> ");
      sharedBuffer[strlen(sharedBuffer)] = input[0];
      sharedBuffer[strlen(sharedBuffer)+1] = '/';
      sharedBuffer[strlen(sharedBuffer)+2] = input[2];
      sharedBuffer[strlen(sharedBuffer)+3] = '\0';
      
      if (!handleConfirmation(sharedBuffer, str_save_eeprom, true)) { // preserveCase = true
        // Discard changes
        pConfigInfo->NAME[0] = o0;
        pConfigInfo->NAME[1] = o1;
        config_changed = 0; // Clear the changed flag
      }
    } else {
      configPrint("Invalid\r\n");
    }
    Serial.flush();
    return true;
  }

  // G) Poll char
  if (cmd == 'G') {
    char *input = getInputOrPrompt(args, "Enter Poll Character (space to clear): ", sharedBuffer, sizeof(sharedBuffer));
    char old = pConfigInfo->POLL_CHAR;
    pConfigInfo->POLL_CHAR = (input[0] == '\0' || input[0] == ' ') ? 0x00 : input[0];
    MARK_CONFIG_CHANGED();
    
    // Show confirmation and get user choice
    strcpy(sharedBuffer, "OK -- Poll Character ");
    if (old) {
      sharedBuffer[strlen(sharedBuffer)] = old;
      sharedBuffer[strlen(sharedBuffer)+1] = '\0';
    } else {
      strcat(sharedBuffer, "none");
    }
    strcat(sharedBuffer, " -> ");
    if (pConfigInfo->POLL_CHAR) {
      sharedBuffer[strlen(sharedBuffer)] = pConfigInfo->POLL_CHAR;
      sharedBuffer[strlen(sharedBuffer)+1] = '\0';
    } else {
      strcat(sharedBuffer, "none");
    }
    
    if (!handleConfirmation(sharedBuffer, str_save_eeprom)) {
      // Discard changes
      pConfigInfo->POLL_CHAR = old;
      config_changed = 0; // Clear the changed flag
    }
    Serial.flush();
    return true;
  }

  // I) Serial baud rate
  if (cmd == 'I') {
    // Interactive baud rate submenu
    for (;;) {
      configPrint("\r\n");
      copyProgStrToBuffer(str_baud_menu, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      
      copyProgStrToBuffer(str_baud1, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_baud2, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_baud3, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_baud4, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_baud5, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_baud6, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      
      configPrint("\r\n");
      copyProgStrToBuffer(str_discard, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_keep, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      configPrint("> ");
      size_t mn = readLine(sharedBuffer, sizeof(sharedBuffer)); 
      char *mline = trimInPlace(sharedBuffer);
      if (mn) {
        if (mline[0] == '1' || mline[0] == '2') {
          // Return options
          if (mline[0] == '1') {
            strcpy(sharedBuffer, "Baud rate changes discarded.\r\n");
            configPrint(sharedBuffer);
          } else {
            strcpy(sharedBuffer, "Baud rate changes kept.\r\n");
            configPrint(sharedBuffer);
          }
          *showMenu = true;
          break; // Exit the submenu loop
        } else {
          // Baud rate setting options
          if (strlen(mline) >= 2 && mline[0] == 'I' && mline[1] >= '1' && mline[1] <= '6') {
            uint32_t old_rate = pConfigInfo->BAUD_RATE;
            uint32_t new_rate;
            switch (mline[1]) {
              case '1': new_rate = 9600; break;
              case '2': new_rate = 19200; break;
              case '3': new_rate = 38400; break;
              case '4': new_rate = 57600; break;
              case '5': new_rate = 115200; break;
              case '6': new_rate = 230400; break;
              default: new_rate = old_rate; break;
            }
            
            // Show baud rate change confirmation and mark config as changed
            if (old_rate != new_rate) {
              pConfigInfo->BAUD_RATE = new_rate;
              strcpy(sharedBuffer, "Baud rate was ");
              sprintf(sharedBuffer + strlen(sharedBuffer), "%ld", (long)old_rate);
              strcat(sharedBuffer, "; now ");
              sprintf(sharedBuffer + strlen(sharedBuffer), "%ld", (long)new_rate);
              strcat(sharedBuffer, "\r\n");
              configPrint(sharedBuffer);
              MARK_CONFIG_CHANGED();
            }
          } else {
            configPrint("Invalid selection\r\n");
          }
        }
      }
    }
    return true;
  }

  // J) Show startup info
  if (cmd == 'J') {
    configPrint("\r\n");
    print_config(*pConfigInfo);
    configPrint("\r\n");
    return true;
  }

  // W) Write changes to EEPROM (without restart)
  if (cmd == 'W') {
    eeprom_write_config();
    config_changed = 0; // Clear the changed flag since changes are now persistent
    configPrint("Changes written to EEPROM (will persist across restarts)\r\n");
    return true;
  }

  // H) Advanced submenu
  if (cmd == 'H') {
    // Interactive Advanced submenu
    for (;;) {
      configPrint("\r\n");
      copyProgStrToBuffer(str_advanced_menu, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      
      // H1 - Clock Speed MHz
      {
        copyProgStrToBuffer(str_h1_clock, sharedBuffer, sizeof(sharedBuffer));
        int64_t MHz = pConfigInfo->CLOCK_HZ / 1000000LL;
        int64_t Hz = MHz * 1000000LL;
        int64_t fract = pConfigInfo->CLOCK_HZ - Hz;
        sprintf(sharedBuffer + strlen(sharedBuffer), "%ld.%06ld", (int32_t)MHz, (int32_t)fract);
        strcat(sharedBuffer, ")\r\n");
        configPrint(sharedBuffer);
      }
      
      // H2 - Coarse Tick us
      {
        copyProgStrToBuffer(str_h2_coarse, sharedBuffer, sizeof(sharedBuffer));
        int64_t us = pConfigInfo->PICTICK_PS / 1000000LL;
        int64_t ps = us * 1000000LL;
        int64_t fract = pConfigInfo->PICTICK_PS - ps;
        sprintf(sharedBuffer + strlen(sharedBuffer), "%ld.%06ld", (int32_t)us, (int32_t)fract);
        strcat(sharedBuffer, ")\r\n");
        configPrint(sharedBuffer);
      }
      
      // H3 - Propagation Delay ps A/B
      {
        copyProgStrToBuffer(str_h3_prop, sharedBuffer, sizeof(sharedBuffer));
        sprintf(sharedBuffer + strlen(sharedBuffer), "%ld/%ld", (long)pConfigInfo->PROP_DELAY[0], (long)pConfigInfo->PROP_DELAY[1]);
        strcat(sharedBuffer, ")\r\n");
        configPrint(sharedBuffer);
      }
      
      // H4 - Time Dilation A/B
      {
        copyProgStrToBuffer(str_h4_dilation, sharedBuffer, sizeof(sharedBuffer));
        sprintf(sharedBuffer + strlen(sharedBuffer), "%ld/%ld", (long)pConfigInfo->TIME_DILATION[0], (long)pConfigInfo->TIME_DILATION[1]);
        strcat(sharedBuffer, ")\r\n");
        configPrint(sharedBuffer);
      }
      
      // H5 - fixedTime2 ps A/B
      {
        copyProgStrToBuffer(str_h5_fixed, sharedBuffer, sizeof(sharedBuffer));
        sprintf(sharedBuffer + strlen(sharedBuffer), "%ld/%ld", (long)pConfigInfo->FIXED_TIME2[0], (long)pConfigInfo->FIXED_TIME2[1]);
        strcat(sharedBuffer, ")\r\n");
        configPrint(sharedBuffer);
      }
      
      // H6 - FUDGE0 ps A/B
      {
        copyProgStrToBuffer(str_h6_fudge, sharedBuffer, sizeof(sharedBuffer));
        sprintf(sharedBuffer + strlen(sharedBuffer), "%ld/%ld", (long)pConfigInfo->FUDGE0[0], (long)pConfigInfo->FUDGE0[1]);
        strcat(sharedBuffer, ")\r\n");
        configPrint(sharedBuffer);
      }
      
      copyProgStrToBuffer(str_discard, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      copyProgStrToBuffer(str_keep, sharedBuffer, sizeof(sharedBuffer));
      strcat(sharedBuffer, "\r\n");
      configPrint(sharedBuffer);
      configPrint("> ");
      char buf[96];
      size_t an = readLine(buf, sizeof(buf)); char *aline = trimInPlace(buf);
      if (an) {
        if (aline[0] == '1' || aline[0] == '2') {
          // Return options - no changes needed
          if (aline[0] == '1') {
            configPrint("Changes discarded.\r\n");
          } else {
            configPrint("Changes kept.\r\n");
          }
          *showMenu = true;
          break; // Exit the submenu loop
        } else {
          // Advanced setting options
          char a = toupper(aline[0]);
          const char *aargs = aline + 1; while (*aargs == ' ') aargs++;
          
          // H1) Clock speed MHz
          if (a == 'H' && aline[1] == '1') {
            configPrint("Clock MHz: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            int64_t hz; if (parseDecimalScaled(cline, 1000000LL, &hz) && hz > 0) { int64_t old=pConfigInfo->CLOCK_HZ; pConfigInfo->CLOCK_HZ = hz; char m[64]; sprintf(m, "OK -- Clock %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(hz/1000000LL),(int32_t)(hz%1000000LL)); configPrint(m); } else configPrint("Invalid\r\n");
            Serial.flush();
          }
          // H2) Coarse tick us
          else if (a == 'H' && aline[1] == '2') {
            configPrint("Coarse Tick (us): "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            int64_t ps; if (parseDecimalScaled(cline, 1000000LL, &ps) && ps > 0) { int64_t old=pConfigInfo->PICTICK_PS; pConfigInfo->PICTICK_PS = ps; char m[64]; sprintf(m, "OK -- Coarse %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(ps/1000000LL),(int32_t)(ps%1000000LL)); configPrint(m); } else configPrint("Invalid\r\n");
            Serial.flush();
          }
          // H3) Prop delays
          else if (a == 'H' && aline[1] == '3') {
            configPrint("Enter Pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { configPrint("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->PROP_DELAY[0], o1=pConfigInfo->PROP_DELAY[1]; if (s0) pConfigInfo->PROP_DELAY[0]=v0; if (s1) pConfigInfo->PROP_DELAY[1]=v1; char m[80]; sprintf(m, "OK -- PropDelay %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->PROP_DELAY[0],(long)pConfigInfo->PROP_DELAY[1]); configPrint(m); Serial.flush(); }
          }
          // H4) Time dilation
          else if (a == 'H' && aline[1] == '4') {
            configPrint("Enter Pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { configPrint("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->TIME_DILATION[0], o1=pConfigInfo->TIME_DILATION[1]; if (s0) pConfigInfo->TIME_DILATION[0]=v0; if (s1) pConfigInfo->TIME_DILATION[1]=v1; char m[80]; sprintf(m, "OK -- TimeDilation %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->TIME_DILATION[0],(long)pConfigInfo->TIME_DILATION[1]); configPrint(m); Serial.flush(); }
          }
          // H5) fixedTime2
          else if (a == 'H' && aline[1] == '5') {
            configPrint("Enter Pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { configPrint("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->FIXED_TIME2[0], o1=pConfigInfo->FIXED_TIME2[1]; if (s0) pConfigInfo->FIXED_TIME2[0]=v0; if (s1) pConfigInfo->FIXED_TIME2[1]=v1; char m[80]; sprintf(m, "OK -- fixedTime2 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FIXED_TIME2[0],(long)pConfigInfo->FIXED_TIME2[1]); configPrint(m); Serial.flush(); }
          }
          // H6) FUDGE0
          else if (a == 'H' && aline[1] == '6') {
            configPrint("Enter Pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { configPrint("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->FUDGE0[0], o1=pConfigInfo->FUDGE0[1]; if (s0) pConfigInfo->FUDGE0[0]=v0; if (s1) pConfigInfo->FUDGE0[1]=v1; char m[80]; sprintf(m, "OK -- FUDGE0 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FUDGE0[0],(long)pConfigInfo->FUDGE0[1]); configPrint(m); Serial.flush(); }
          }
          else { configPrint("Invalid\r\n"); Serial.flush(); }
        }
      }
    }
    return true;
  }

  // X) Developer EEPROM clear (undocumented)
  if (cmd == 'X') {
    configPrint("WARNING: This will completely erase the entire EEPROM including serial number!\r\n");
    configPrint("Type 'YES' to confirm: ");
    char buf[96];
    size_t n = readLine(buf, sizeof(buf));
    char *input = trimInPlace(buf);
    if (strcmp(input, "YES") == 0) {
      configPrint("Clearing entire EEPROM...\r\n");
      eeprom_clear();
      configPrint("EEPROM cleared. Restarting...\r\n");
      // Force a restart by setting a flag that will cause main loop to exit
      extern volatile uint8_t request_restart;
      request_restart = 1;
      return false;
    } else {
      configPrint("EEPROM clear cancelled.\r\n");
    }
    return true;
  }

  // Numbered exits
  if (cmd == '1') { 
    configPrint("Discarded changes.\r\n"); 
    config_changed = 0; // Clear the changed flag since we're discarding
    return false; 
  }
  if (cmd == '2') { 
    // Apply changes and restart
    configPrint("Applying changes and restarting...\r\n"); 
    // Force a restart by setting a flag that will cause main loop to exit
    extern volatile uint8_t request_restart;
    request_restart = 1;
    return false; 
  }
  if (cmd == '3') { 
    // Apply changes and resume operation
    configPrint("Applying changes and resuming operation...\r\n"); 
    // Clear the changed flag since we're applying changes
    config_changed = 0;
    return false; 
  }
  if (cmd == '4') { 
    // Reset all to defaults and restart
    eeprom_write_config_default(CONFIG_START); 
    configPrint("Defaults written. Restarting...\r\n"); 
    // Force a restart by setting a flag that will cause main loop to exit
    extern volatile uint8_t request_restart;
    request_restart = 1;
    return false; 
  }

  configPrint("? Unknown command\r\n");
  return true;
}
