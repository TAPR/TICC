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
      configPrint("-- Mode --\r\n");
      configPrint("A1 - Timestamp\r\n");
      configPrint("A2 - Binary Timestamp\r\n");
      configPrint("A3 - Time Interval A -> B\r\n");
      configPrint("A4 - Period\r\n");
      configPrint("A5 - TimeLab 3-Cornered Hat\r\n");
      configPrint("A6 - Debug\r\n");
      configPrint("A7 - Null Output\r\n");
      configPrint("\r\n");
      configPrint("Current mode: ");
      
      switch (pConfigInfo->MODE) {
        case Timestamp: serialPrintImmediate("Timestamp"); break;
        case Binary:    serialPrintImmediate("Binary Timestamp"); break;
        case Period:    serialPrintImmediate("Period"); break;
        case Interval:  serialPrintImmediate("Time Interval A->B"); break;
        case timeLab:   serialPrintImmediate("TimeLab 3-Cornered Hat"); break;
        case Debug:     serialPrintImmediate("Debug"); break;
        case Null:      serialPrintImmediate("Null Output"); break;
      }
      serialPrintImmediate("\r\n");
      configPrint("\r\n");
      configPrint("1 - Discard changes and return to main menu\r\n");
      configPrint("2 - Keep changes and return to main menu\r\n");
      configPrint("> ");
      char buf[96];
      size_t mn = readLine(buf, sizeof(buf)); char *mline = trimInPlace(buf);
      if (mn) {
        if (mline[0] == '1' || mline[0] == '2') {
          // Return options
          if (mline[0] == '1') {
            configPrint("Mode changes discarded.\r\n");
          } else {
            configPrint("Mode changes kept.\r\n");
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
            char msg[128];
            sprintf(msg, "Mode was %s; now %s\r\n", 
                    (old == Timestamp) ? "Timestamp" :
                    (old == Binary) ? "Binary Timestamp" :
                    (old == Interval) ? "Time Interval A->B" :
                    (old == Period) ? "Period" :
                    (old == timeLab) ? "TimeLab 3-Cornered Hat" :
                    (old == Debug) ? "Debug" : "Null Output",
                    (pConfigInfo->MODE == Timestamp) ? "Timestamp" :
                    (pConfigInfo->MODE == Binary) ? "Binary Timestamp" :
                    (pConfigInfo->MODE == Interval) ? "Time Interval A->B" :
                    (pConfigInfo->MODE == Period) ? "Period" :
                    (pConfigInfo->MODE == timeLab) ? "TimeLab 3-Cornered Hat" :
                    (pConfigInfo->MODE == Debug) ? "Debug" : "Null Output");
            serialPrintImmediate(msg);
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
    char buf[96];
    char *input = getInputOrPrompt(args, "Wrap Digits (0..10): ", buf, sizeof(buf));
    int64_t wrap; 
    if (parseInt64Simple(input, &wrap) && wrap >= 0 && wrap <= 10) { 
      int16_t old = pConfigInfo->WRAP; 
      pConfigInfo->WRAP = (int16_t)wrap; 
      MARK_CONFIG_CHANGED();
      char m[64]; 
      sprintf(m, "OK -- Wrap Digits %d -> %d\r\n", (int)old, (int)pConfigInfo->WRAP); 
      configPrint(m); 
    } else {
      configPrint("Invalid\r\n");
    }
    Serial.flush();
    return true;
  }
  
  // C) Output decimal places
  if (cmd == 'C') {
    char buf[96];
    char *input = getInputOrPrompt(args, "Output Decimal Places (0..12): ", buf, sizeof(buf));
    int64_t places; 
    if (parseInt64Simple(input, &places) && places >= 0 && places <= 12) { 
      int16_t old = pConfigInfo->PLACES; 
      pConfigInfo->PLACES = (int16_t)places; 
      MARK_CONFIG_CHANGED();
      char m[64]; 
      sprintf(m, "OK -- Decimal Places %d -> %d\r\n", (int)old, (int)pConfigInfo->PLACES); 
      configPrint(m); 
    } else {
      configPrint("Invalid\r\n");
    }
    Serial.flush();
    return true;
  }
  
  // D) Trigger edges
  if (cmd == 'D') {
    char buf[96];
    char *input = getInputOrPrompt(args, "Enter Edges A/B (R/F): ", buf, sizeof(buf));
    if (input[0] && input[1] == '/' && input[2]) {
      char e0 = toupper(input[0]), e1 = toupper(input[2]);
      if ((e0 == 'R' || e0 == 'F') && (e1 == 'R' || e1 == 'F')) {
        char o0 = pConfigInfo->START_EDGE[0], o1 = pConfigInfo->START_EDGE[1];
        pConfigInfo->START_EDGE[0] = e0; 
        pConfigInfo->START_EDGE[1] = e1;
        MARK_CONFIG_CHANGED();
        char m[64]; 
        sprintf(m, "OK -- Edges %c/%c -> %c/%c\r\n", o0, o1, e0, e1); 
        configPrint(m);
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
    char buf[96];
    char *input = getInputOrPrompt(args, "Enter P or S: ", buf, sizeof(buf));
    char c = toupper(input[0]); 
    if (c == 'P' || c == 'S') { 
      char old = pConfigInfo->SYNC_MODE; 
      pConfigInfo->SYNC_MODE = c; 
      MARK_CONFIG_CHANGED();
      char m[64]; 
      sprintf(m, "OK -- Sync Mode %c -> %c\r\n", old, c); 
      configPrint(m); 
    } else {
      configPrint("Invalid\r\n");
    }
    Serial.flush();
    return true;
  }
  
  // F) Channel names (preserve case - no uppercasing)
  if (cmd == 'F') {
    char buf[96];
    char *input = getInputOrPrompt(args, "Enter Names A/B: ", buf, sizeof(buf));
    if (input[0] && input[1] == '/' && input[2]) {
      char o0 = pConfigInfo->NAME[0], o1 = pConfigInfo->NAME[1]; 
      pConfigInfo->NAME[0] = input[0];  // No toupper() - preserve case
      pConfigInfo->NAME[1] = input[2];  // No toupper() - preserve case
      MARK_CONFIG_CHANGED();
      char m[64]; 
      sprintf(m, "OK -- Names %c/%c -> %c/%c\r\n", o0, o1, input[0], input[2]); 
      configPrint(m);
    } else {
      configPrint("Invalid\r\n");
    }
    Serial.flush();
    return true;
  }

  // G) Poll char
  if (cmd == 'G') {
    char buf[96];
    char *input = getInputOrPrompt(args, "Enter Poll Character (space to clear): ", buf, sizeof(buf));
    char old = pConfigInfo->POLL_CHAR;
    pConfigInfo->POLL_CHAR = (input[0] == '\0' || input[0] == ' ') ? 0x00 : input[0];
    MARK_CONFIG_CHANGED();
    char msg[64]; 
    if (old) {
      sprintf(msg, "OK -- Poll Character %c -> %c\r\n", old, pConfigInfo->POLL_CHAR ? pConfigInfo->POLL_CHAR : ' '); 
    } else {
      sprintf(msg, "OK -- Poll Character none -> %c\r\n", pConfigInfo->POLL_CHAR ? pConfigInfo->POLL_CHAR : ' '); 
    }
    configPrint(msg);
    Serial.flush();
    return true;
  }

  // I) Show startup info
  if (cmd == 'I') {
    configPrint("\r\n");
    print_config(*pConfigInfo);
    configPrint("\r\n");
    return true;
  }

  // W) Write changes to EEPROM (without restart)
  if (cmd == 'W') {
    eeprom_write_config();
    configPrint("Changes written to EEPROM (will persist across restarts)\r\n");
    return true;
  }

  // H) Advanced submenu
  if (cmd == 'H') {
    // Interactive Advanced submenu
    for (;;) {
      configPrint("\r\n");
      configPrint("-- Advanced Settings --\r\n");
      
      // H1 - Clock Speed MHz
      {
        char tmp[64]; 
        int64_t MHz = pConfigInfo->CLOCK_HZ / 1000000LL;
        int64_t Hz = MHz * 1000000LL;
        int64_t fract = pConfigInfo->CLOCK_HZ - Hz;
        sprintf(tmp, "H1 - Clock Speed MHz (currently: %ld.%06ld)\r\n", (int32_t)MHz, (int32_t)fract);
        configPrint(tmp);
      }
      
      // H2 - Coarse Tick us
      {
        char tmp[64]; 
        int64_t us = pConfigInfo->PICTICK_PS / 1000000LL;
        int64_t ps = us * 1000000LL;
        int64_t fract = pConfigInfo->PICTICK_PS - ps;
        sprintf(tmp, "H2 - Coarse Tick us (currently: %ld.%06ld)\r\n", (int32_t)us, (int32_t)fract);
        configPrint(tmp);
      }
      
      // H3 - Propagation Delay ps A/B
      {
        char tmp[64]; 
        sprintf(tmp, "H3 - Propagation Delay ps A/B (currently: %ld/%ld)\r\n", (long)pConfigInfo->PROP_DELAY[0], (long)pConfigInfo->PROP_DELAY[1]);
        configPrint(tmp);
      }
      
      // H4 - Time Dilation A/B
      {
        char tmp[64]; 
        sprintf(tmp, "H4 - Time Dilation A/B (currently: %ld/%ld)\r\n", (long)pConfigInfo->TIME_DILATION[0], (long)pConfigInfo->TIME_DILATION[1]);
        configPrint(tmp);
      }
      
      // H5 - fixedTime2 ps A/B
      {
        char tmp[64]; 
        sprintf(tmp, "H5 - fixedTime2 ps A/B (currently: %ld/%ld)\r\n", (long)pConfigInfo->FIXED_TIME2[0], (long)pConfigInfo->FIXED_TIME2[1]);
        configPrint(tmp);
      }
      
      // H6 - FUDGE0 ps A/B
      {
        char tmp[64]; 
        sprintf(tmp, "H6 - FUDGE0 ps A/B (currently: %ld/%ld)\r\n", (long)pConfigInfo->FUDGE0[0], (long)pConfigInfo->FUDGE0[1]);
        configPrint(tmp);
      }
      
      configPrint("1 - Discard changes and return to main menu\r\n");
      configPrint("2 - Keep changes and return to main menu\r\n");
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
