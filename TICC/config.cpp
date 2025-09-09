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
      serialWriteImmediate('\r'); serialWriteImmediate('\n');
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

#define inputLineIndexMax 125
char inputLine[128];    // The above define is less than the declared size to ensure against overflow
int inputLineIndex = 0;
int inputLineReadIndex = 0;

char getChar()
{
  char c;
  while ( ! Serial.available())      /*    wait for a character   */     ;
  c = Serial.read();
  Serial.print(c);                      // echo the character received
  return c;
}

// get a character
// if  charSourceIsLine != 0   get the next character from inputLine
// inputLineReadIndex should be set to 0 after filling  inputLine and
// before calling getChar
char getChar(int charSourceIsLine)
{
  if (charSourceIsLine == 0)
  { return getChar();
  }
  else if (inputLineReadIndex < inputLineIndex)
  {
    Serial.print(inputLine[inputLineReadIndex]);    // Add this as a debug aid
    return inputLine[inputLineReadIndex++];
  }
  else return getChar();
}

// Get a string of characters from Serial input.  Return when buffer inputLine is full or when a newline character is received.
// The newline is  put into inputLine. In addition, inputLine is always none-terminated.
// ToDo:  
// (1) allow use of Backspace  DONE
// (2) allow use of Home, End, Delete;  
// (3) allow insertion of characters
void getLine()
{
  char newChar;
  for ( inputLineIndex = 0; ; )
  {
      newChar = getChar();
      if ( (inputLineIndex > inputLineIndexMax) || (newChar == '\n')  || (newChar == '\r') )
      {
        inputLine[inputLineIndex++] = '\r';
        inputLine[inputLineIndex] = '\0';
        if (inputLineIndex > 1) return;
      }
      else if (newChar == '\b')
      {
        Serial.print(' ');
        Serial.print(newChar);
        if (inputLineIndex > 0) --inputLineIndex;
      }
      else inputLine[inputLineIndex++] = newChar;
    }
}

/**********************************************************************************************************/
// Convert character to int64.
// The 64-bit integer part is placed in result.
// Handles any optionally-signed floating point format number, as long as it fits in [64 bit].[64 bit]
// In principle, that could mean up to 9,223,372,036,854,775,807
// No, maybe half that?  4,611,686,018,427,387,903 ?
// The values below are made available so that the fractional part may be used.
// The value of the fraction is    mantissaFractionPart / mantissaFractionPartPower
// The number of fraction digits is limited to 18.
// Algorithm based on http://krashan.ppa.pl/articles/stringtofloat/

  int getInt64new = 0;    // set to 1 if a new value has been parsed
  int64_t mantissa = 0;
  int64_t mantissaIntegerPart = 0;
  int64_t mantissaFractionPart = 0;
  int64_t mantissaFractionPartPower = 1;

