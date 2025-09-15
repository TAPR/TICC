// config.cpp -- set/read/write configuration

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2020
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Portions Copyright David McQuate WA8YWQ 2016
// Licensed under BSD 2-clause license

#include <stdint.h>           // define unint16_t, uint32_t
#include <ctype.h>
#include <EEPROM.h>           // read/write EEPROM
#include <SPI.h>

#include "misc.h"             // random functions
#include "config.h"           // config and eeprom
#include "board.h"            // Arduino pin definitions
#include "tdc7200.h"          // TDC registers and structures

extern const char SW_VERSION[17]; // set in TICC.ino
extern const char SW_TAG[6];      // set in TICC.ino
char SER_NUM[17];          // set by get_ser_num();

// External variables for config change tracking
extern uint8_t config_changed;

// Macro to mark config as changed
#define MARK_CONFIG_CHANGED() do { config_changed = 1; } while(0)

// --- New robust serial helpers ---
static void serialWriteRaw(const char *s, size_t len) {
  while (len) {
    // Wait until there is buffer space to avoid internal buffering delaying output
    while (Serial.availableForWrite() == 0) { /* yield */ }
    size_t n = Serial.write((const uint8_t*)s, len);
    s += n; len -= n;
  }
  Serial.flush();
}

static void serialPrintImmediate(const char *s) {
  Serial.print(s);
  Serial.flush();
}

// Helper function to print config output with "# " prefix for data file compatibility
static void configPrint(const char* msg) {
  serialPrintImmediate("# ");
  serialPrintImmediate(msg);
}

// Macro to replace serialPrintImmediate with configPrint for config output
#define CONFIG_PRINT(msg) configPrint(msg)

static void serialWriteImmediate(char c) {
  Serial.write(c);
  Serial.flush();
}

static void serialDrain() {
  Serial.flush();
  delay(5);
}

// Read a line into buf (cap includes terminator). Returns length (excludes terminator).
// Handles echo, backspace, CR/LF termination. Produces a NUL-terminated string without CR/LF.
static size_t readLine(char *buf, size_t cap) {
  if (cap == 0) return 0;
  size_t n = 0;
  for (;;) {
    while (!Serial.available()) { delay(1); }
    int ch = Serial.read();
    if (ch == '\r' || ch == '\n') {
      // Only echo newline if there was actual input
      if (n > 0) {
        serialWriteImmediate('\r'); serialWriteImmediate('\n');
      }
      buf[n] = '\0';
      return n;
    }
    if (ch == 0x08 || ch == 0x7F) { // backspace/delete
      if (n > 0) { n--; serialWriteImmediate('\b'); serialWriteImmediate(' '); serialWriteImmediate('\b'); }
      continue;
    }
    if (n + 1 < cap) {
      buf[n++] = (char)ch;
      serialWriteImmediate((char)ch);
    }
  }
}

// Trim leading/trailing spaces in place; returns start pointer inside buf.
static char* trimInPlace(char *s) {
  while (*s == ' ' || *s == '\t') s++;
  char *end = s + strlen(s);
  while (end > s && (end[-1] == ' ' || end[-1] == '\t')) { --end; }
  *end = '\0';
  return s;
}


// Simple int64 parser: accepts optional +/-, digits only; returns true on success
static bool parseInt64Simple(const char *s, int64_t *out) {
  if (!s || !*s) return false;
  bool neg = false; if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
  if (!*s) return false;
  int64_t v = 0;
  while (*s) {
    if (*s < '0' || *s > '9') return false;
    int d = *s - '0';
    v = v * 10 + d;
    s++;
  }
  *out = neg ? -v : v;
  return true;
}

// Parse decimal like 10.5 into integer scaled by scale (e.g., 1e6). Returns true on success.
static bool parseDecimalScaled(const char *s, int64_t scale, int64_t *out) {
  if (!s || !*s) return false;
  bool neg = false; if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
  if (!*s) return false;
  int64_t intPart = 0;
  while (*s && *s != '.') {
    if (*s < '0' || *s > '9') return false;
    intPart = intPart * 10 + (*s - '0');
    s++;
  }
  int64_t fracPart = 0; int64_t fracScale = 1;
  if (*s == '.') {
    s++;
    while (*s && fracScale < scale) {
      if (*s < '0' || *s > '9') return false;
      fracPart = fracPart * 10 + (*s - '0');
      fracScale *= 10;
      s++;
    }
    while (*s) { if (*s < '0' || *s > '9') return false; s++; }
  }
  // scale fractional to match target scale
  while (fracScale < scale) { fracPart *= 10; fracScale *= 10; }
  int64_t total = intPart * scale + fracPart;
  *out = neg ? -total : total;
  return true;
}

