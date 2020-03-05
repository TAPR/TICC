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
  // The final ts_wrap is an int64 value of 100 us ticks, and the default is (2^63 - 1).
  // Input is in seconds to make life easier for the user. 
  int64_t secs;
  Serial.print("Timestamp Wraparound"), Serial.print(((int32_t)pConfigInfo->WRAP) / 10000), Serial.println(" seconds.  Default is bignum");
  Serial.println("Enter new value (in seconds; 0 for default) and press Enter (or just Enter for no change)");
  getLine();
  inputLineReadIndex = 0;
  getInt64(&secs, 1);
  if (secs == 0) {
    secs = DEFAULT_WRAP;
    pConfigInfo->WRAP = secs;
  } else {
    pConfigInfo->WRAP = secs * 1e4;
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
  char* e0 = toupper(strtok(inputLine,delim)[1]);
  char* e1 = toupper(strtok(NULL,delim)[0]);

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

void doSetupMenu(struct config_t *pConfigInfo)      // also display the default values ----------------
{
  char response;
  for ( ; ; )
  {
  Serial.println(), Serial.println();
  Serial.print("A   measurement mode (default T)                "); Serial.println( modeToChar(pConfigInfo->MODE));  // enum MeasureMode, default Timestamp
  Serial.print("B   poll character (default unset)              "); 
          if (pConfigInfo->POLL_CHAR) {
            Serial.println(pConfigInfo->POLL_CHAR); // normally unset
          } else {
            Serial.println("unset");
          }
  Serial.print("C   clock speed in MHz (default 10)             "); printHzAsMHz(pConfigInfo->CLOCK_HZ), Serial.println();       // int_64

  Serial.print("D   coarse clock rate in us) (default 100)      "); printHzAsMHz(pConfigInfo->PICTICK_PS), Serial.println();   // int_64
  
  Serial.print("E   calibration periods (default 20)            "); Serial.println((int32_t)pConfigInfo->CAL_PERIODS);  // int_16, choices are 2, 10, 20, 40
  
  Serial.print("F   timeout (default 0x05)                      ");       // int16 
    char str[8];sprintf(str, "0x%02X", (int32_t)pConfigInfo->TIMEOUT);Serial.println(str);

  Serial.print("G   ts wrap (default bignum)                    "); print_int64(pConfigInfo->WRAP / 10000); Serial.println(); // int64_t ; will be 1e4 to low if left default
  
  Serial.print("H   sync:  master / client (default M)           "); Serial.print(pConfigInfo->SYNC_MODE); Serial.println();  // M (default) or S
  
  Serial.print("I   channel name (default A/B)                  ");  
    Serial.print(pConfigInfo->NAME[0]);Serial.print('/');Serial.println(pConfigInfo->NAME[1]);        
  
  Serial.print("J   prop delay (default 0)                      ");       // int_64, default 0
    Serial.print((int32_t)pConfigInfo->PROP_DELAY[0]);Serial.print(' ');Serial.println((int32_t)pConfigInfo->PROP_DELAY[1]);
    	
  Serial.print("K   trigger edge (default R R)                  ");     // R(ising) or F(alling)
    Serial.print(pConfigInfo->START_EDGE[0]);Serial.print(' ');Serial.println(pConfigInfo->START_EDGE[1]);
          
  Serial.print("L   time dilation (default 2500)                ");       // int_64, default 2500
    Serial.print((int32_t)pConfigInfo->TIME_DILATION[0]);Serial.print(' ');Serial.println((int32_t)pConfigInfo->TIME_DILATION[1]);
    	  
  Serial.print("M   fixed time2 (default 0)                     ");   // int_64, default 0
    Serial.print((int32_t)pConfigInfo->FIXED_TIME2[0]);Serial.print(' ');Serial.println((int32_t)pConfigInfo->FIXED_TIME2[1]);
     
  Serial.print("N   fudge0 (default 0)                          ");   // int_64, default 0
    Serial.print((int32_t)pConfigInfo->FUDGE0[0]);Serial.print(' ');Serial.println((int32_t)pConfigInfo->FUDGE0[1]);
    	
  Serial.println();
  Serial.println("1   Reset all to default values");
  Serial.println("2   Write changes");
  Serial.println("3   Discard changes and exit setup");
  Serial.println("choose one: ");
    
  response = toupper(getChar());    // wait for a character
  Serial.println();
  
    switch(response)   
    {
      case 'A':  measurementMode(pConfigInfo);
    		break;
      case 'B':  pollChar(pConfigInfo);
        break;
      case 'C':  clockSpeed(pConfigInfo);
    		break;
      case 'D':	 coarseClockRate(pConfigInfo);
    		break;
      case 'E':  calibrationPeriods(pConfigInfo);
    		break;
      case 'F':  timeout(pConfigInfo);
    		break;
      case 'G':  ts_wrap(pConfigInfo);
        break;
      case 'H':  masterClient(pConfigInfo); // (sync mode)
    		break;
      case 'I':  channel_name(pConfigInfo);
    		break;
      case 'J':  prop_delay(pConfigInfo);
        break;
      case 'K':  triggerEdge(pConfigInfo);
    		break;
      case 'L':  timeDilation(pConfigInfo);
    		break;
      case 'M':  fixedTime2(pConfigInfo);
    		break;
      case 'N':  fudge0(pConfigInfo);
    		break;
      case '1': initializeConfig(pConfigInfo);
        break;
      case '2':  // write changes and exit
        Serial.println("Writing changes to eeprom...");
        EEPROM_writeAnything(CONFIG_START, *pConfigInfo); // save change to config
        Serial.println("Finished.  Please Restart!");
        return;
    	  break; 
      case '3':	// discard changes and exit
        return;
    	  break;
      
      // this doesn't show up in the menu -- reset entire eeprom to 0xFF (factory
      // default).  Restart the board to write new serial number and defaults.
      case '4':
        Serial.println("");
        Serial.println("Setting EEPROM to factory status.  Stand by...");
        eeprom_clear();
        Serial.println("Finished.");
        return;
        break;      
      default:  Serial.println("???");  // 'bad selection'
    		break;
    }   // switch
  }   // for 
}


void UserConfig(struct config_t *pConfigInfo) 
{
    char c;
    while ( ! Serial )   /* wait until Serial port is open */ ;

    Serial.println("# Type any character for config menu");  // leading hash so logging programs can ignore
    Serial.print("# ");
    bool configRequested = 0;
    for (int i = 6; i >= 0; --i)  // wait ~6 sec so user can type something
    { 
      delay(250);   Serial.print('.');      if (Serial.available())       { configRequested = 1;        break;      }
      delay(250);   Serial.print('.');      if (Serial.available())       { configRequested = 1;        break;      }
      delay(250);   Serial.print('.');      if (Serial.available())       { configRequested = 1;        break;      }
      delay(250);   Serial.print('.');      if (Serial.available())       { configRequested = 1;        break;      }
    }
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
    case Debug:
      Serial.println("Debug");
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
  Serial.print("# Timestamp Wrap:  ");print_int64(x.WRAP  / 10000);Serial.println(" seconds"); // NOTE: this will be 1e4 to low if WRAP is left to default
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