// if 'source' is 0, characters are obtained from the Serial port;
// if source is 1, characters come from inputLine, using inputLineIndex
// parse Serial input and return value in result.  Result is unchanged if only a newline is received.  In this case getInt64new remains 0.
void getInt64(int64_t *result, int source)   
{
  int negative = 0;
  int exponentNegative = 0;
  int exponent = 0;
  int exponentInteger = 0;
  mantissa = 0;;
  mantissaIntegerPart = 0;
  mantissaFractionPart = 0;
  mantissaFractionPartPower = 1;
  getInt64new = 0;          // If we are successful in building a number, this will be set to one.
  char newChar;

  while ( isspace(newChar = getChar(source) )  ) /* ignore leading whitespace */ ;
  
  if (newChar == '+') newChar = getChar(source);    // allow leading plus
  else if (newChar == '-')                                      // handle leading minus
  {
    negative = 1;
    newChar = getChar(source);
  }
    while (newChar == '0')
    { newChar = getChar(source);   // ignore mantissa leading zeros     
      getInt64new = 1;                  // but note any zero entered
    }
    
  if (isdigit(newChar))    // get mantissa integer part
  {
    while (isdigit(newChar))
    {
      getInt64new = 1;
      mantissa *= 10;
      mantissa += (newChar - '0');
      newChar = getChar(source);
    }
  }
  else if (newChar == '.')          // No integer part; begin mantissa fractional part
  { newChar = getChar(source);
    while (newChar == '0')
    {
      --exponent;
      newChar = getChar(source);
    }
  }
  
    // newChar should be '.' or digit
    if (newChar == '.') newChar = getChar(source);
    while (isdigit(newChar))      // fractional part of mantissa
    {
      getInt64new = 1;
      mantissa *= 10;
      mantissa += (newChar - '0');
      --exponent;
      newChar = getChar(source);
    }
    // optional exponent
    if ( (newChar == 'e') || (newChar == 'E') )
    {
      newChar = getChar(source);
      if (newChar == '+') newChar = getChar(source);
     else if (newChar == '-') 
      {
        exponentNegative = 1;
        newChar = getChar(source);
      }
      while (newChar == '0') newChar = getChar(source);   // bypass exponent leading zeros
      while(isdigit(newChar))
      {
        exponentInteger *= 10;
        exponentInteger += (newChar - '0');
        newChar = getChar(source);
      }
      if (exponentNegative) exponentInteger = -exponentInteger;
      exponent += exponentInteger;
    }

    if (getInt64new == 0) return;   // no mantissa found

    // scale the mantissa according to the exponent, and separate into integer and fractional parts
    
    mantissaIntegerPart = mantissa;
    if (exponent < 0)  
    { 
      for (int i = exponent; i < 0; ++i)  mantissaFractionPartPower *= 10;
      mantissaFractionPart = mantissaIntegerPart % mantissaFractionPartPower;
      mantissaIntegerPart /= mantissaFractionPartPower;
    }
    if (exponent > 0)  for (int i = 0; i < exponent; ++i)   mantissaIntegerPart *= 10;
    if (negative) 
    { mantissaIntegerPart = -mantissaIntegerPart;
      mantissaFractionPart = -mantissaFractionPart;
    }
    *result = mantissaIntegerPart;
}

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
 * now start the actual menu routines
*******************************************************************************/
void measurementMode(struct config_t *pConfigInfo)
{
  int valid = 1;
Serial.println("Set mode.  Valid single-letter commands are:"), Serial.println();
  Serial.println("   T     Timestamp mode");
  Serial.println("   P     Period mode");
  Serial.println("   I     time Interval A->B mode");
  Serial.println("   L     TimeLab interval mode");
  Serial.println("   D     Debug mode");
  Serial.println("   N     Null output mode");
  Serial.println(),  Serial.print("Enter mode: ");

  do
  {
    valid = 1;
   switch( toupper(getChar()))
  {
    case 'T': pConfigInfo->MODE = Timestamp;    break;
    case 'P': pConfigInfo->MODE = Period;   break;
    case 'I': pConfigInfo->MODE = Interval;   break;
    case 'L': pConfigInfo->MODE = timeLab;    break;
    case 'D': pConfigInfo->MODE = Debug;    break;
    case 'N': pConfigInfo->MODE = Null;    break;
      default:  valid = 0;
      Serial.println("? Please enter T P I L D or N");
    break;
  }
  } while (valid == 0);
}

void pollChar(struct config_t *pConfigInfo)
{
  Serial.print("Poll Character ");
  if (pConfigInfo->POLL_CHAR) {
    Serial.print(pConfigInfo->POLL_CHAR); // normally unset
  } else {
    Serial.print("unset");
  }
 Serial.println("   Enter one character to set; enter space to unset");
  char c = toupper(getChar());
  if (c == ' ') { c = 0x00; }; // so don't use space as the poll character!
  pConfigInfo->POLL_CHAR = c; 
}