// Parse pair syntax "A/B" where either side may be empty.
// Returns which sides are set, and their parsed int64 values.
static bool parseInt64Pair(const char *s, bool *set0, int64_t *v0, bool *set1, int64_t *v1) {
  if (!s) return false;
  const char *slash = strchr(s, '/');
  char tmp[64];
  if (!slash) { // single value => apply to both
    size_t len = strlcpy(tmp, s, sizeof(tmp)); (void)len;
    char *t = trimInPlace(tmp);
    int64_t v; if (!parseInt64Simple(t, &v)) return false;
    *set0 = *set1 = true; *v0 = *v1 = v; return true;
  }
  bool ok;
  if (slash != s) {
    size_t l = (size_t)(slash - s); if (l >= sizeof(tmp)) l = sizeof(tmp) - 1;
    memcpy(tmp, s, l); tmp[l] = '\0';
    char *t = trimInPlace(tmp);
    ok = parseInt64Simple(t, v0); if (!ok) return false; *set0 = true;
  } else { *set0 = false; }
  if (*(slash+1)) {
    size_t l = strlcpy(tmp, slash+1, sizeof(tmp)); (void)l;
    char *t = trimInPlace(tmp);
    ok = parseInt64Simple(t, v1); if (!ok) return false; *set1 = true;
  } else { *set1 = false; }
  return true;
}

// Specialized pair parser for decimal scaled values (e.g., us->ps or MHz->Hz)
static bool parseDecimalScaledPair(const char *s, int64_t scale, bool *set0, int64_t *v0, bool *set1, int64_t *v1) {
  const char *slash = strchr(s, '/');
  char tmp[64];
  if (!slash) {
    size_t l = strlcpy(tmp, s, sizeof(tmp)); (void)l;
    char *t = trimInPlace(tmp);
    int64_t v; if (!parseDecimalScaled(t, scale, &v)) return false; *set0 = *set1 = true; *v0 = *v1 = v; return true;
  }
  bool ok;
  if (slash != s) {
    size_t l = (size_t)(slash - s); if (l >= sizeof(tmp)) l = sizeof(tmp) - 1;
    memcpy(tmp, s, l); tmp[l] = '\0';
    char *t = trimInPlace(tmp);
    ok = parseDecimalScaled(t, scale, v0); if (!ok) return false; *set0 = true;
  } else { *set0 = false; }
  if (*(slash+1)) {
    size_t l = strlcpy(tmp, slash+1, sizeof(tmp)); (void)l;
    char *t = trimInPlace(tmp);
    ok = parseDecimalScaled(t, scale, v1); if (!ok) return false; *set1 = true;
  } else { *set1 = false; }
  return true;
}

// Legacy input functions removed - replaced by readLine() in unified menu system

// Legacy getInt64 function removed - replaced by parseInt64Simple() and parseDecimalScaled() in unified menu system

void printHzAsMHz(int64_t x)
{
  char str[128];
  int64_t MHz = x / 1000000;
  int64_t Hz = MHz * 1000000;
  int64_t fract = x - Hz;
  sprintf(str, "%ld.", (int32_t)MHz), Serial.print(str);
  sprintf(str,"%06ld", (int32_t)fract), Serial.print(str);
}

/******************************************************************************
 * Legacy menu functions removed - replaced by unified menu system
*******************************************************************************/

char modeToChar(unsigned char mode)
{
		switch (mode)
		{
			case Timestamp: return 'T';
			case Interval:  return 'I';
			case Period:    return 'P';
			case timeLab:   return 'L';
			case Debug:     return 'D';
		}
   return '?';
}

