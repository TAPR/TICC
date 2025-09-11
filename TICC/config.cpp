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
#include <avr/wdt.h>          // watchdog for software reset

#include "misc.h"             // random functions
#include "config.h"           // config and eeprom
#include "board.h"            // Arduino pin definitions
#include "tdc7200.h"          // TDC registers and structures

extern const char SW_VERSION[17]; // set in TICC.ino
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
  char *args = line + 1; while (*args == ' ') args++;
  
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
      serialPrintImmediate("Invalid mode choice\r\n");
      return true;
    }
    MARK_CONFIG_CHANGED();
    char msg[64]; sprintf(msg, "OK -- Mode set to %d\r\n", (int)choice); serialPrintImmediate(msg);
    return true;
  }
  
  if (cmd == 'G' && strlen(line) >= 2 && isdigit(line[1])) {
    // Advanced submenu commands
    char choice = line[1];
    if (choice == '1') {
      // G1) Clock speed MHz - need user input
      serialPrintImmediate("Clock MHz: "); 
      char buf[96];
      size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
      int64_t hz; if (parseDecimalScaled(cline, 1000000LL, &hz) && hz > 0) { 
        int64_t old=pConfigInfo->CLOCK_HZ; pConfigInfo->CLOCK_HZ = hz; 
        MARK_CONFIG_CHANGED();
        char m[64]; sprintf(m, "OK -- Clock %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(hz/1000000LL),(int32_t)(hz%1000000LL)); serialPrintImmediate(m); 
      } else serialPrintImmediate("Invalid\r\n");
      Serial.flush();
    }
    else if (choice == '2') {
      // G2) Coarse tick us - need user input
      serialPrintImmediate("Coarse tick (us): "); 
      char buf[96];
      size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
      int64_t ps; if (parseDecimalScaled(cline, 1000000LL, &ps) && ps > 0) { 
        int64_t old=pConfigInfo->PICTICK_PS; pConfigInfo->PICTICK_PS = ps; 
        MARK_CONFIG_CHANGED();
        char m[64]; sprintf(m, "OK -- Coarse %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(ps/1000000LL),(int32_t)(ps%1000000LL)); serialPrintImmediate(m); 
      } else serialPrintImmediate("Invalid\r\n");
      Serial.flush();
    }
    else if (choice == '3') {
      // G3) Prop delays - need user input
      serialPrintImmediate("Enter pair A/B: "); 
      char buf[96];
      size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { 
        serialPrintImmediate("Invalid\r\n"); Serial.flush(); 
      } else { 
        int32_t o0=pConfigInfo->PROP_DELAY[0], o1=pConfigInfo->PROP_DELAY[1]; 
        if (s0) pConfigInfo->PROP_DELAY[0]=v0; if (s1) pConfigInfo->PROP_DELAY[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; sprintf(m, "OK -- PropDelay %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->PROP_DELAY[0],(long)pConfigInfo->PROP_DELAY[1]); serialPrintImmediate(m); Serial.flush(); 
      }
    }
    else if (choice == '4') {
      // G4) Time dilation - need user input
      serialPrintImmediate("Enter pair A/B: "); 
      char buf[96];
      size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { 
        serialPrintImmediate("Invalid\r\n"); Serial.flush(); 
      } else { 
        int32_t o0=pConfigInfo->TIME_DILATION[0], o1=pConfigInfo->TIME_DILATION[1]; 
        if (s0) pConfigInfo->TIME_DILATION[0]=v0; if (s1) pConfigInfo->TIME_DILATION[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; sprintf(m, "OK -- TimeDilation %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->TIME_DILATION[0],(long)pConfigInfo->TIME_DILATION[1]); serialPrintImmediate(m); Serial.flush(); 
      }
    }
    else if (choice == '5') {
      // G5) fixedTime2 - need user input
      serialPrintImmediate("Enter pair A/B: "); 
      char buf[96];
      size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { 
        serialPrintImmediate("Invalid\r\n"); Serial.flush(); 
      } else { 
        int32_t o0=pConfigInfo->FIXED_TIME2[0], o1=pConfigInfo->FIXED_TIME2[1]; 
        if (s0) pConfigInfo->FIXED_TIME2[0]=v0; if (s1) pConfigInfo->FIXED_TIME2[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; sprintf(m, "OK -- fixedTime2 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FIXED_TIME2[0],(long)pConfigInfo->FIXED_TIME2[1]); serialPrintImmediate(m); Serial.flush(); 
      }
    }
    else if (choice == '6') {
      // G6) FUDGE0 - need user input
      serialPrintImmediate("Enter pair A/B: "); 
      char buf[96];
      size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
      bool s0=false, s1=false; int64_t v0=0, v1=0; 
      if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { 
        serialPrintImmediate("Invalid\r\n"); Serial.flush(); 
      } else { 
        int32_t o0=pConfigInfo->FUDGE0[0], o1=pConfigInfo->FUDGE0[1]; 
        if (s0) pConfigInfo->FUDGE0[0]=v0; if (s1) pConfigInfo->FUDGE0[1]=v1; 
        MARK_CONFIG_CHANGED();
        char m[80]; sprintf(m, "OK -- FUDGE0 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FUDGE0[0],(long)pConfigInfo->FUDGE0[1]); serialPrintImmediate(m); Serial.flush(); 
      }
    }
    else {
      serialPrintImmediate("Invalid advanced choice\r\n");
    }
    return true;
  }

  // Main menu commands
  if (cmd == 'A') {
    // Interactive Mode submenu
    for (;;) {
      serialPrintImmediate("\r\n-- Mode --\r\n");
      serialPrintImmediate("A1 - Timestamps\r\n");
      serialPrintImmediate("A2 - Time Interval A -> B\r\n");
      serialPrintImmediate("A3 - Period\r\n");
      serialPrintImmediate("A4 - TimeLab 3-cornered Hat\r\n");
      serialPrintImmediate("A5 - Debug\r\n");
      serialPrintImmediate("A6 - Null Output\r\n");
      serialPrintImmediate("Current mode: ");
      switch (pConfigInfo->MODE) {
        case Timestamp: serialPrintImmediate("Timestamp"); break;
        case Period:    serialPrintImmediate("Period"); break;
        case Interval:  serialPrintImmediate("Time Interval A->B"); break;
        case timeLab:   serialPrintImmediate("TimeLab 3-cornered Hat"); break;
        case Debug:     serialPrintImmediate("Debug"); break;
        case Null:      serialPrintImmediate("Null Output"); break;
      }
      serialPrintImmediate("\r\n1 - Discard changes and return to main menu\r\n");
      serialPrintImmediate("2 - Keep changes and return to main menu\r\n");
      serialPrintImmediate("> ");
      char buf[96];
      size_t mn = readLine(buf, sizeof(buf)); char *mline = trimInPlace(buf);
      if (mn) {
        if (mline[0] == '1' || mline[0] == '2') {
          // Return options
          if (mline[0] == '1') {
            serialPrintImmediate("Mode changes discarded.\r\n");
          } else {
            serialPrintImmediate("Mode changes kept.\r\n");
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
        }
      }
    }
    return true;
  }

  if (cmd == '?' || cmd == 'M') { *showMenu = true; serialPrintImmediate("\r\n"); return true; }

  // B) Wrap digits
  if (cmd == 'B') {
    serialPrintImmediate("Wrap digits (0..10): "); 
    char buf[96];
    size_t n = readLine(buf, sizeof(buf)); char *line = trimInPlace(buf);
    int64_t wrap; if (parseInt64Simple(line, &wrap) && wrap >= 0 && wrap <= 10) { 
      int16_t old=pConfigInfo->WRAP; pConfigInfo->WRAP = (int16_t)wrap; 
      MARK_CONFIG_CHANGED();
      char m[64]; sprintf(m, "OK -- Wrap %d -> %d\r\n", (int)old, (int)pConfigInfo->WRAP); serialPrintImmediate(m); 
    } else serialPrintImmediate("Invalid\r\n");
    Serial.flush();
    return true;
  }
  
  // C) Trigger edges
  if (cmd == 'C') {
    serialPrintImmediate("Enter edges A/B (R/F): "); 
    char buf[96];
    size_t n = readLine(buf, sizeof(buf)); char *ln = trimInPlace(buf);
    if (ln[0] && ln[1] && ln[2] == '/' && ln[3]) {
      char e0 = toupper(ln[0]), e1 = toupper(ln[3]);
      if ((e0 == 'R' || e0 == 'F') && (e1 == 'R' || e1 == 'F')) {
        char o0=pConfigInfo->START_EDGE[0], o1=pConfigInfo->START_EDGE[1];
        pConfigInfo->START_EDGE[0]=e0; pConfigInfo->START_EDGE[1]=e1;
        MARK_CONFIG_CHANGED();
        char m[64]; sprintf(m, "OK -- Edges %c/%c -> %c/%c\r\n", o0,o1,e0,e1); serialPrintImmediate(m);
      } else serialPrintImmediate("Invalid\r\n");
    } else serialPrintImmediate("Invalid\r\n");
    Serial.flush();
    return true;
  }
  
  // D) Sync mode
  if (cmd == 'D') {
    serialPrintImmediate("Enter M or C: "); 
    char buf[96];
    size_t n = readLine(buf, sizeof(buf)); char *line = trimInPlace(buf);
    char c = toupper(line[0]); 
    if (c == 'M' || c == 'C') { 
      char old=pConfigInfo->SYNC_MODE; pConfigInfo->SYNC_MODE=c; 
      MARK_CONFIG_CHANGED();
      char m[64]; sprintf(m, "OK -- Sync %c -> %c\r\n", old, c); serialPrintImmediate(m); 
    } else serialPrintImmediate("Invalid\r\n");
    Serial.flush();
    return true;
  }
  
  // E) Channel names
  if (cmd == 'E') {
    serialPrintImmediate("Enter names A/B: "); 
    char buf[96];
    size_t n = readLine(buf, sizeof(buf)); char *ln = trimInPlace(buf);
    if (ln[0] && ln[1] == '/' && ln[2]) {
      char o0=pConfigInfo->NAME[0], o1=pConfigInfo->NAME[1]; 
      pConfigInfo->NAME[0]=ln[0]; pConfigInfo->NAME[1]=ln[2];
      MARK_CONFIG_CHANGED();
      char m[64]; sprintf(m, "OK -- Names %c/%c -> %c/%c\r\n", o0,o1,ln[0],ln[2]); serialPrintImmediate(m);
    } else serialPrintImmediate("Invalid\r\n");
    Serial.flush();
    return true;
  }

  // F) Poll char
  if (cmd == 'F') {
    char old = pConfigInfo->POLL_CHAR;
    serialPrintImmediate("Enter poll character (space to clear): ");
    char buf[96];
    size_t n = readLine(buf, sizeof(buf)); char *line = trimInPlace(buf);
    pConfigInfo->POLL_CHAR = (line[0] == '\0' || line[0] == ' ') ? 0x00 : line[0];
    MARK_CONFIG_CHANGED();
    char msg[64]; 
    if (old) sprintf(msg, "OK -- Poll Char %c -> %c\r\n", old, pConfigInfo->POLL_CHAR ? pConfigInfo->POLL_CHAR : ' '); 
    else sprintf(msg, "OK -- Poll Char none -> %c\r\n", pConfigInfo->POLL_CHAR ? pConfigInfo->POLL_CHAR : ' '); 
    serialPrintImmediate(msg);
    Serial.flush();
    return true;
  }

  // H) Show startup info
  if (cmd == 'H') {
    serialPrintImmediate("\r\n");
    print_config(*pConfigInfo);
    serialPrintImmediate("\r\n");
    return true;
  }

  // W) Write changes to EEPROM (without restart)
  if (cmd == 'W') {
    EEPROM_writeAnything(CONFIG_START, *pConfigInfo);
    serialPrintImmediate("Changes written to EEPROM (will persist across restarts)\r\n");
    return true;
  }

  // G) Advanced submenu
  if (cmd == 'G') {
    // Interactive Advanced submenu
    for (;;) {
      serialPrintImmediate("\r\n-- Advanced Settings --\r\n");
      
      // G1 - Clock Speed MHz
      {
        char tmp[64]; 
        int64_t MHz = pConfigInfo->CLOCK_HZ / 1000000LL;
        int64_t Hz = MHz * 1000000LL;
        int64_t fract = pConfigInfo->CLOCK_HZ - Hz;
        sprintf(tmp, "G1 - Clock Speed MHz (currently: %ld.%06ld)\r\n", (int32_t)MHz, (int32_t)fract);
        serialPrintImmediate(tmp);
      }
      
      // G2 - Coarse Tick us
      {
        char tmp[64]; 
        int64_t us = pConfigInfo->PICTICK_PS / 1000000LL;
        int64_t ps = us * 1000000LL;
        int64_t fract = pConfigInfo->PICTICK_PS - ps;
        sprintf(tmp, "G2 - Coarse Tick us (currently: %ld.%06ld)\r\n", (int32_t)us, (int32_t)fract);
        serialPrintImmediate(tmp);
      }
      
      // G3 - Propagation Delay ps A/B
      {
        char tmp[64]; 
        sprintf(tmp, "G3 - Propagation Delay ps A/B (currently: %ld/%ld)\r\n", (long)pConfigInfo->PROP_DELAY[0], (long)pConfigInfo->PROP_DELAY[1]);
        serialPrintImmediate(tmp);
      }
      
      // G4 - Time Dilation A/B
      {
        char tmp[64]; 
        sprintf(tmp, "G4 - Time Dilation A/B (currently: %ld/%ld)\r\n", (long)pConfigInfo->TIME_DILATION[0], (long)pConfigInfo->TIME_DILATION[1]);
        serialPrintImmediate(tmp);
      }
      
      // G5 - fixedTime2 ps A/B
      {
        char tmp[64]; 
        sprintf(tmp, "G5 - fixedTime2 ps A/B (currently: %ld/%ld)\r\n", (long)pConfigInfo->FIXED_TIME2[0], (long)pConfigInfo->FIXED_TIME2[1]);
        serialPrintImmediate(tmp);
      }
      
      // G6 - FUDGE0 ps A/B
      {
        char tmp[64]; 
        sprintf(tmp, "G6 - FUDGE0 ps A/B (currently: %ld/%ld)\r\n", (long)pConfigInfo->FUDGE0[0], (long)pConfigInfo->FUDGE0[1]);
        serialPrintImmediate(tmp);
      }
      
      serialPrintImmediate("1 - Discard changes and return to main menu\r\n");
      serialPrintImmediate("2 - Keep changes and return to main menu\r\n");
      serialPrintImmediate("> ");
      char buf[96];
      size_t an = readLine(buf, sizeof(buf)); char *aline = trimInPlace(buf);
      if (an) {
        if (aline[0] == '1' || aline[0] == '2') {
          // Return options - no changes needed
          if (aline[0] == '1') {
            serialPrintImmediate("Advanced changes discarded.\r\n");
          } else {
            serialPrintImmediate("Advanced changes kept.\r\n");
          }
          *showMenu = true;
          break; // Exit the submenu loop
        } else {
          // Advanced setting options
          char a = toupper(aline[0]);
          const char *aargs = aline + 1; while (*aargs == ' ') aargs++;
          
          // G1) Clock speed MHz
          if (a == 'G' && aline[1] == '1') {
            serialPrintImmediate("Clock MHz: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            int64_t hz; if (parseDecimalScaled(cline, 1000000LL, &hz) && hz > 0) { int64_t old=pConfigInfo->CLOCK_HZ; pConfigInfo->CLOCK_HZ = hz; char m[64]; sprintf(m, "OK -- Clock %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(hz/1000000LL),(int32_t)(hz%1000000LL)); serialPrintImmediate(m); } else serialPrintImmediate("Invalid\r\n");
            Serial.flush();
          }
          // G2) Coarse tick us
          else if (a == 'G' && aline[1] == '2') {
            serialPrintImmediate("Coarse tick (us): "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            int64_t ps; if (parseDecimalScaled(cline, 1000000LL, &ps) && ps > 0) { int64_t old=pConfigInfo->PICTICK_PS; pConfigInfo->PICTICK_PS = ps; char m[64]; sprintf(m, "OK -- Coarse %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(ps/1000000LL),(int32_t)(ps%1000000LL)); serialPrintImmediate(m); } else serialPrintImmediate("Invalid\r\n");
            Serial.flush();
          }
          // G3) Prop delays
          else if (a == 'G' && aline[1] == '3') {
            serialPrintImmediate("Enter pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { serialPrintImmediate("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->PROP_DELAY[0], o1=pConfigInfo->PROP_DELAY[1]; if (s0) pConfigInfo->PROP_DELAY[0]=v0; if (s1) pConfigInfo->PROP_DELAY[1]=v1; char m[80]; sprintf(m, "OK -- PropDelay %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->PROP_DELAY[0],(long)pConfigInfo->PROP_DELAY[1]); serialPrintImmediate(m); Serial.flush(); }
          }
          // G4) Time dilation
          else if (a == 'G' && aline[1] == '4') {
            serialPrintImmediate("Enter pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { serialPrintImmediate("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->TIME_DILATION[0], o1=pConfigInfo->TIME_DILATION[1]; if (s0) pConfigInfo->TIME_DILATION[0]=v0; if (s1) pConfigInfo->TIME_DILATION[1]=v1; char m[80]; sprintf(m, "OK -- TimeDilation %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->TIME_DILATION[0],(long)pConfigInfo->TIME_DILATION[1]); serialPrintImmediate(m); Serial.flush(); }
          }
          // G5) fixedTime2
          else if (a == 'G' && aline[1] == '5') {
            serialPrintImmediate("Enter pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { serialPrintImmediate("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->FIXED_TIME2[0], o1=pConfigInfo->FIXED_TIME2[1]; if (s0) pConfigInfo->FIXED_TIME2[0]=v0; if (s1) pConfigInfo->FIXED_TIME2[1]=v1; char m[80]; sprintf(m, "OK -- fixedTime2 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FIXED_TIME2[0],(long)pConfigInfo->FIXED_TIME2[1]); serialPrintImmediate(m); Serial.flush(); }
          }
          // G6) FUDGE0
          else if (a == 'G' && aline[1] == '6') {
            serialPrintImmediate("Enter pair A/B: "); size_t cn = readLine(buf, sizeof(buf)); char *cline = trimInPlace(buf);
            bool s0=false, s1=false; int64_t v0=0, v1=0; if (!parseInt64Pair(cline, &s0, &v0, &s1, &v1)) { serialPrintImmediate("Invalid\r\n"); Serial.flush(); } else { int32_t o0=pConfigInfo->FUDGE0[0], o1=pConfigInfo->FUDGE0[1]; if (s0) pConfigInfo->FUDGE0[0]=v0; if (s1) pConfigInfo->FUDGE0[1]=v1; char m[80]; sprintf(m, "OK -- FUDGE0 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FUDGE0[0],(long)pConfigInfo->FUDGE0[1]); serialPrintImmediate(m); Serial.flush(); }
          }
          else { serialPrintImmediate("Invalid\r\n"); Serial.flush(); }
        }
      }
    }
    return true;
  }

  // Numbered exits - now handled by main loop restart/resume logic
  if (cmd == '1') { 
    serialPrintImmediate("Discarded changes.\r\n"); 
    config_changed = 0; // Clear the changed flag since we're discarding
    return false; 
  }
  if (cmd == '2') { 
    // Apply changes and restart
    serialPrintImmediate("Applying changes and restarting...\r\n"); 
    return false; 
  }
  if (cmd == '3') { 
    // Apply changes and resume operation
    serialPrintImmediate("Applying changes and resuming operation...\r\n"); 
    return false; 
  }
  if (cmd == '4') { 
    // Reset all to defaults and restart
    eeprom_write_config_default(CONFIG_START); 
    serialPrintImmediate("Defaults written. Restarting...\r\n"); 
    return false; 
  }

  serialPrintImmediate("? Unknown command\r\n");
  return true;
}

void doSetupMenu(struct config_t *pConfigInfo)      // line-oriented, robust serial menu with semicolon support
{
  char buf[96];
  bool showMenu = true;
  for (;;) {
    if (showMenu) {
      serialPrintImmediate("\r\n== TICC Configuration ==\r\n");
      // A) Mode
      serialPrintImmediate("A - Mode (currently: ");
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
        serialPrintImmediate(tmp);
      }
      // C) Trigger edges
      {
        char tmp[64]; sprintf(tmp, "C - Trigger Edge A/B (currently: %c/%c)\r\n", pConfigInfo->START_EDGE[0], pConfigInfo->START_EDGE[1]);
        serialPrintImmediate(tmp);
      }
      // D) Sync mode
      {
        char tmp[48]; sprintf(tmp, "D - Master/Client (currently: %c)\r\n", pConfigInfo->SYNC_MODE);
        serialPrintImmediate(tmp);
      }
      // E) Channel names
      {
        char tmp[48]; sprintf(tmp, "E - Channel Names (currently: %c/%c)\r\n", pConfigInfo->NAME[0], pConfigInfo->NAME[1]);
        serialPrintImmediate(tmp);
      }
      // F) Poll char
      serialPrintImmediate("F - Poll Character (currently: ");
      if (pConfigInfo->POLL_CHAR) {
        char ch[8]; ch[0] = pConfigInfo->POLL_CHAR; ch[1] = 0; serialPrintImmediate(ch);
      } else {
        serialPrintImmediate("none");
      }
      serialPrintImmediate(")\r\n");
      // G) Advanced settings
      serialPrintImmediate("G - Advanced settings\r\n");
      serialPrintImmediate("H - Show startup info\r\n");
      serialPrintImmediate("W - Write changes to EEPROM (persist across restarts)\r\n");
      serialPrintImmediate("1 - Discard changes and exit\r\n");
      serialPrintImmediate("2 - Apply changes and restart\r\n");
      serialPrintImmediate("3 - Apply changes and resume operation\r\n");
      serialPrintImmediate("4 - Reset all to defaults and restart\r\n");
      serialPrintImmediate("? - Show this menu again\r\n");
      showMenu = false;
    }
    Serial.print("> ");
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
      Serial.println("TimeLab 3-channel");
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
  Serial.print("# Measurement Mode: ");print_MeasureMode(MeasureMode(x.MODE));
  Serial.print("# Poll Character: ");
  if (x.POLL_CHAR) {
            Serial.println(x.POLL_CHAR);          // normally unset
          } else {
            Serial.println("none");
          }
  Serial.print("# EEPROM Version: ");Serial.print(EEPROM.read(CONFIG_START)); 
  Serial.print(", Board Version: ");Serial.println(x.BOARD_REV);
  Serial.print("# Software Version: ");Serial.println(SW_VERSION); // Print from const, not from eeprom, which won't update until next "W" command
  Serial.print("# Board Serial Number: ");Serial.println(x.SER_NUM); 
  Serial.print("# Clock Speed: ");printHzAsMHz(x.CLOCK_HZ);Serial.println(" MHz");
  Serial.print("# Coarse tick: ");printHzAsMHz(x.PICTICK_PS);Serial.println(" usec");
  Serial.print("# Cal Periods: ");Serial.println(x.CAL_PERIODS);
  Serial.print("# Timestamp Wrap:  ");Serial.print(x.WRAP);Serial.println("");
  Serial.print("# SyncMode: ");Serial.println(x.SYNC_MODE);
  Serial.print("# Ch Names: ");Serial.print(x.NAME[0]);Serial.print("/");Serial.println(x.NAME[1]);
  Serial.print("# PropDelay: ");Serial.print((int32_t)x.PROP_DELAY[0]);
    Serial.print(" (ch0), ");Serial.print((int32_t)x.PROP_DELAY[1]);Serial.println(" (ch1)");
  Serial.print("# Timeout: ");
    sprintf(tmpbuf,"0x%.2X",x.TIMEOUT);Serial.println(tmpbuf);
  Serial.print("# Trigger Edge: ");Serial.print(x.START_EDGE[0]);Serial.print(" (ch0), ");  
    Serial.print(x.START_EDGE[1]);Serial.println(" (ch1)");
  Serial.print("# Time Dilation: ");Serial.print((int32_t)x.TIME_DILATION[0]);
    Serial.print(" (ch0), ");Serial.print((int32_t)x.TIME_DILATION[1]);Serial.println(" (ch1)");
  Serial.print("# FIXED_TIME2: ");Serial.print((int32_t)x.FIXED_TIME2[0]);
    Serial.print(" (ch0), ");Serial.print((int32_t)x.FIXED_TIME2[1]);Serial.println(" (ch1)");
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

// Perform a software reset via watchdog (AVR Mega 2560)
void software_reset() {
  wdt_enable(WDTO_15MS);
  while (1) { }
}