void clockSpeed(struct config_t *pConfigInfo)
{ int64_t clock;
  Serial.print("Clock speed "), printHzAsMHz(pConfigInfo->CLOCK_HZ), Serial.println(" MHz  (1 to 16 MHz, default 10 MHz)");
  Serial.println("Enter new value (in MHz) and press Enter (or just Enter for no change)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&clock, 1);
  if (( clock >= 1) && (clock <= 16) ) pConfigInfo->CLOCK_HZ = clock * 1000000 + mantissaFractionPart * 1000000 / mantissaFractionPartPower;    
}

void coarseClockRate(struct config_t *pConfigInfo)
{
  int64_t clock;
  Serial.print("Coarse Clock Rate "), printHzAsMHz(pConfigInfo->PICTICK_PS), Serial.println(" us.  Default 100 us");
  Serial.println("Enter new value (in us) and press Enter (or just Enter for no change)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&clock, 1);
  if (clock > 0) pConfigInfo->PICTICK_PS = clock * 1000000 + mantissaFractionPart * 1000000 / mantissaFractionPartPower;  // value entered in us, but stored in ps
}

void calibrationPeriods(struct config_t *pConfigInfo)
{
  int64_t periods;
  Serial.print("Calibration periods "), Serial.print((int32_t)pConfigInfo->CAL_PERIODS), Serial.println(" Choose one of 2, 10, 20, or 40.  Default 20");  // casting to int32_t discards upper 32 bits
  Serial.println("Enter new value and press Enter (or just Enter for no change)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&periods, 1);
  if (getInt64new)
          if ( (periods == 2) || (periods == 10) || (periods == 20) || (periods == 40) ) pConfigInfo->CAL_PERIODS = periods;
}

void timeout(struct config_t *pConfigInfo)
{
  int64_t timeout;
  char str[8];
  sprintf(str, "0x%02X", (int32_t)pConfigInfo->TIMEOUT);
  
  Serial.print("Timeout "), Serial.print(str), Serial.println(" Choose a value in the range 0 to 255.  Default 5");  // casting to int32_t discards upper 32 bits
  Serial.println("Enter new value and press Enter (or just Enter for no change)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&timeout, 1);
  if ( (getInt64new) && (timeout >= 0) && (timeout <= 255) ) pConfigInfo->TIMEOUT = timeout;   
}

void ts_wrap(struct config_t *pConfigInfo)
{
  // The final value here has a practical range from 0 through 10.  0 is default and means no timestamp
  // wrap. If not zero, value is the number of integer places left of the decimal point
  
  int64_t wrap;
  Serial.print("Timestamp Wraparound"), Serial.print(pConfigInfo->WRAP), Serial.println(" Default is no wrap");
  Serial.println("Enter new value for number of integers in timestamp (0 for no wrap) and press Enter (or just Enter for no change)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&wrap, 1);
  if (wrap == 0) {
    pConfigInfo->WRAP = DEFAULT_WRAP;
  } else {
    pConfigInfo->WRAP = wrap;
  }
}

void masterClient(struct config_t *pConfigInfo)
{
  Serial.print("Master or Client "), Serial.print(pConfigInfo->SYNC_MODE), Serial.println("   Choose M or C.  Default is M");
  char c = toupper(getChar());
  if ( (c == 'M') || (c == 'C') ) pConfigInfo->SYNC_MODE = c;
}

void channel_name(struct config_t *pConfigInfo)
{
  char delim[] = " ";
  Serial.print("Channel Names:");Serial.print(pConfigInfo->NAME[0]);Serial.print("/");Serial.println(pConfigInfo->NAME[1]);
  Serial.println("Enter single-character channel names with space between (e.g., 'C D').  Default is A B");
  
  getLine();
  char* ch0 = strtok(inputLine,delim);
  char* ch1 = strtok(NULL,delim);
  pConfigInfo->NAME[0] = ch0[1]; // not sure why we need index 1 for first, and index 0 for second
  pConfigInfo->NAME[1] = ch1[0]; 
}

void prop_delay(struct config_t *pConfigInfo)
{
  char delim[] = " ";
  Serial.print("Propagation Delay (ps):");Serial.print(int32_t(pConfigInfo->PROP_DELAY[0]));
  Serial.print("/");Serial.println(int32_t(pConfigInfo->PROP_DELAY[1]));
  Serial.println("Enter new delay values in integer ps with space between (e.g., '750 1250'); max 2,147,483,647.  Default is 0 0");
  
  getLine();
  char* p0 = strtok(inputLine,delim);
  char* p1 = strtok(NULL,delim);
    
  uint32_t prop0 = strtol(p0,NULL,10);
  uint32_t prop1 = strtol(p1,NULL,10);
  
  pConfigInfo->PROP_DELAY[0] = prop0;
  pConfigInfo->PROP_DELAY[1] = prop1;
}

void triggerEdge(struct config_t *pConfigInfo)
{
  char delim[] = " ";
  Serial.print("Trigger Edge:");Serial.print(pConfigInfo->START_EDGE[0]);Serial.print("/");Serial.println(pConfigInfo->START_EDGE[1]);
  Serial.println("Enter R for rising or F for falling edge for each channel with space between (e.g., 'R F').  Default is R R");
  
  getLine();
  char* token0 = strtok(inputLine,delim);
  char* token1 = strtok(NULL,delim);
  char e0 = token0 ? toupper(token0[0]) : '\0';
  char e1 = token1 ? toupper(token1[0]) : '\0';

  if ( (e0 == 'R') || (e0 == 'F') ) pConfigInfo->START_EDGE[0] = e0;
  if ( (e1 == 'R') || (e1 == 'F') ) pConfigInfo->START_EDGE[1] = e1;
}

void timeDilation(struct config_t *pConfigInfo)
{
  char delim[] = " ";
  Serial.print("Time Dilation:");Serial.print(int32_t(pConfigInfo->TIME_DILATION[0]));
  Serial.print("/");Serial.println(int32_t(pConfigInfo->TIME_DILATION[1]));
  Serial.println("Enter new dilation values as integers with space between (e.g., '750 1250'); max 2,147,483,647.  Default is 2500 2500");
  
  getLine();
  char* t0 = strtok(inputLine,delim);
  char* t1 = strtok(NULL,delim);
    
  uint32_t td0 = strtol(t0,NULL,10);
  uint32_t td1 = strtol(t1,NULL,10);
  
  pConfigInfo->TIME_DILATION[0] = td0;
  pConfigInfo->TIME_DILATION[1] = td1;
   
}

void fixedTime2(struct  config_t *pConfigInfo)
{
  char delim[] = " ";
  Serial.print("fixedTime2:");Serial.print(int32_t(pConfigInfo->FIXED_TIME2[0]));
  Serial.print("/");Serial.println(int32_t(pConfigInfo->FIXED_TIME2[1]));
  Serial.println("Enter new fixedTime2 values as integers with space between (e.g., '500 500'); max 2,147,483,647.  Default is 0 0");
  
  getLine();
  char* f0 = strtok(inputLine,delim);
  char* f1 = strtok(NULL,delim);
    
  uint32_t ft0 = strtol(f0,NULL,10);
  uint32_t ft1 = strtol(f1,NULL,10);
  
  pConfigInfo->FIXED_TIME2[0] = ft0;
  pConfigInfo->FIXED_TIME2[1] = ft1;
   
}

void fudge0(struct config_t *pConfigInfo)
{
  char delim[] = " ";
  Serial.print("fudge0:");Serial.print(int32_t(pConfigInfo->FUDGE0[0]));
  Serial.print("/");Serial.println(int32_t(pConfigInfo->FUDGE0[1]));
  Serial.println("Enter new fudge0 values as integers with space between (e.g., '500 500'); max 2,147,483,647.  Default is 0 0");
  
  getLine();
  char* f0 = strtok(inputLine,delim);
  char* f1 = strtok(NULL,delim);
    
  uint32_t ft0 = strtol(f0,NULL,10);
  uint32_t ft1 = strtol(f1,NULL,10);
  
  pConfigInfo->FUDGE0[0] = ft0;
  pConfigInfo->FUDGE0[1] = ft1;
}

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

void initializeConfig(struct config_t *x)
{
  *x = defaultConfig();
}

void doSetupMenu(struct config_t *pConfigInfo)      // line-oriented, robust serial menu
{
  char buf[96];
  bool showMenu = true;
  for (;;) {
    if (showMenu) {
      serialPrintImmediate("\r\n== TICC Configuration ==\r\n");
      // Mode
      serialPrintImmediate("T/P/I/L/D/N  - Set Mode (currently: ");
      switch (pConfigInfo->MODE) {
        case Timestamp: serialPrintImmediate("Timestamp"); break;
        case Period:    serialPrintImmediate("Period"); break;
        case Interval:  serialPrintImmediate("Interval A->B"); break;
        case timeLab:   serialPrintImmediate("TimeLab 3-ch"); break;
        case Debug:     serialPrintImmediate("Debug"); break;
        case Null:      serialPrintImmediate("Null"); break;
      }
      serialPrintImmediate(")\r\n");
      // Poll char
      serialPrintImmediate("O            - Poll Character (currently: ");
      if (pConfigInfo->POLL_CHAR) {
        char ch[8]; ch[0] = pConfigInfo->POLL_CHAR; ch[1] = 0; serialPrintImmediate(ch);
      } else {
        serialPrintImmediate("none");
      }
      serialPrintImmediate(")\r\n");
      // Clock speed MHz
      {
        char tmp[48];
        int32_t mhz   = (int32_t)(pConfigInfo->CLOCK_HZ / 1000000LL);
        int32_t frac6 = (int32_t)(pConfigInfo->CLOCK_HZ % 1000000LL);
        sprintf(tmp, "H            - Clock Speed MHz (currently: %ld.%06ld)\r\n", mhz, frac6);
        serialPrintImmediate(tmp);
      }
      // Coarse tick us
      {
        char tmp[48];
        int32_t us    = (int32_t)(pConfigInfo->PICTICK_PS / 1000000LL);
        int32_t frac6 = (int32_t)(pConfigInfo->PICTICK_PS % 1000000LL);
        sprintf(tmp, "U            - Coarse Tick us (currently: %ld.%06ld)\r\n", us, frac6);
        serialPrintImmediate(tmp);
      }
      // Wrap digits
      {
        char tmp[48]; sprintf(tmp, "R            - Timestamp Wrap digits (currently: %d)\r\n", (int)pConfigInfo->WRAP);
        serialPrintImmediate(tmp);
      }
      // Sync mode
      {
        char tmp[48]; sprintf(tmp, "S            - Master/Client (currently: %c)\r\n", pConfigInfo->SYNC_MODE);
        serialPrintImmediate(tmp);
      }
      // Channel names
      {
        char tmp[48]; sprintf(tmp, "N            - Channel Names (currently: %c/%c)\r\n", pConfigInfo->NAME[0], pConfigInfo->NAME[1]);
        serialPrintImmediate(tmp);
      }
      // Prop delays
      {
        char tmp[64]; sprintf(tmp, "G            - Propagation Delay ps A/B (currently: %ld/%ld)\r\n", (int32_t)pConfigInfo->PROP_DELAY[0], (int32_t)pConfigInfo->PROP_DELAY[1]);
        serialPrintImmediate(tmp);
      }
      // Trigger edges
      {
        char tmp[64]; sprintf(tmp, "E            - Trigger Edge A/B (currently: %c/%c)\r\n", pConfigInfo->START_EDGE[0], pConfigInfo->START_EDGE[1]);
        serialPrintImmediate(tmp);
      }
      // Time dilation
      {
        char tmp[64]; sprintf(tmp, "D            - Time Dilation A/B (currently: %ld/%ld)\r\n", (int32_t)pConfigInfo->TIME_DILATION[0], (int32_t)pConfigInfo->TIME_DILATION[1]);
        serialPrintImmediate(tmp);
      }
      // fixedTime2
      {
        char tmp[64]; sprintf(tmp, "F            - fixedTime2 ps A/B (currently: %ld/%ld)\r\n", (int32_t)pConfigInfo->FIXED_TIME2[0], (int32_t)pConfigInfo->FIXED_TIME2[1]);
        serialPrintImmediate(tmp);
      }
      // FUDGE0
      {
        char tmp[64]; sprintf(tmp, "Z            - FUDGE0 ps A/B (currently: %ld/%ld)\r\n", (int32_t)pConfigInfo->FUDGE0[0], (int32_t)pConfigInfo->FUDGE0[1]);
        serialPrintImmediate(tmp);
      }
      serialPrintImmediate("1            - Discard changes and exit\r\n");
      serialPrintImmediate("2            - Write changes to EEPROM and restart\r\n");
      serialPrintImmediate("3            - Reset all to defaults and restart\r\n");
      showMenu = false;
    }
    Serial.print("> ");
    Serial.flush();
    // Force immediate rendering on some terminals by emitting a space then backspace
    serialWriteImmediate(' ');
    serialWriteImmediate('\b');
    serialDrain();

    size_t n = readLine(buf, sizeof(buf));
    char *line = trimInPlace(buf);
    if (n == 0) continue;
    char cmd = toupper(line[0]);
    const char *args = line + 1; while (*args == ' ') args++;

    // Mode quick-set letters for now (will move to submenu later)
    if (strchr("TPILD", cmd)) {
      MeasureMode old = pConfigInfo->MODE;
      switch (cmd) {
        case 'T': pConfigInfo->MODE = Timestamp; break;
        case 'P': pConfigInfo->MODE = Period; break;
        case 'I': pConfigInfo->MODE = Interval; break;
        case 'L': pConfigInfo->MODE = timeLab; break;
        case 'D': pConfigInfo->MODE = Debug; break;
      }
      serialPrintImmediate("OK -- Mode changed\r\n");
      Serial.flush();
      continue;
    }
    if (cmd == '?' || cmd == 'M') { showMenu = true; serialPrintImmediate("\r\n"); continue; }

    if (cmd == 'N') { // Channel names; allow inline or prompted entry
      char old0 = pConfigInfo->NAME[0], old1 = pConfigInfo->NAME[1];
      if (*args) {
        const char *slash = strchr(args, '/');
        if (slash && slash != args && slash[1]) {
          pConfigInfo->NAME[0] = args[0];
          pConfigInfo->NAME[1] = slash[1];
          char msg[64]; sprintf(msg, "OK -- Channel Names %c/%c -> %c/%c\r\n", old0, old1, pConfigInfo->NAME[0], pConfigInfo->NAME[1]); serialPrintImmediate(msg);
        } else {
          serialPrintImmediate("Invalid\r\n");
        }
      } else {
        serialPrintImmediate("Enter names A/B: "); n = readLine(buf, sizeof(buf)); char *ln = trimInPlace(buf);
        const char *slash = strchr(ln, '/');
        if (slash && slash != ln && slash[1]) { pConfigInfo->NAME[0] = ln[0]; pConfigInfo->NAME[1] = slash[1]; char msg[64]; sprintf(msg, "OK -- Channel Names %c/%c -> %c/%c\r\n", old0, old1, pConfigInfo->NAME[0], pConfigInfo->NAME[1]); serialPrintImmediate(msg); }
         else { serialPrintImmediate("Invalid\r\n"); }
      }
      Serial.flush();
      continue;
    }

    if (cmd == 'O') {
      char old = pConfigInfo->POLL_CHAR;
      serialPrintImmediate("Enter poll character (space to clear): ");
      n = readLine(buf, sizeof(buf)); line = trimInPlace(buf);
      pConfigInfo->POLL_CHAR = (line[0] == '\0' || line[0] == ' ') ? 0x00 : line[0];
      char msg[64]; if (old) sprintf(msg, "OK -- Poll Char %c -> %c\r\n", old, pConfigInfo->POLL_CHAR ? pConfigInfo->POLL_CHAR : ' '); else sprintf(msg, "OK -- Poll Char none -> %c\r\n", pConfigInfo->POLL_CHAR ? pConfigInfo->POLL_CHAR : ' '); serialPrintImmediate(msg);
      Serial.flush();
      continue;
    }

    if (cmd == 'H') { // Clock speed MHz -> Hz
      serialPrintImmediate("Clock MHz: "); n = readLine(buf, sizeof(buf)); line = trimInPlace(buf);
      int64_t hz; if (parseDecimalScaled(line, 1000000LL, &hz) && hz > 0) { int64_t old=pConfigInfo->CLOCK_HZ; pConfigInfo->CLOCK_HZ = hz; char m[64]; sprintf(m, "OK -- Clock %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(hz/1000000LL),(int32_t)(hz%1000000LL)); serialPrintImmediate(m); } else serialPrintImmediate("Invalid\r\n");
      Serial.flush();
      continue;
    }
    if (cmd == 'U') { // Coarse tick us -> ps
      serialPrintImmediate("Coarse tick (us): "); n = readLine(buf, sizeof(buf)); line = trimInPlace(buf);
      int64_t ps; if (parseDecimalScaled(line, 1000000LL, &ps) && ps > 0) { int64_t old=pConfigInfo->PICTICK_PS; pConfigInfo->PICTICK_PS = ps; char m[64]; sprintf(m, "OK -- Coarse %ld.%06ld -> %ld.%06ld\r\n", (int32_t)(old/1000000LL),(int32_t)(old%1000000LL),(int32_t)(ps/1000000LL),(int32_t)(ps%1000000LL)); serialPrintImmediate(m); } else serialPrintImmediate("Invalid\r\n");
      Serial.flush();
      continue;
    }
    if (cmd == 'R') { // wrap digits
      serialPrintImmediate("Wrap digits (0..10): "); n = readLine(buf, sizeof(buf)); line = trimInPlace(buf);
      int64_t wrap; if (parseInt64Simple(line, &wrap) && wrap >= 0 && wrap <= 10) { int16_t old=pConfigInfo->WRAP; pConfigInfo->WRAP = (int16_t)wrap; char m[64]; sprintf(m, "OK -- Wrap %d -> %d\r\n", (int)old, (int)pConfigInfo->WRAP); serialPrintImmediate(m); } else serialPrintImmediate("Invalid\r\n");
      Serial.flush();
      continue;
    }
    if (cmd == 'S') { // sync mode
      serialPrintImmediate("Enter M or C: "); n = readLine(buf, sizeof(buf)); line = trimInPlace(buf);
      char v = toupper(line[0]); if (v == 'M' || v == 'C') { char old = pConfigInfo->SYNC_MODE; pConfigInfo->SYNC_MODE = v; char m[48]; sprintf(m, "OK -- Sync %c -> %c\r\n", old, v); serialPrintImmediate(m); } else serialPrintImmediate("Invalid\r\n");
      Serial.flush();
      continue;
    }
    if (cmd == 'G' || cmd == 'D' || cmd == 'F' || cmd == 'Z' || cmd == 'E') {
      // Pair handlers
      serialPrintImmediate("Enter pair A/B: "); n = readLine(buf, sizeof(buf)); line = trimInPlace(buf);
      if (cmd == 'E') {
        // edges
        const char *slash = strchr(line, '/');
        char e0 = (slash == NULL) ? toupper(line[0]) : (slash == line ? '\0' : toupper(line[0]));
        char e1 = (slash == NULL) ? toupper(line[0]) : (slash[1] ? toupper(slash[1]) : '\0');
        char old0=pConfigInfo->START_EDGE[0], old1=pConfigInfo->START_EDGE[1];
        if (e0 == 'R' || e0 == 'F') pConfigInfo->START_EDGE[0] = e0;
        if (e1 == 'R' || e1 == 'F') pConfigInfo->START_EDGE[1] = e1;
        char m[64]; sprintf(m, "OK -- Edges %c/%c -> %c/%c\r\n", old0, old1, pConfigInfo->START_EDGE[0], pConfigInfo->START_EDGE[1]); serialPrintImmediate(m);
        Serial.flush();
        continue;
      }
      bool s0=false, s1=false; int64_t v0=0, v1=0;
      if (!parseInt64Pair(line, &s0, &v0, &s1, &v1)) { serialPrintImmediate("Invalid\r\n"); Serial.flush(); continue; }
      if (cmd == 'G') { int32_t o0=pConfigInfo->PROP_DELAY[0], o1=pConfigInfo->PROP_DELAY[1]; if (s0) pConfigInfo->PROP_DELAY[0]=v0; if (s1) pConfigInfo->PROP_DELAY[1]=v1; char m[80]; sprintf(m, "OK -- PropDelay %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->PROP_DELAY[0],(long)pConfigInfo->PROP_DELAY[1]); serialPrintImmediate(m); Serial.flush(); continue; }
      if (cmd == 'D') { int32_t o0=pConfigInfo->TIME_DILATION[0], o1=pConfigInfo->TIME_DILATION[1]; if (s0) pConfigInfo->TIME_DILATION[0]=v0; if (s1) pConfigInfo->TIME_DILATION[1]=v1; char m[80]; sprintf(m, "OK -- TimeDilation %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->TIME_DILATION[0],(long)pConfigInfo->TIME_DILATION[1]); serialPrintImmediate(m); Serial.flush(); continue; }
      if (cmd == 'F') { int32_t o0=pConfigInfo->FIXED_TIME2[0], o1=pConfigInfo->FIXED_TIME2[1]; if (s0) pConfigInfo->FIXED_TIME2[0]=v0; if (s1) pConfigInfo->FIXED_TIME2[1]=v1; char m[80]; sprintf(m, "OK -- fixedTime2 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FIXED_TIME2[0],(long)pConfigInfo->FIXED_TIME2[1]); serialPrintImmediate(m); Serial.flush(); continue; }
      if (cmd == 'Z') { int32_t o0=pConfigInfo->FUDGE0[0], o1=pConfigInfo->FUDGE0[1]; if (s0) pConfigInfo->FUDGE0[0]=v0; if (s1) pConfigInfo->FUDGE0[1]=v1; char m[80]; sprintf(m, "OK -- FUDGE0 %ld/%ld -> %ld/%ld\r\n", (long)o0,(long)o1,(long)pConfigInfo->FUDGE0[0],(long)pConfigInfo->FUDGE0[1]); serialPrintImmediate(m); Serial.flush(); continue; }
      continue;
    }

    // Numbered exits: 1 discard, 2 write+restart, 3 defaults+restart
    if (cmd == '1') { // discard
      serialPrintImmediate("Discarded changes.\r\n");
      return;
    }
    if (cmd == '2') { // write and restart (caller will reinit)
      EEPROM_writeAnything(CONFIG_START, *pConfigInfo);
      serialPrintImmediate("Saved. Restarting...\r\n");
      request_restart = 1;
      return;
    }
    if (cmd == '3') { // defaults and restart
      eeprom_write_config_default(CONFIG_START);
      serialPrintImmediate("Defaults written. Restarting...\r\n");
      request_restart = 1;
      return;
    }
    serialPrintImmediate("? Unknown command\r\n");
    Serial.flush();
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