struct config_t defaultConfig() {
  struct config_t x;
  x.VERSION == EEPROM_VERSION;
  strncpy(x.SW_VERSION,SW_VERSION,sizeof(SW_VERSION));
  x.BOARD_REV = BOARD_REVISION;
  strncpy(x.SER_NUM,SER_NUM,sizeof(SER_NUM));
  x.MODE = DEFAULT_MODE;
  x.POLL_CHAR = DEFAULT_POLL_CHAR;
  x.CLOCK_HZ = DEFAULT_CLOCK_HZ;
  x.PICTICK_PS = DEFAULT_PICTICK_PS;
  x.CAL_PERIODS = DEFAULT_CAL_PERIODS;
  x.TIMEOUT = DEFAULT_TIMEOUT;
  x.WRAP = DEFAULT_WRAP;
  x.PLACES = DEFAULT_PLACES;
  x.SYNC_MODE = DEFAULT_SYNC_MODE;
  x.NAME[0] = DEFAULT_NAME_0;
  x.NAME[1] = DEFAULT_NAME_1;
  x.PROP_DELAY[0] = DEFAULT_PROP_DELAY_0;
  x.PROP_DELAY[1] = DEFAULT_PROP_DELAY_1;
  x.START_EDGE[0] = DEFAULT_START_EDGE_0;
  x.START_EDGE[1] = DEFAULT_START_EDGE_1;
  x.TIME_DILATION[0] = DEFAULT_TIME_DILATION_0;
  x.TIME_DILATION[1] = DEFAULT_TIME_DILATION_1;
  x.FIXED_TIME2[0] = DEFAULT_FIXED_TIME2_0;
  x.FIXED_TIME2[1] = DEFAULT_FIXED_TIME2_1;
  x.FUDGE0[0] = DEFAULT_FUDGE0_0;
  x.FUDGE0[1] = DEFAULT_FUDGE0_1;
  return x;
}

// initializeConfig function removed - unused

// Process a single command and return true if the command was processed successfully
static bool processCommand(struct config_t *pConfigInfo, char *cmdLine, bool *showMenu) {
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
    else if (choice == '2') pConfigInfo->MODE = Interval;
    else if (choice == '3') pConfigInfo->MODE = Period;
    else if (choice == '4') pConfigInfo->MODE = timeLab;
    else if (choice == '5') pConfigInfo->MODE = Debug;
    else if (choice == '6') pConfigInfo->MODE = Null;
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
      char *cline;
      if (strlen(args) >= 2) {  // Need at least 2 chars for G1 plus parameter
        // Direct parameter provided (e.g., "G1"10.0")
        cline = args + 1;  // Skip past "G1"
      } else {
        // Interactive mode
        configPrint("Clock MHz: "); 
        char buf[96];
        size_t cn = readLine(buf, sizeof(buf)); 
        cline = trimInPlace(buf);
      }
      
      int64_t hz; if (parseDecimalScaled(cline, 1000000LL, &hz) && hz > 0) { 
        int64_t old=pConfigInfo->CLOCK_HZ; pConfigInfo->CLOCK_HZ = hz; 
        MARK_CONFIG_CHANGED();
        char m[64]; sprintf(m, "OK -- Clock %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(hz/1000000LL),(int32_t)(hz%1000000LL)); configPrint(m); 
      } else configPrint("Invalid\r\n");
      Serial.flush();
    }
    else if (choice == '2') {
      // G2) Coarse tick us
      char *cline;
      if (strlen(args) >= 2) {  // Need at least 2 chars for G2 plus parameter
        // Direct parameter provided (e.g., "G2"100.0")
        cline = args + 1;  // Skip past "G2"
      } else {
        // Interactive mode
        configPrint("Coarse tick (us): "); 
        char buf[96];
        size_t cn = readLine(buf, sizeof(buf)); 
        cline = trimInPlace(buf);
      }
      
      int64_t ps; if (parseDecimalScaled(cline, 1000000LL, &ps) && ps > 0) { 
        int64_t old=pConfigInfo->PICTICK_PS; pConfigInfo->PICTICK_PS = ps; 
        MARK_CONFIG_CHANGED();
        char m[64]; sprintf(m, "OK -- Coarse %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(ps/1000000LL),(int32_t)(ps%1000000LL)); configPrint(m); 
      } else configPrint("Invalid\r\n");
      Serial.flush();
    }
    else if (choice == '3') {
      // G3) Prop delays
      char *cline;
      if (strlen(args) >= 2) {  // Need at least 2 chars for G3 plus parameter
        // Direct parameter provided (e.g., "G3"100/200")
        cline = args + 1;  // Skip past "G3"
      } else {
        // Interactive mode
        configPrint("Enter pair A/B: "); 
        char buf[96];
        size_t cn = readLine(buf, sizeof(buf)); 
        cline = trimInPlace(buf);
      }
      
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { 
        configPrint("Invalid\r\n"); Serial.flush(); 
      } else { 
        int32_t o0=pConfigInfo->PROP_DELAY[0], o1=pConfigInfo->PROP_DELAY[1]; 
        if (s0) pConfigInfo->PROP_DELAY[0]=v0; if (s1) pConfigInfo->PROP_DELAY[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; sprintf(m, "OK -- PropDelay %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->PROP_DELAY[0],(long)pConfigInfo->PROP_DELAY[1]); configPrint(m); Serial.flush(); 
      }
    }
    else if (choice == '4') {
      // G4) Time dilation
      char *cline;
      if (strlen(args) >= 2) {  // Need at least 2 chars for G4 plus parameter
        // Direct parameter provided (e.g., "G4"1.001/1.002")
        cline = args + 1;  // Skip past "G4"
      } else {
        // Interactive mode
        configPrint("Enter pair A/B: "); 
        char buf[96];
        size_t cn = readLine(buf, sizeof(buf)); 
        cline = trimInPlace(buf);
      }
      
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { 
        configPrint("Invalid\r\n"); Serial.flush(); 
      } else { 
        int32_t o0=pConfigInfo->TIME_DILATION[0], o1=pConfigInfo->TIME_DILATION[1]; 
        if (s0) pConfigInfo->TIME_DILATION[0]=v0; if (s1) pConfigInfo->TIME_DILATION[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; sprintf(m, "OK -- TimeDilation %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->TIME_DILATION[0],(long)pConfigInfo->TIME_DILATION[1]); configPrint(m); Serial.flush(); 
      }
    }
    else if (choice == '5') {
      // G5) fixedTime2
      char *cline;
      if (strlen(args) >= 2) {  // Need at least 2 chars for G5 plus parameter
        // Direct parameter provided (e.g., "G5"1000/2000")
        cline = args + 1;  // Skip past "G5"
      } else {
        // Interactive mode
        configPrint("Enter pair A/B: "); 
        char buf[96];
        size_t cn = readLine(buf, sizeof(buf)); 
        cline = trimInPlace(buf);
      }
      
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { 
        configPrint("Invalid\r\n"); Serial.flush(); 
      } else { 
        int32_t o0=pConfigInfo->FIXED_TIME2[0], o1=pConfigInfo->FIXED_TIME2[1]; 
        if (s0) pConfigInfo->FIXED_TIME2[0]=v0; if (s1) pConfigInfo->FIXED_TIME2[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; sprintf(m, "OK -- fixedTime2 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FIXED_TIME2[0],(long)pConfigInfo->FIXED_TIME2[1]); configPrint(m); Serial.flush(); 
      }
    }
    else if (choice == '6') {
      // G6) FUDGE0
      char *cline;
      if (strlen(args) >= 2) {  // Need at least 2 chars for G6 plus parameter
        // Direct parameter provided (e.g., "G6"50/100")
        cline = args + 1;  // Skip past "G6"
      } else {
        // Interactive mode
        configPrint("Enter pair A/B: "); 
        char buf[96];
        size_t cn = readLine(buf, sizeof(buf)); 
        cline = trimInPlace(buf);
      }
      
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { 
        configPrint("Invalid\r\n"); Serial.flush(); 
      } else { 
        int32_t o0=pConfigInfo->FUDGE0[0], o1=pConfigInfo->FUDGE0[1]; 
        if (s0) pConfigInfo->FUDGE0[0]=v0; if (s1) pConfigInfo->FUDGE0[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; sprintf(m, "OK -- FUDGE0 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FUDGE0[0],(long)pConfigInfo->FUDGE0[1]); configPrint(m); Serial.flush(); 
      }
    }
    else {
      configPrint("Invalid advanced choice\r\n");
    }
    return true;
  }

  // Main menu commands
  if (cmd == 'A') {
    // Interactive Mode submenu
    for (;;) {
      configPrint("\r\n");
      configPrint("-- Mode --\r\n");
      configPrint("A1 - Timestamps\r\n");
      configPrint("A2 - Time Interval A -> B\r\n");
      configPrint("A3 - Period\r\n");
      configPrint("A4 - TimeLab 3-Cornered Hat\r\n");
      configPrint("A5 - Debug\r\n");
      configPrint("A6 - Null Output\r\n");
      configPrint("\r\n");
      configPrint("Current mode: ");
      
      switch (pConfigInfo->MODE) {
        case Timestamp: serialPrintImmediate("Timestamp"); break;
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
          else if (m == 'A' && mline[1] == '2') pConfigInfo->MODE = Interval;
          else if (m == 'A' && mline[1] == '3') pConfigInfo->MODE = Period;
          else if (m == 'A' && mline[1] == '4') pConfigInfo->MODE = timeLab;
          else if (m == 'A' && mline[1] == '5') pConfigInfo->MODE = Debug;
          else if (m == 'A' && mline[1] == '6') pConfigInfo->MODE = Null;
          
          // Show mode change confirmation and mark config as changed
          if (old != pConfigInfo->MODE) {
            char msg[128];
            sprintf(msg, "Mode was %s; now %s\r\n", 
                    (old == Timestamp) ? "Timestamp" :
                    (old == Interval) ? "Time Interval A->B" :
                    (old == Period) ? "Period" :
                    (old == timeLab) ? "TimeLab 3-Cornered Hat" :
                    (old == Debug) ? "Debug" : "Null Output",
                    (pConfigInfo->MODE == Timestamp) ? "Timestamp" :
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

  if (cmd == '?' || cmd == 'M') { *showMenu = true; configPrint("\r\n"); return true; }

  // B) Wrap digits
  if (cmd == 'B') {
    char *line;
    if (strlen(args) >= 1) {
      // Direct parameter provided (e.g., "B5")
      line = args;
    } else {
      // Interactive mode
      configPrint("Wrap digits (0..10): "); 
      char buf[96];
      size_t n = readLine(buf, sizeof(buf)); 
      line = trimInPlace(buf);
    }
    
    int64_t wrap; if (parseInt64Simple(line, &wrap) && wrap >= 0 && wrap <= 10) { 
      int16_t old=pConfigInfo->WRAP; pConfigInfo->WRAP = (int16_t)wrap; 
      MARK_CONFIG_CHANGED();
      char m[64]; sprintf(m, "OK -- Wrap %d -> %d\r\n", (int)old, (int)pConfigInfo->WRAP); configPrint(m); 
    } else configPrint("Invalid\r\n");
    Serial.flush();
    return true;
  }
  
  // C) Output decimal places
  if (cmd == 'C') {
    char *line;
    if (strlen(args) >= 1) {
      // Direct parameter provided (e.g., "C6")
      line = args;
    } else {
      // Interactive mode
      configPrint("Output decimal places (0..12): "); 
      char buf[96];
      size_t n = readLine(buf, sizeof(buf)); 
      line = trimInPlace(buf);
    }
    
    int64_t places; if (parseInt64Simple(line, &places) && places >= 0 && places <= 12) { 
      int16_t old=pConfigInfo->PLACES; pConfigInfo->PLACES = (int16_t)places; 
      MARK_CONFIG_CHANGED();
      char m[64]; sprintf(m, "OK -- Places %d -> %d\r\n", (int)old, (int)pConfigInfo->PLACES); configPrint(m); 
    } else configPrint("Invalid\r\n");
    Serial.flush();
    return true;
  }
  
  // D) Trigger edges
  if (cmd == 'D') {
    char *ln;
    if (strlen(args) >= 3) {
      // Direct parameter provided (e.g., "DR/F")
      ln = args;
    } else {
      // Interactive mode
      configPrint("Enter edges A/B (R/F): "); 
      char buf[96];
      size_t n = readLine(buf, sizeof(buf)); 
      ln = trimInPlace(buf);
    }
    
    if (ln[0] && ln[1] == '/' && ln[2]) {
      char e0 = toupper(ln[0]), e1 = toupper(ln[2]);
      if ((e0 == 'R' || e0 == 'F') && (e1 == 'R' || e1 == 'F')) {
        char o0=pConfigInfo->START_EDGE[0], o1=pConfigInfo->START_EDGE[1];
        pConfigInfo->START_EDGE[0]=e0; pConfigInfo->START_EDGE[1]=e1;
        MARK_CONFIG_CHANGED();
        char m[64]; sprintf(m, "OK -- Edges %c/%c -> %c/%c\r\n", o0,o1,e0,e1); configPrint(m);
      } else configPrint("Invalid\r\n");
    } else configPrint("Invalid\r\n");
    Serial.flush();
    return true;
  }
  
  // E) Sync mode
  if (cmd == 'E') {
    char *line;
    if (strlen(args) >= 1) {
      // Direct parameter provided (e.g., "EM")
      line = args;
    } else {
      // Interactive mode
      configPrint("Enter M or C: "); 
      char buf[96];
      size_t n = readLine(buf, sizeof(buf)); 
      line = trimInPlace(buf);
    }
    
    char c = toupper(line[0]); 
    if (c == 'M' || c == 'C') { 
      char old=pConfigInfo->SYNC_MODE; pConfigInfo->SYNC_MODE=c; 
      MARK_CONFIG_CHANGED();
      char m[64]; sprintf(m, "OK -- Sync %c -> %c\r\n", old, c); configPrint(m); 
    } else configPrint("Invalid\r\n");
    Serial.flush();
    return true;
  }
  
  // F) Channel names
  if (cmd == 'F') {
    char *ln;
    if (strlen(args) >= 3) {
      // Direct parameter provided (e.g., "FAB")
      ln = args;
    } else {
      // Interactive mode
      configPrint("Enter names A/B: "); 
      char buf[96];
      size_t n = readLine(buf, sizeof(buf)); 
      ln = trimInPlace(buf);
    }
    
    if (ln[0] && ln[1] == '/' && ln[2]) {
      char o0=pConfigInfo->NAME[0], o1=pConfigInfo->NAME[1]; 
      pConfigInfo->NAME[0]=ln[0]; pConfigInfo->NAME[1]=ln[2];
      MARK_CONFIG_CHANGED();
      char m[64]; sprintf(m, "OK -- Names %c/%c -> %c/%c\r\n", o0,o1,ln[0],ln[2]); configPrint(m);
    } else configPrint("Invalid\r\n");
    Serial.flush();
    return true;
  }

  // G) Poll char
  if (cmd == 'G') {
    char *line;
    if (strlen(args) >= 1) {
      // Direct parameter provided (e.g., "Gx")
      line = args;
    } else {
      // Interactive mode
      char old = pConfigInfo->POLL_CHAR;
      configPrint("Enter poll character (space to clear): ");
      char buf[96];
      size_t n = readLine(buf, sizeof(buf)); 
      line = trimInPlace(buf);
    }
    
    char old = pConfigInfo->POLL_CHAR;
    pConfigInfo->POLL_CHAR = (line[0] == '\0' || line[0] == ' ') ? 0x00 : line[0];
    MARK_CONFIG_CHANGED();
    char msg[64]; 
    if (old) sprintf(msg, "OK -- Poll Char %c -> %c\r\n", old, pConfigInfo->POLL_CHAR ? pConfigInfo->POLL_CHAR : ' '); 
    else sprintf(msg, "OK -- Poll Char none -> %c\r\n", pConfigInfo->POLL_CHAR ? pConfigInfo->POLL_CHAR : ' '); 
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

  // X) Write changes to EEPROM (without restart)
  if (cmd == 'X') {
    EEPROM_writeAnything(CONFIG_START, *pConfigInfo);
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
            configPrint("Coarse tick (us): "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            int64_t ps; if (parseDecimalScaled(cline, 1000000LL, &ps) && ps > 0) { int64_t old=pConfigInfo->PICTICK_PS; pConfigInfo->PICTICK_PS = ps; char m[64]; sprintf(m, "OK -- Coarse %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(ps/1000000LL),(int32_t)(ps%1000000LL)); configPrint(m); } else configPrint("Invalid\r\n");
            Serial.flush();
          }
          // H3) Prop delays
          else if (a == 'H' && aline[1] == '3') {
            configPrint("Enter pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { configPrint("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->PROP_DELAY[0], o1=pConfigInfo->PROP_DELAY[1]; if (s0) pConfigInfo->PROP_DELAY[0]=v0; if (s1) pConfigInfo->PROP_DELAY[1]=v1; char m[80]; sprintf(m, "OK -- PropDelay %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->PROP_DELAY[0],(long)pConfigInfo->PROP_DELAY[1]); configPrint(m); Serial.flush(); }
          }
          // H4) Time dilation
          else if (a == 'H' && aline[1] == '4') {
            configPrint("Enter pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { configPrint("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->TIME_DILATION[0], o1=pConfigInfo->TIME_DILATION[1]; if (s0) pConfigInfo->TIME_DILATION[0]=v0; if (s1) pConfigInfo->TIME_DILATION[1]=v1; char m[80]; sprintf(m, "OK -- TimeDilation %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->TIME_DILATION[0],(long)pConfigInfo->TIME_DILATION[1]); configPrint(m); Serial.flush(); }
          }
          // H5) fixedTime2
          else if (a == 'H' && aline[1] == '5') {
            configPrint("Enter pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { configPrint("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->FIXED_TIME2[0], o1=pConfigInfo->FIXED_TIME2[1]; if (s0) pConfigInfo->FIXED_TIME2[0]=v0; if (s1) pConfigInfo->FIXED_TIME2[1]=v1; char m[80]; sprintf(m, "OK -- fixedTime2 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FIXED_TIME2[0],(long)pConfigInfo->FIXED_TIME2[1]); configPrint(m); Serial.flush(); }
          }
          // H6) FUDGE0
          else if (a == 'H' && aline[1] == '6') {
            configPrint("Enter pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { configPrint("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->FUDGE0[0], o1=pConfigInfo->FUDGE0[1]; if (s0) pConfigInfo->FUDGE0[0]=v0; if (s1) pConfigInfo->FUDGE0[1]=v1; char m[80]; sprintf(m, "OK -- FUDGE0 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FUDGE0[0],(long)pConfigInfo->FUDGE0[1]); configPrint(m); Serial.flush(); }
          }
          else { configPrint("Invalid\r\n"); Serial.flush(); }
        }
      }
    }
    return true;
  }

  // Numbered exits - now handled by main loop restart/resume logic
  if (cmd == '1') { 
    configPrint("Discarded changes.\r\n"); 
    config_changed = 0; // Clear the changed flag since we're discarding
    return false; 
  }
  if (cmd == '2') { 
    // Apply changes and restart
    configPrint("Applying changes and restarting...\r\n"); 
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
    return false; 
  }

  configPrint("? Unknown command\r\n");
  return true;
}

void doSetupMenu(struct config_t *pConfigInfo)      // line-oriented, robust serial menu with semicolon support
{
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
        case Period:    serialPrintImmediate("Period"); break;
        case Interval:  serialPrintImmediate("Interval A->B"); break;
        case timeLab:   serialPrintImmediate("TimeLab 3-ch"); break;
        case Debug:     serialPrintImmediate("Debug"); break;
        case Null:      serialPrintImmediate("Null"); break;
      }
      serialPrintImmediate(")\r\n");
      // B) Wrap digits
      {
        char tmp[48]; sprintf(tmp, "B - Timestamp Wrap digits (currently: %d)\r\n", (int)pConfigInfo->WRAP);
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
      char tmp[48]; sprintf(tmp, "E - Master/Client (currently: %c)\r\n", pConfigInfo->SYNC_MODE);
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
      configPrint("\r\n");
      configPrint("? - Show this menu again\r\n");
      configPrint("I - Show startup info\r\n");
      configPrint("X - Write changes to EEPROM (persist across restarts)\r\n");
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

void UserConfig(struct config_t *pConfigInfo) 
{
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

// Pretty-print mode
void print_MeasureMode(MeasureMode x) {
  switch (x) {
    case Timestamp:
      Serial.println("Timestamp");
      break;
    case Period:
      Serial.println("Period");
      break;
    case Interval:
      Serial.println("Time Interval A->B");
      break;
    case timeLab:
      Serial.println("TimeLab 3-Cornered Hat");
      break;
    case Debug:
      Serial.println("Debug");
      break;
  }  
}

void eeprom_write_config_default (uint16_t offset) {
  struct config_t x = defaultConfig();
  EEPROM_writeAnything(offset,x);
}

void print_config (config_t x) {
  char tmpbuf[8];
  
  // Software Version
  Serial.print("# Software Version: ");Serial.print(SW_VERSION);
  if (strlen(SW_TAG) > 0) {
    Serial.print(" (");Serial.print(SW_TAG);Serial.print(")");
  }
  Serial.println();
  
  // EEPROM Version and Board Version
  Serial.print("# EEPROM Version: ");Serial.print(EEPROM.read(CONFIG_START)); 
  Serial.print(", Board Version: ");Serial.println(x.BOARD_REV);
  
  // Board Serial Number
  Serial.print("# Board Serial Number: ");Serial.println(x.SER_NUM);
  
  // Measurement Mode (most important param)
  Serial.print("# Measurement Mode: ");print_MeasureMode(MeasureMode(x.MODE));
  
  // Timestamp Wrap
  Serial.print("# Timestamp Wrap: ");Serial.println(x.WRAP);
  
  // Output Decimal Places
  Serial.print("# Output Decimal Places: ");Serial.println(x.PLACES);
  
  // Trigger Edge
  Serial.print("# Trigger Edge: ");Serial.print(x.START_EDGE[0]);Serial.print(" (ch0), ");  
  Serial.print(x.START_EDGE[1]);Serial.println(" (ch1)");
  
  // SyncMode
  Serial.print("# SyncMode: ");Serial.println(x.SYNC_MODE);
  
  // Channel Names
  Serial.print("# Channel Names: ");Serial.print(x.NAME[0]);Serial.print("/");Serial.println(x.NAME[1]);
  
  // Poll Character (moved to follow Channel Names)
  Serial.print("# Poll Character: ");
  if (x.POLL_CHAR) {
    Serial.println(x.POLL_CHAR);
  } else {
    Serial.println("none");
  }
  
  // Clock Speed
  Serial.print("# Clock Speed: ");printHzAsMHz(x.CLOCK_HZ);Serial.println(" MHz");
  
  // Coarse tick
  Serial.print("# Coarse tick: ");printHzAsMHz(x.PICTICK_PS);Serial.println(" usec");
  
  // Cal Periods
  Serial.print("# Cal Periods: ");Serial.println(x.CAL_PERIODS);
  
  // PropDelay
  Serial.print("# PropDelay: ");Serial.print((int32_t)x.PROP_DELAY[0]);
  Serial.print(" (ch0), ");Serial.print((int32_t)x.PROP_DELAY[1]);Serial.println(" (ch1)");
  
  // Timeout
  Serial.print("# Timeout: ");
  sprintf(tmpbuf,"0x%.2X",x.TIMEOUT);Serial.println(tmpbuf);
  
  // Time Dilation
  Serial.print("# Time Dilation: ");Serial.print((int32_t)x.TIME_DILATION[0]);
  Serial.print(" (ch0), ");Serial.print((int32_t)x.TIME_DILATION[1]);Serial.println(" (ch1)");
  
  // FIXED_TIME2
  Serial.print("# FIXED_TIME2: ");Serial.print((int32_t)x.FIXED_TIME2[0]);
  Serial.print(" (ch0), ");Serial.print((int32_t)x.FIXED_TIME2[1]);Serial.println(" (ch1)");
  
  // FUDGE0
  Serial.print("# FUDGE0: ");Serial.print((int32_t)x.FUDGE0[0]);
  Serial.print(" (ch0), ");Serial.print((int32_t)x.FUDGE0[1]);Serial.println(" (ch1)");
}

void get_serial_number() { 

  // Serial number is 8 bytes.  On first run,
  // check location and if not found, use random()
  // to generate.  If found, just read, format as string,
  // and set config.SER_NUM
  
  int32_t x,y;  // 2 longs because sprintf can't handle uint64
  
  EEPROM_readAnything(SER_NUM_START,x);
  EEPROM_readAnything(SER_NUM_START+4,y);

  // New Ardiuno has all EEPROM set to 0xFF; example
  // clear routine sets to 0x00.  Test for both
  // If no serial number, make one
  if ( ((x == 0xFFFFFFFF) && (y == 0xFFFFFFFF)) ||
       ((x == 0x00000000) && (y == 0x00000000)) ) {
    Serial.println("No serial number found... making one");
    randomSeed(analogRead(A0));  // seed with noise from A0
    x = random(0xFFFF);
    randomSeed(analogRead(A3));  // seed with noise from A3
    y = random(0xFFFF);
    EEPROM_writeAnything(SER_NUM_START,x);
    EEPROM_writeAnything(SER_NUM_START+4,y);
    sprintf(SER_NUM, "%04lX%04lX", x,y); 
    Serial.print("Serial Number: ");
    Serial.println(SER_NUM);
    delay(7500);
  }
  sprintf(SER_NUM, "%04lX%04lX", x,y);
}

void eeprom_clear() {
  // write 0xFF (factory default) to entire eeprom area
  for (int i = 0 ; i < EEPROM.length() ; i++) {
  EEPROM.write(i, 0xFF);
  } 
}

